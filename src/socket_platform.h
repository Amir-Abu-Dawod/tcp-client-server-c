#ifndef SOCKET_PLATFORM_H
#define SOCKET_PLATFORM_H

#include <stdbool.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET socket_t;
typedef int socket_len_t;
typedef int socket_io_t;

#define SOCKET_INVALID INVALID_SOCKET
#define SOCKET_FAILURE SOCKET_ERROR

static inline int socketPlatformInit(void)
{
    WSADATA wsaData;

    return WSAStartup(
        MAKEWORD(2, 2),
        &wsaData
    );
}

static inline void socketPlatformCleanup(void)
{
    WSACleanup();
}

static inline int socketClose(socket_t socket)
{
    return closesocket(socket);
}

static inline int socketLastError(void)
{
    return WSAGetLastError();
}

#else

//POSIX/Linux

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

typedef int socket_t;
typedef socklen_t socket_len_t;
typedef ssize_t socket_io_t;

#define SOCKET_INVALID (-1)
#define SOCKET_FAILURE (-1)

static inline int socketPlatformInit(void)
{
    /*
     * Prevent send() from terminating the process with SIGPIPE
     * if the peer has already closed the connection.
     *
     * send() will instead fail normally, allowing our existing
     * error handling to deal with it.
     */
    signal(SIGPIPE, SIG_IGN);

    return 0;
}

static inline void socketPlatformCleanup(void)
{
    /* POSIX sockets require no global cleanup. */
}

static inline int socketClose(socket_t socket)
{
    return close(socket);
}

static inline int socketLastError(void)
{
    return errno;
}

#endif

#endif