# TCP Client-Server Networking in C

A computer-networking course project implemented in C using Windows Winsock.

The project demonstrates TCP client-server communication, socket programming,
interaction with an external HTTP server, local response caching, and
request/response handling.

This repository initially preserves the original academic implementation.
The project is being extended with improved C networking design, Linux/POSIX
support, automated Python testing, failure-case validation, and CI.

## Original Project

The original implementation consists of two C programs:

- `client.c` — interactive TCP client
- `server.c` — TCP server that processes client requests and can communicate
  with an external HTTP server

The client connects to the server through localhost on TCP port `27015`.

The server can retrieve content from `httpbin.org` through an HTTP connection
and save returned content locally for later use.

## Architecture

```text
                 TCP :27015
+------------+ -----------------> +-------------+
|  C Client  |                    |  C Server   |
+------------+ <----------------- +-------------+
                                        |
                                        | HTTP / TCP :80
                                        v
                                  +-------------+
                                  | httpbin.org |
                                  +-------------+
                                        |
                             Local response cache
                              anything.txt/json.txt
