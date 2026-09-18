#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include "math.h"

const int TIME_PORT = 27015;
const char* HTTP_SERVER_ADDRESS = "httpbin.org";
const int HTTP_SERVER_PORT = 80;

/* Function to calculate Round-Trip Time (RTT)*/
double measureRTT(clock_t start, clock_t end) {
    return ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
}

char *readFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        printf("Memory allocation error\n");
        fclose(file);
        return NULL;
    }

    size_t result = fread(buffer, 1, file_size, file);
    if (result != file_size) {
        printf("Error reading file %s\n", filename);
        fclose(file);
        free(buffer);
        return NULL;
    }

    buffer[file_size] = '\0';

    fclose(file);
    return buffer;
}

/* Function to send HTTP request to the main server */
bool sendHttpRequest(const char* request, char* responseBuffer, int bufferSize) {
    /* Create a socket to communicate with the main server*/
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        printf("Error creating server socket.\n");
        return false;
    }

    /*Resolve the server's IP address*/
    struct hostent *remoteHost = gethostbyname(HTTP_SERVER_ADDRESS);
    if (remoteHost == NULL) {
        printf("Error resolving server address.\n");
        closesocket(serverSocket);
        return false;
    }

    /* Create a sockaddr_in structure for connecting to the server*/
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = *((unsigned long*)remoteHost->h_addr);
    serverAddr.sin_port = htons(HTTP_SERVER_PORT);

    /*Connect to the main server*/
    if (connect(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("Error connecting to server.\n");
        closesocket(serverSocket);
        return false;
    }

    /* Send the request to the main server*/
    int bytesSent = send(serverSocket, request, strlen(request), 0);
    if (bytesSent == SOCKET_ERROR) {
        printf("Error sending request to server.\n");
        closesocket(serverSocket);
        return false;
    }

    /* Receive the response from the main server*/
    int totalBytesReceived = 0;
    int bytesReceived;
    while ((bytesReceived = recv(serverSocket, responseBuffer + totalBytesReceived, bufferSize - totalBytesReceived, 0)) > 0) {
        totalBytesReceived += bytesReceived;
    }

    /*Close the socket*/
    closesocket(serverSocket);

    /*Null-terminate the response buffer*/
    if (totalBytesReceived < bufferSize) {
        responseBuffer[totalBytesReceived] = '\0';
    } else {
        printf("Response buffer too small to hold entire response.\n");
        return false;
    }

    return true;
}


bool checkForAnError(int bytesResult, char* ErrorAt, SOCKET socket_1, SOCKET socket_2){
    if (SOCKET_ERROR == bytesResult) {
        printf("Time Server: Error at %s(): ",ErrorAt);
        printf("%d", WSAGetLastError());
        closesocket(socket_1);
        closesocket(socket_2);
        WSACleanup();
        return true;
    }
    return false;
}



void main() {

    WSADATA wsaData;
    SOCKET listenSocket;
    struct sockaddr_in serverService;
    char timeBuff[26];
    time_t timer;
    struct tm * tm_info;
    char randomletter;

    if (NO_ERROR != WSAStartup(MAKEWORD(2, 0), & wsaData)) {
        printf("Time Server: Error at WSAStartup()\n");
        return;
    }

    listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (INVALID_SOCKET == listenSocket) {
        printf("Time Server: Error at socket(): ");
        printf("%d", WSAGetLastError());
        WSACleanup();
        return;
    }

    memset( & serverService, 0, sizeof(serverService));

    serverService.sin_family = AF_INET;

    serverService.sin_addr.s_addr = htonl(INADDR_ANY);

    serverService.sin_port = htons(TIME_PORT);


    if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR * ) & serverService, sizeof(serverService))) {
        printf("Time Server: Error at bind(): ");
        printf("%d", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return;
    }


    if (SOCKET_ERROR == listen(listenSocket, 5)) {
        printf("Time Server: Error at listen(): ");
        printf("%d", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    while(1)
    {

        struct sockaddr_in from;
        int fromLen = sizeof(from);

        printf("Time Server: Wait for clients' requests.\n");


        SOCKET msgSocket = accept(listenSocket, (struct sockaddr * ) & from, & fromLen);
        if (INVALID_SOCKET == msgSocket) {
            printf("Time Server: Error at accept(): ");
            printf("%d", WSAGetLastError());
            closesocket(listenSocket);
            WSACleanup();
            return;
        }

        printf("Time Server: Client is connected.\n");
        while (1) {
            int bytesSent = 0;
            int bytesRecv = 0;
            char sendBuff[255];
            char recvBuff[255];

            bytesRecv = recv(msgSocket, recvBuff, sizeof(recvBuff), 0);
            if (checkForAnError(bytesRecv, "recv", listenSocket, msgSocket))
                return;

            if (strncmp(recvBuff, "anything", 8) == 0) {
                char *fileContent = readFile("anything.txt");
                if (fileContent != NULL) {
                    strcpy(sendBuff, fileContent);
                } else {
                    printf("File 'anything.txt' not found locally. Making HTTP request...\n");

                    /* Send HTTP request to main server */
                    char responseBuffer[5000];
                    if (sendHttpRequest("GET /anything HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n", responseBuffer, sizeof(responseBuffer))) {
                        /*Save the received content to a new file*/
                        FILE *newFile = fopen("anything.txt", "w");
                        if (newFile != NULL) {
                            if (fwrite(responseBuffer, 1, strlen(responseBuffer), newFile) != strlen(responseBuffer)) {
                                printf("Error writing to file 'anything.txt'.\n");
                            } else {
                                printf("Content saved to 'anything.txt'.\n");
                            }
                            fclose(newFile);
                        } else {
                            printf("Error creating 'anything.txt'.\n");
                        }

                        /*Send the received content to the client*/
                        strcpy(sendBuff, responseBuffer);
                        bytesSent = send(msgSocket, sendBuff, strlen(sendBuff), 0);{
                            if (checkForAnError(bytesSent, "send", listenSocket, msgSocket))
                                return;
                        }
                    } else {
                        printf("Failed to send HTTP request to server.\n");
                    }
                }
            } else if (strncmp(recvBuff, "json", 4) == 0) { /* Second Request */
                char *fileContent = readFile("json.txt");
                if (fileContent != NULL) {
                    strcpy(sendBuff, fileContent);
                } else {
                    printf("File 'json.txt' not found locally. Making HTTP request...\n");

                    /* Send HTTP request to main server */
                    char responseBuffer[5000];
                    if (sendHttpRequest("GET /json HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n",
                                        responseBuffer, sizeof(responseBuffer))) {
                        /*Save the received content to a new file*/
                        FILE *newFile = fopen("json.txt", "w");
                        if (newFile != NULL) {
                            if (fwrite(responseBuffer, 1, strlen(responseBuffer), newFile) != strlen(responseBuffer)) {
                                printf("Error writing to file 'anything.txt'.\n");
                            } else {
                                printf("Content saved to 'anything.txt'.\n");
                            }
                            fclose(newFile);
                        } else {
                            printf("Error creating 'anything.txt'.\n");
                        }

                        /* Send the received content to the client*/
                        strcpy(sendBuff, responseBuffer);
                        bytesSent = send(msgSocket, sendBuff, strlen(sendBuff), 0);{
                            if (checkForAnError(bytesSent, "send", listenSocket, msgSocket))
                                return;
                        }
                    } else {
                        printf("Failed to send HTTP request to server.\n");
                    }
                }
            }
            else if (strncmp(recvBuff, "RTT", 3) == 0) { /* Third Request */
                clock_t startTime = clock();

                /* specific task related to RTT measurement */
                int i;
                for ( i = 0; i < 1000000; ++i) {
                    double temp = sqrt(i);
                }

                clock_t endTime = clock();

                double rtt = measureRTT(startTime, endTime); /* Calculate RTT */

                printf("RTT for request \"RTT\": %.2f ms\n", rtt);

                /*Convert double value rtt to string*/
                char rttStr[20]; /* Assuming a maximum of 20 characters for the string representation*/
                snprintf(rttStr, sizeof(rttStr), "%.2f", rtt);
                strcat(rttStr, " ms");

                /* Copy the string to sendBuff*/
                strcpy(sendBuff, rttStr);

                /* Send the string back to the client*/
                int bytesSent = send(msgSocket, sendBuff, strlen(sendBuff), 0);
                if (bytesSent == SOCKET_ERROR) {
                    printf("Error sending RTT result to client.\n");
                    closesocket(msgSocket);
                    return;
                }
            } else { /* Closing Socket */
                printf("Time Server: Closing Connection.\n");
                closesocket(msgSocket);
                break;
            }
            strcpy(sendBuff, "");
            strcpy(recvBuff, "");
        }
    }
    closesocket(listenSocket);
    WSACleanup();
    return;
}
