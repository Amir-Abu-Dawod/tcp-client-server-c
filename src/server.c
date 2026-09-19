#include <stdint.h>
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

const int TIME_PORT = 27015;
const char *HTTP_SERVER_ADDRESS = "httpbin.org";
const int HTTP_SERVER_PORT = 80;

/* Function to calculate Round-Trip Time (RTT)*/
double measureRTT(clock_t start, clock_t end)
{
    return ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
}

char *readFile(const char *filename)
{
    FILE *file = fopen(filename, "rb");

    if (file == NULL)
    {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        printf("Error seeking file %s\n", filename);
        fclose(file);
        return NULL;
    }

    long fileSize = ftell(file);

    if (fileSize < 0)
    {
        printf("Error determining size of file %s\n", filename);
        fclose(file);
        return NULL;
    }

    rewind(file);

    char *buffer = malloc((size_t)fileSize + 1);

    if (buffer == NULL)
    {
        printf("Memory allocation error\n");
        fclose(file);
        return NULL;
    }

    size_t bytesRead = fread(
        buffer,
        1,
        (size_t)fileSize,
        file);

    if (bytesRead != (size_t)fileSize)
    {
        printf("Error reading file %s\n", filename);
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[bytesRead] = '\0';

    fclose(file);

    return buffer;
}

bool sendAll(SOCKET socket, const char *data, int length)
{
    int totalSent = 0;

    while (totalSent < length)
    {
        int bytesSent = send(
            socket,
            data + totalSent,
            length - totalSent,
            0
        );

        if (bytesSent == SOCKET_ERROR)
        {
            printf(
                "Error sending data: %d\n",
                WSAGetLastError()
            );
            return false;
        }

        if (bytesSent == 0)
        {
            printf("Socket closed while sending data.\n");
            return false;
        }

        totalSent += bytesSent;
    }

    return true;
}

bool sendFramedResponse(
    SOCKET socket,
    const char *payload,
    int payloadLength
)
{
    if (payload == NULL || payloadLength < 0)
    {
        return false;
    }

    uint32_t networkLength =
        htonl((uint32_t)payloadLength);

        //send first 4-bytes response  
    if (!sendAll(
            socket,
            (const char *)&networkLength,
            (int)sizeof(networkLength)))
    {
        return false;
    }

    if (payloadLength == 0)
    {
        return true;
    }

    return sendAll(
        socket,
        payload,
        payloadLength
    );
}

/* Function to send HTTP request to the main server */
bool sendHttpRequest(const char *request, char *responseBuffer, int bufferSize)
{
    if (responseBuffer == NULL || bufferSize <= 1)
    {
        return false;
    }

    /* Create a socket to communicate with the main server*/
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET)
    {
        printf("Error creating server socket.\n");
        return false;
    }

    /*Resolve the server's IP address*/
    struct hostent *remoteHost = gethostbyname(HTTP_SERVER_ADDRESS);
    if (remoteHost == NULL)
    {
        printf("Error resolving server address.\n");
        closesocket(serverSocket);
        return false;
    }

    /* Create a sockaddr_in structure for connecting to the server*/
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = *((unsigned long *)remoteHost->h_addr);
    serverAddr.sin_port = htons(HTTP_SERVER_PORT);

    /*Connect to the main server*/
    if (connect(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        printf("Error connecting to server.\n");
        closesocket(serverSocket);
        return false;
    }

    /* Send the request to the main server*/
    if (!sendAll(serverSocket, request, (int)strlen(request)))
    {
        printf("Error sending request to server.\n");
        closesocket(serverSocket);
        return false;
    }

    /* Receive the response from the main server*/
    int totalBytesReceived = 0;

    while (totalBytesReceived < bufferSize - 1) {
        int remainingSpace =
            bufferSize - totalBytesReceived - 1;

        int bytesReceived =
            recv(
                serverSocket,
                responseBuffer + totalBytesReceived,
                remainingSpace,
                0
            );

        if (bytesReceived > 0) {
            totalBytesReceived += bytesReceived;
            continue;
        }

        if (bytesReceived == 0) {
            break;
        }

        printf(
            "Error receiving HTTP response: %d\n",
            WSAGetLastError()
        );

        closesocket(serverSocket);
        return false;
    }

    /*
     * If the buffer became completely full, check whether
     * additional response data still exists.
     */
    if (totalBytesReceived == bufferSize - 1) {
        char extraByte;

        int extraBytes =
            recv(serverSocket, &extraByte, 1, 0);

        if (extraBytes > 0) {
            printf(
                "Response buffer too small to hold entire response.\n"
            );

            closesocket(serverSocket);
            return false;
        }

        if (extraBytes == SOCKET_ERROR) {
            printf(
                "Error receiving HTTP response: %d\n",
                WSAGetLastError()
            );

            closesocket(serverSocket);
            return false;
        }
    }

    responseBuffer[totalBytesReceived] = '\0';

    closesocket(serverSocket);

    return true;
}

bool checkForAnError(int bytesResult, const char *errorAt, SOCKET socket_1, SOCKET socket_2)
{
    if (SOCKET_ERROR == bytesResult)
    {
        printf("Time Server: Error at %s(): %d\n", errorAt, WSAGetLastError());
        closesocket(socket_1);
        closesocket(socket_2);
        WSACleanup();
        return true;
    }
    return false;
}

int main(void)
{

    WSADATA wsaData;
    SOCKET listenSocket;
    struct sockaddr_in serverService;

    if (NO_ERROR != WSAStartup(MAKEWORD(2, 0), &wsaData))
    {
        printf("Time Server: Error at WSAStartup()\n");
        return EXIT_FAILURE;
    }

    listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (INVALID_SOCKET == listenSocket)
    {
        printf("Time Server: Error at socket(): ");
        printf("%d", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    memset(&serverService, 0, sizeof(serverService));

    serverService.sin_family = AF_INET;

    serverService.sin_addr.s_addr = htonl(INADDR_ANY);

    serverService.sin_port = htons(TIME_PORT);

    if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR *)&serverService, sizeof(serverService)))
    {
        printf("Time Server: Error at bind(): ");
        printf("%d", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (SOCKET_ERROR == listen(listenSocket, 5))
    {
        printf("Time Server: Error at listen(): ");
        printf("%d", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    while (1)
    {

        struct sockaddr_in from;
        int fromLen = sizeof(from);

        printf("Time Server: Wait for clients' requests.\n");

        SOCKET msgSocket = accept(listenSocket, (struct sockaddr *)&from, &fromLen);
        if (INVALID_SOCKET == msgSocket)
        {
            printf("Time Server: Error at accept(): ");
            printf("%d", WSAGetLastError());
            closesocket(listenSocket);
            WSACleanup();
            return EXIT_FAILURE;
        }

        printf("Time Server: Client is connected.\n");
        while (1)
        {
            int bytesRecv = 0;
            char recvBuff[255];

            bytesRecv = recv(msgSocket, recvBuff, (int)sizeof(recvBuff) - 1, 0);
            if (checkForAnError(bytesRecv, "recv", listenSocket, msgSocket))
            {
                return EXIT_FAILURE;
            }

            if (bytesRecv == 0)
            {
                printf("Time Server: Client disconnected.\n");
                closesocket(msgSocket);
                break;
            }

            // No Socket Error, and no 0 bytes received
            recvBuff[bytesRecv] = '\0';

            // Option 1
            if (strcmp(recvBuff, "anything") == 0)
            {
                char *fileContent = readFile("anything.txt");
                if (fileContent != NULL)
                {
                    /*Send the received content to the client*/
                    if (!sendFramedResponse(
                            msgSocket,
                            fileContent,
                            (int)strlen(fileContent)))
                    {
                        printf("Failed to send response to client.\n");
                        closesocket(msgSocket);
                        closesocket(listenSocket);
                        WSACleanup();
                        free(fileContent);
                        return EXIT_FAILURE;
                    }

                }
                else
                {
                    printf("File 'anything.txt' not found locally. Making HTTP request...\n");

                    /* Send HTTP request to main server */
                    char responseBuffer[5000];
                    if (sendHttpRequest("GET /anything HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n",
                                        responseBuffer, sizeof(responseBuffer)))
                    {
                        /*Save the received content to a new file*/
                        FILE *newFile = fopen("anything.txt", "wb");
                        size_t responseLength = strlen(responseBuffer);

                        if (newFile != NULL)
                        {
                            if (fwrite(responseBuffer, 1, responseLength, newFile) != responseLength)
                            {
                                printf("Error writing to file 'anything.txt'.\n");
                            }
                            else
                            {
                                printf("Content saved to 'anything.txt'.\n");
                            }
                            fclose(newFile);
                        }
                        else
                        {
                            printf("Error creating 'anything.txt'.\n");
                        }

                        /*Send the received content to the client*/
                        if (!sendFramedResponse(
                                msgSocket,
                                responseBuffer,
                                (int)strlen(responseBuffer)))
                        {
                            printf("Failed to send response to client.\n");
                            closesocket(msgSocket);
                            closesocket(listenSocket);
                            WSACleanup();
                            return EXIT_FAILURE;
                        }
                    }
                    else
                    {
                        const char *errorMessage = "ERROR: Failed to retrieve anything resource.";

                        /*Send error message to the client*/
                        if (!sendFramedResponse(
                                msgSocket,
                                errorMessage,
                                (int)strlen(errorMessage)))
                        {
                            printf("Failed to send response to client.\n");
                            closesocket(msgSocket);
                            closesocket(listenSocket);
                            WSACleanup();
                            return EXIT_FAILURE;
                        }
                    }
                }
            }
            
            // Option 2
            else if (strcmp(recvBuff, "json") == 0)
            { /* Second Request */
                char *fileContent = readFile("json.txt");
                if (fileContent != NULL)
                {
                    /* Send the received content to the client*/
                    if (!sendFramedResponse(
                            msgSocket,
                            fileContent,
                            (int)strlen(fileContent)))
                    {
                        printf("Failed to send response to client.\n");
                        closesocket(msgSocket);
                        closesocket(listenSocket);
                        WSACleanup();
                        return EXIT_FAILURE;
                    }
                }
                else
                {
                    printf("File 'json.txt' not found locally. Making HTTP request...\n");

                    /* Send HTTP request to main server */
                    char responseBuffer[5000];
                    if (sendHttpRequest("GET /json HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n",
                                        responseBuffer, sizeof(responseBuffer)))
                    {
                        /*Save the received content to a new file*/
                        FILE *newFile = fopen("json.txt", "wb");
                        size_t responseLength = strlen(responseBuffer);

                        if (newFile != NULL)
                        {
                            if (fwrite(responseBuffer, 1, responseLength, newFile) != responseLength)
                            {
                                printf("Error writing to file 'json.txt'.\n");
                            }
                            else
                            {
                                printf("Content saved to 'json.txt'.\n");
                            }
                            fclose(newFile);
                        }
                        else
                        {
                            printf("Error creating 'json.txt'.\n");
                        }

                        /* Send the received content to the client*/
                        if (!sendFramedResponse(
                                msgSocket,
                                responseBuffer,
                                (int)strlen(responseBuffer)))
                        {
                            printf("Failed to send response to client.\n");
                            closesocket(msgSocket);
                            closesocket(listenSocket);
                            WSACleanup();
                            return EXIT_FAILURE;
                        }
                    }
                    else
                    {
                        const char *errorMessage = "ERROR: Failed to retrieve JSON resource.";

                        /*Send error message to the client*/
                        if (!sendFramedResponse(
                                msgSocket,
                                errorMessage,
                                (int)strlen(errorMessage)))
                        {
                            printf("Failed to send response to client.\n");
                            closesocket(msgSocket);
                            closesocket(listenSocket);
                            WSACleanup();
                            return EXIT_FAILURE;
                        }
                    }
                }
            }

            // Option 3
            else if (strcmp(recvBuff, "RTT") == 0)
            { /* Third Request */
                clock_t startTime = clock();

                /* specific task related to RTT measurement */
                volatile double temp = 0.0;

                for (int i = 0; i < 1000000; ++i) {
                    temp = sqrt((double)i);
                }

                (void)temp;

                clock_t endTime = clock();

                double rtt = measureRTT(startTime, endTime); /* Calculate RTT */

                printf("RTT for request \"RTT\": %.2f ms\n", rtt);

                /*Convert double value rtt to string*/
                char rttStr[20]; /* Assuming a maximum of 20 characters for the string representation*/
                snprintf(rttStr, sizeof(rttStr), "%.2f", rtt);
                strcat(rttStr, " ms");

                /* Send the string back to the client*/
                if (!sendFramedResponse(
                        msgSocket,
                        rttStr,
                        (int)strlen(rttStr)))
                {
                    printf("Failed to send RTT response to client.\n");
                    closesocket(msgSocket);
                    closesocket(listenSocket);
                    WSACleanup();
                    return EXIT_FAILURE;
                }
            }

            else
            { /* Closing Socket */
                printf("Time Server: Closing Connection.\n");
                closesocket(msgSocket);
                break;
            }
        }
    }
    
    closesocket(listenSocket);
    WSACleanup();

    return EXIT_SUCCESS;
}
