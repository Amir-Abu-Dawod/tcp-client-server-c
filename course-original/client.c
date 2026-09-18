#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include <windows.h>
#include <assert.h>


const int TIME_PORT = 27015;

bool checkForAnError(int bytesResult, char* ErrorAt, SOCKET socket){
    if (SOCKET_ERROR == bytesResult) {
        printf("Time Client: Error at %s(): ",ErrorAt);
        printf("%d", WSAGetLastError());
        closesocket(socket);
        WSACleanup();
        return true;
    }
    return false;
}

void main() {
    WSADATA wsaData;
    if (NO_ERROR != WSAStartup(MAKEWORD(2, 0), &wsaData)) {
        printf("Time Client: Error at WSAStartup()\n");
        return;
    }

    SOCKET connSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == connSocket) {
        printf("Time Client: Error at socket(): ");
        printf("%d", WSAGetLastError());
        WSACleanup();
        return;
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
        return;
    }
    printf("Connection established successfully.\n");

    int bytesSent = 0;
    int bytesRecv = 0;
    char sendBuff[255];
    char recvBuff[255];
    char option;

    while (option != '4') {
        printf("\nPlease insert an option :\n");
        printf("\n 1 : Get 'anything' file");
        printf("\n 2 : Get 'JSON' file");
        printf("\n 3 : Measure RTT");
        printf("\n 4 : Exit\n");
        printf("\n Your option : ");
        fflush(stdin);
        scanf(" %c", &option);

        switch (option) {
            case '1':
                strcpy(sendBuff, "anything");
                break;
            case '2':
                strcpy(sendBuff, "JSON");
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
            return;

        if (option == '4') {
            printf("Closing connection.\n");
            break;
        }

        bytesRecv = recv(connSocket, recvBuff, 255, 0);
        if (checkForAnError(bytesRecv, "recv", connSocket))
            return;

        recvBuff[bytesRecv] = '\0'; /* Ensure null termination*/
        printf("\nReceived from server: %s\n", recvBuff);

        memset(recvBuff, 0, sizeof(recvBuff));
        strcpy(recvBuff, "");
        strcpy(sendBuff, "");
    }

    closesocket(connSocket);
    WSACleanup();
}
