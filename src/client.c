#include <winsock2.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>


const int TIME_PORT = 27015;

bool checkForAnError(int bytesResult, const char* errorAt, SOCKET socket){
    if (SOCKET_ERROR == bytesResult) {
        printf("Time Client: Error at %s(): ", errorAt);
        printf("%d\n", WSAGetLastError());
        closesocket(socket);
        WSACleanup();
        return true;
    }
    return false;
}

int main(void) {
    WSADATA wsaData;
    if (NO_ERROR != WSAStartup(MAKEWORD(2, 0), &wsaData)) {
        printf("Time Client: Error at WSAStartup()\n");
        return EXIT_FAILURE;
    }

    SOCKET connSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == connSocket) {
        printf("Time Client: Error at socket(): ");
        printf("%d", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(TIME_PORT);

    if (SOCKET_ERROR == connect(connSocket, (SOCKADDR*)&server, sizeof(server))) {
        printf("Time Client: Error at connect(): ");
        printf("%d", WSAGetLastError());
        closesocket(connSocket);
        WSACleanup();
        return EXIT_FAILURE;
    }
    printf("Connection established successfully.\n");

    int bytesSent = 0;
    int bytesRecv = 0;

    char sendBuff[255];
    char recvBuff[255];
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
            printf("Closing connection.\n");
            break;
        }

        // Reserve one byte for the null terminator.
        bytesRecv = recv(connSocket, recvBuff, (int)sizeof(recvBuff) - 1, 0);
        if (checkForAnError(bytesRecv, "recv", connSocket))
            return EXIT_FAILURE;

        if (bytesRecv == 0) {
            printf("\nServer closed the connection.\n");
            break;
        }

        recvBuff[bytesRecv] = '\0'; 
        printf("\nReceived from server: %s\n", recvBuff);

    }

    closesocket(connSocket);
    WSACleanup();

    return EXIT_SUCCESS;
}
