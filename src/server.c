#include <stdint.h>
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

const int SERVER_PORT = 27015;
const char *HTTP_SERVER_ADDRESS = "httpbin.org";
const int HTTP_SERVER_PORT = 80;


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
            0);

        if (bytesSent == SOCKET_ERROR)
        {
            printf(
                "Error sending data: %d\n",
                WSAGetLastError());
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
    int payloadLength)
{
    if (payload == NULL || payloadLength < 0)
    {
        return false;
    }

    uint32_t networkLength =
        htonl((uint32_t)payloadLength);

    // send first 4-bytes response
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
        payloadLength);
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

    while (totalBytesReceived < bufferSize - 1)
    {
        int remainingSpace =
            bufferSize - totalBytesReceived - 1;

        int bytesReceived =
            recv(
                serverSocket,
                responseBuffer + totalBytesReceived,
                remainingSpace,
                0);

        if (bytesReceived > 0)
        {
            totalBytesReceived += bytesReceived;
            continue;
        }

        if (bytesReceived == 0)
        {
            break;
        }

        printf(
            "Error receiving HTTP response: %d\n",
            WSAGetLastError());

        closesocket(serverSocket);
        return false;
    }

    /*
     * If the buffer became completely full, check whether
     * additional response data still exists.
     */
    if (totalBytesReceived == bufferSize - 1)
    {
        char extraByte;

        int extraBytes =
            recv(serverSocket, &extraByte, 1, 0);

        if (extraBytes > 0)
        {
            printf(
                "Response buffer too small to hold entire response.\n");

            closesocket(serverSocket);
            return false;
        }

        if (extraBytes == SOCKET_ERROR)
        {
            printf(
                "Error receiving HTTP response: %d\n",
                WSAGetLastError());

            closesocket(serverSocket);
            return false;
        }
    }

    responseBuffer[totalBytesReceived] = '\0';

    closesocket(serverSocket);

    return true;
}

bool handleResourceRequest(
    SOCKET clientSocket,
    const char *cacheFilename,
    const char *httpRequest,
    const char *errorMessage)
{
    char *fileContent = readFile(cacheFilename);

    /*
     * Cache hit:
     * send the locally stored response.
     */
    if (fileContent != NULL)
    {
        bool sendSucceeded = sendFramedResponse(
            clientSocket,
            fileContent,
            (int)strlen(fileContent));

        free(fileContent);

        if (!sendSucceeded)
        {
            printf("Failed to send cached response to client.\n");
            return false;
        }

        return true;
    }

    /*
     * Cache miss:
     * retrieve the resource from the HTTP server.
     */
    printf(
        "File '%s' not found locally. Making HTTP request...\n",
        cacheFilename);

    char responseBuffer[5000];

    if (!sendHttpRequest(
            httpRequest,
            responseBuffer,
            (int)sizeof(responseBuffer)))
    {
        /*
         * HTTP retrieval failed, but the client should still
         * receive a framed error response.
         */
        if (!sendFramedResponse(
                clientSocket,
                errorMessage,
                (int)strlen(errorMessage)))
        {
            printf("Failed to send error response to client.\n");
            return false;
        }

        return true;
    }

    /*
     * HTTP retrieval succeeded.
     * Cache the response locally.
     */
    FILE *newFile = fopen(cacheFilename, "wb");

    size_t responseLength = strlen(responseBuffer);

    if (newFile != NULL)
    {
        if (fwrite(
                responseBuffer,
                1,
                responseLength,
                newFile) != responseLength)
        {
            printf(
                "Error writing to file '%s'.\n",
                cacheFilename);
        }
        else
        {
            printf(
                "Content saved to '%s'.\n",
                cacheFilename);
        }

        fclose(newFile);
    }
    else
    {
        printf(
            "Error creating '%s'.\n",
            cacheFilename);
    }

    /*
     * Whether caching succeeded or not, we already have the
     * HTTP response and can return it to the client.
     */
    if (!sendFramedResponse(
            clientSocket,
            responseBuffer,
            (int)responseLength))
    {
        printf("Failed to send response to client.\n");
        return false;
    }

    return true;
}

bool handleRttRequest(SOCKET clientSocket)
{
    const char *response = "RTT_ACK";

    if (!sendFramedResponse(
            clientSocket,
            response,
            (int)strlen(response)))
    {
        printf("Failed to send RTT response to client.\n");
        return false;
    }

    return true;
}

bool handleClientCommand(
    SOCKET clientSocket,
    const char *command,
    bool *shouldDisconnect)
{
    if (command == NULL || shouldDisconnect == NULL)
    {
        return false;
    }

    *shouldDisconnect = false;

    // Option 1
    if (strcmp(command, "anything") == 0)
    {
        const char *httpRequest =
            "GET /anything HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n"
            "\r\n";

        const char *errorMessage =
            "ERROR: Failed to retrieve anything resource.";

        return handleResourceRequest(
            clientSocket,
            "anything.txt",
            httpRequest,
            errorMessage);
    }

    // Option 2
    if (strcmp(command, "json") == 0)
    {
        const char *httpRequest =
            "GET /json HTTP/1.1\r\n"
            "Host: httpbin.org\r\n"
            "Connection: close\r\n"
            "\r\n";

        const char *errorMessage =
            "ERROR: Failed to retrieve JSON resource.";

        return handleResourceRequest(
            clientSocket,
            "json.txt",
            httpRequest,
            errorMessage);
    }

    //Option 3
    if (strcmp(command, "RTT") == 0)
    {
        return handleRttRequest(clientSocket);
    }

    //Option 4
    if (strcmp(command, "EXIT") == 0)
    {
        printf("Server: Closing Client Connection.\n");
        *shouldDisconnect = true;
        return true;
    }

    /* send framed "unknown command" error */
    const char *errorMessage = "ERROR: Unknown command.";

    if (!sendFramedResponse(
            clientSocket,
            errorMessage,
            (int)strlen(errorMessage)))
    {
        printf("Failed to send unknown command response to client.\n");
        return false;
    }

    return true;
}

bool handleClientSession(SOCKET clientSocket)
{
    bool shouldDisconnect = false;
    while (!shouldDisconnect)
    {
        char recvBuff[255];

        int bytesRecv = recv(clientSocket, recvBuff, (int)sizeof(recvBuff) - 1, 0);
        if (bytesRecv == SOCKET_ERROR)
        {
            printf("Server: Error at recv(): %d\n", WSAGetLastError());
            return false;
        }

        if (bytesRecv == 0)
        {
            printf("Server: Client disconnected.\n");
            return false;
        }

        // No Socket Error, and no 0 bytes received
        recvBuff[bytesRecv] = '\0';

        if (!handleClientCommand(
                    clientSocket, 
                    recvBuff, 
                    &shouldDisconnect))
        {
            printf("Server: Failed to handle command: %s\n", recvBuff);
            return false;
        }
    }

    return true;
}

int main(void)
{

    WSADATA wsaData;
    SOCKET listenSocket;
    struct sockaddr_in serverService;

    if (NO_ERROR != WSAStartup(MAKEWORD(2, 0), &wsaData))
    {
        printf("Server: Error at WSAStartup()\n");
        return EXIT_FAILURE;
    }

    listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (INVALID_SOCKET == listenSocket)
    {
        printf("Server: Error at socket(): ");
        printf("%d", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    memset(&serverService, 0, sizeof(serverService));

    serverService.sin_family = AF_INET;

    serverService.sin_addr.s_addr = htonl(INADDR_ANY);

    serverService.sin_port = htons(SERVER_PORT);

    if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR *)&serverService, sizeof(serverService)))
    {
        printf("Server: Error at bind(): ");
        printf("%d", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (SOCKET_ERROR == listen(listenSocket, 5))
    {
        printf("Server: Error at listen(): ");
        printf("%d", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    while (1)
    {

        struct sockaddr_in from;
        int fromLen = sizeof(from);

        printf("Server: Wait for clients' requests.\n");

        SOCKET clientSocket = accept(listenSocket, (struct sockaddr *)&from, &fromLen);
        if (INVALID_SOCKET == clientSocket)
        {
            printf("Server: Error at accept(): ");
            printf("%d", WSAGetLastError());
            closesocket(listenSocket);
            WSACleanup();
            return EXIT_FAILURE;
        }

        printf("Server: Client is connected.\n");

        if (!handleClientSession(clientSocket))
        {
            printf("Server: Client session ended with an error.\n");
        }

        closesocket(clientSocket);
    }

    closesocket(listenSocket);
    WSACleanup();

    return EXIT_SUCCESS;
}
