#include <stdint.h>
#include "socket_platform.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

const uint32_t MAX_RESPONSE_SIZE = 1024 * 1024;
const int CLIENT_PORT = 27015;

double elapsedMilliseconds(
    LARGE_INTEGER start,
    LARGE_INTEGER end,
    LARGE_INTEGER frequency
)
{
    return
        ((double)(end.QuadPart - start.QuadPart) * 1000.0) /
        (double)frequency.QuadPart;
}

bool checkForAnError(int bytesResult, const char* errorAt, socket_t socket){
    if (SOCKET_FAILURE == bytesResult) {
        printf("Client: Error at %s(): ", errorAt);
        printf("%d\n", socketLastError());
        socketClose(socket);
        socketPlatformCleanup();
        return true;
    }
    return false;
}

bool sendAll(
    socket_t socket,
    const char *data,
    int length
)
{
    socket_io_t totalSent = 0;

    while (totalSent < length)
    {
        socket_io_t bytesSent = send(
            socket,
            data + totalSent,
            length - totalSent,
            0
        );

        if (bytesSent == SOCKET_FAILURE)
        {
            printf(
                "Error sending data: %d\n",
                socketLastError()
            );

            return false;
        }

        if (bytesSent == 0)
        {
            printf(
                "Socket closed while sending data.\n"
            );

            return false;
        }

        totalSent += bytesSent;
    }

    return true;
}

int recvAll(
    socket_t socket,
    char *buffer,
    int length
)
{
    socket_io_t totalReceived = 0;

    while (totalReceived < length)
    {
        socket_io_t bytesReceived = recv(
            socket,
            buffer + totalReceived,
            length - totalReceived,
            0
        );

        if (bytesReceived > 0)
        {
            totalReceived += bytesReceived;
            continue;
        }

        if (bytesReceived == 0)
        {
            //peer closed connection
            return 0;
        }

        printf(
            "Error receiving data: %d\n",
            socketLastError()
        );

        //socket error
        return -1;
    }

    //successfully received everything requested
    return 1;
}

int receiveFramedResponse(socket_t socket, char **responseOut)
{
    if (responseOut == NULL)
    {
        return -1;
    }

    *responseOut = NULL;

    uint32_t networkLength = 0;

    int status = recvAll(
        socket,
        (char *)&networkLength,
        (int)sizeof(networkLength)
    );

    if (status <= 0)
    {
        return status;
    }

    uint32_t payloadLength =
        ntohl(networkLength);

    if (payloadLength > MAX_RESPONSE_SIZE)
    {
        printf(
            "Server response exceeds maximum allowed size.\n"
        );
        return -1;
    }

    char *response =
        malloc((size_t)payloadLength + 1);

    if (response == NULL)
    {
        printf(
            "Failed to allocate response buffer.\n"
        );
        return -1;
    }

    if (payloadLength > 0)
    {
        status = recvAll(
            socket,
            response,
            (int)payloadLength
        );

        if (status <= 0)
        {
            free(response);
            return status;
        }
    }

    response[payloadLength] = '\0';

    *responseOut = response;

    return 1;
}

int main(void) {
    LARGE_INTEGER frequency;

    if (!QueryPerformanceFrequency(&frequency))
    {
        printf("Failed to initialize high-resolution timer.\n");
        return EXIT_FAILURE;
    }

    int platformStatus = socketPlatformInit();

    if (platformStatus != 0)
    {
        printf(
            "Client: Failed to initialize socket platform: %d\n",
            platformStatus
        );

        return EXIT_FAILURE;
    }

    socket_t connSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SOCKET_INVALID == connSocket) {
        printf("Client: Error at socket(): %d\n", socketLastError());
        socketClose(connSocket);
        socketPlatformCleanup();
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(CLIENT_PORT);

    if (SOCKET_FAILURE == connect(connSocket, (struct sockaddr *)&server, sizeof(server))) {
        printf("Client: Error at connect(): ");
        printf("%d", socketLastError());
        socketClose(connSocket);
        socketPlatformCleanup();
        return EXIT_FAILURE;
    }
    printf("Connection established successfully.\n");

    char sendBuff[255];
    char option;

    while (true) {
        printf("\nPlease insert an option :\n");
        printf("\n 1 : Get 'anything' file");
        printf("\n 2 : Get 'JSON' file");
        printf("\n 3 : Measure RTT");
        printf("\n 4 : Exit\n");
        printf("\n Your option : ");

        if (scanf(" %c", &option) != 1) { //space to skip whitespace
            printf("\nFailed to read input.\n");
            break;
        }

        switch (option) {
            case '1':
                strcpy(sendBuff, "anything");
                break;
            case '2':
                strcpy(sendBuff, "json");
                break;
            case '3':
                strcpy(sendBuff, "RTT");
                break;
            case '4':
                strcpy(sendBuff, "EXIT");
                break;
            default:
                printf("\n *-*-* Please enter a valid option only. *-*-*\n");
                continue;
        }

        LARGE_INTEGER rttStart;
        LARGE_INTEGER rttEnd;

        if (option == '3')
        {
            QueryPerformanceCounter(&rttStart);
        }

        if (!sendAll(
            connSocket,
            sendBuff,
            (int)strlen(sendBuff)))
        {
            printf("\nFailed to send command to server.\n");

            socketClose(connSocket);
            socketPlatformCleanup();

            return EXIT_FAILURE;
        }

        if (option == '4') {
            printf("\nClosing connection.\n");
            break;
        }

        char *response = NULL;

        // Reserve one byte for the null terminator.
        int receiveStatus = receiveFramedResponse(connSocket, &response);

        if (receiveStatus == 0)
        {
            printf(
                "\nServer closed the connection.\n"
            );
            break;
        }

        if (receiveStatus < 0)
        {
            printf(
                "\nFailed to receive server response.\n"
            );

            socketClose(connSocket);
            socketPlatformCleanup();

            return EXIT_FAILURE;
        }
        if (option == '3')
        {
            QueryPerformanceCounter(&rttEnd);

            double rttMs =
                elapsedMilliseconds(
                    rttStart,
                    rttEnd,
                    frequency
                );

            printf("\nRTT: %.3f ms\n", rttMs);
        }
        else
        {
            printf(
                "\nReceived from server:\n%s\n",
                response
            );
        }

        free(response);
    }

    socketClose(connSocket);
    socketPlatformCleanup();

    return EXIT_SUCCESS;
}
