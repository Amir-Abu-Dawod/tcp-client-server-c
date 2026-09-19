#include <stdint.h>
#include <winsock2.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

const uint32_t MAX_RESPONSE_SIZE = 1024 * 1024;
const int CLIENT_PORT = 27015;

bool checkForAnError(int bytesResult, const char* errorAt, SOCKET socket){
    if (SOCKET_ERROR == bytesResult) {
        printf("Client: Error at %s(): ", errorAt);
        printf("%d\n", WSAGetLastError());
        closesocket(socket);
        WSACleanup();
        return true;
    }
    return false;
}

int recvAll(
    SOCKET socket,
    char *buffer,
    int length
)
{
    int totalReceived = 0;

    while (totalReceived < length)
    {
        int bytesReceived = recv(
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
            WSAGetLastError()
        );

        //socket error
        return -1;
    }

    //successfully received everything requested
    return 1;
}

int receiveFramedResponse(SOCKET socket)
{
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

    printf(
        "\nReceived from server:\n%s\n",
        response
    );

    free(response);

    return 1;
}

int main(void) {
    WSADATA wsaData;
    if (NO_ERROR != WSAStartup(MAKEWORD(2, 0), &wsaData)) {
        printf("Client: Error at WSAStartup()\n");
        return EXIT_FAILURE;
    }

    SOCKET connSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == connSocket) {
        printf("Client: Error at socket(): ");
        printf("%d", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(CLIENT_PORT);

    if (SOCKET_ERROR == connect(connSocket, (SOCKADDR*)&server, sizeof(server))) {
        printf("Client: Error at connect(): ");
        printf("%d", WSAGetLastError());
        closesocket(connSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    printf("Connection established successfully.\n");

    int bytesSent = 0;

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

        bytesSent = send(connSocket, sendBuff, (int)strlen(sendBuff), 0);
        if (checkForAnError(bytesSent, "send", connSocket))
            return EXIT_FAILURE;

        if (option == '4') {
            printf("\nClosing connection.\n");
            break;
        }

        // Reserve one byte for the null terminator.
        int receiveStatus = receiveFramedResponse(connSocket);

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

            closesocket(connSocket);
            WSACleanup();

            return EXIT_FAILURE;
        }
    }

    closesocket(connSocket);
    WSACleanup();

    return EXIT_SUCCESS;
}
