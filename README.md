# TCP Client–Server in C

A cross-platform TCP client–server application written in C, originally developed as an academic socket-programming project and later modernized to improve protocol correctness, portability, error handling, testability, and automated verification.

The repository preserves the original course implementation while maintaining a separate modernization path that demonstrates how a small academic networking project can be evolved toward more production-oriented engineering practices.

---

## Project Status

The repository contains two distinct stages of the project:

* **`main`** — untouched original academic implementation
* **`modernization`** — refactored and extended cross-platform implementation

The original course version is also preserved with the Git tag:

```text
v0.1-course-original
```

This makes it possible to compare the original implementation directly with the modernized version.

---

## Features

The modernized implementation includes:

* TCP client–server communication in C
* Length-prefixed framed server responses
* Reliable full-buffer send and receive handling
* HTTP resource retrieval
* Local file caching
* HTTP status validation
* Real application-level RTT measurement
* Structured client-command dispatch
* Reusable HTTP resource handling
* Modern hostname resolution with `getaddrinfo()`
* Windows Winsock support
* Linux/POSIX socket support
* Portable monotonic timing
* Configurable HTTP upstream for deterministic testing
* Automated Python integration tests
* Deterministic mock HTTP server
* Linux and Windows GitHub Actions CI

---

## Architecture

The application consists of a TCP client, a TCP server, platform abstractions, and a Python integration-test layer.

```text
tcp-client-server-c/
├── src/
│   ├── client.c
│   ├── server.c
│   ├── socket_platform.h
│   └── timer_platform.h
│
├── tests/
│   ├── __init__.py
│   ├── conftest.py
│   ├── mock_http_server.py
│   ├── protocol_client.py
│   ├── test_requests.py
│   ├── test_cache.py
│   ├── test_rtt.py
│   └── test_failures.py
│
├── .github/
│   └── workflows/
│       └── tests.yml
│
├── Makefile
├── requirements-dev.txt
├── .gitignore
└── README.md
```

At runtime, the normal application flow is:

```text
Interactive C Client
        │
        │ TCP
        ▼
      C Server
        │
        │ HTTP
        ▼
    httpbin.org
```

During automated testing, the external HTTP dependency is replaced:

```text
Python Test Client
        │
        │ TCP
        ▼
      C Server
        │
        │ HTTP
        ▼
Python Mock HTTP Server
```

This makes the automated test suite deterministic and independent of external network availability.

---

## Client Commands

The interactive client currently provides four operations:

```text
1 : Get 'anything' resource
2 : Get JSON resource
3 : Measure RTT
4 : Exit
```

The corresponding application protocol commands are:

```text
anything
json
RTT
EXIT
```

---

## TCP Response Framing

TCP is a byte stream and does not preserve application-message boundaries.

The modernized implementation therefore uses explicit framing for server responses:

```text
+----------------------+-------------------------+
| 4-byte payload size  | response payload        |
| network byte order   | N bytes                 |
+----------------------+-------------------------+
```

The server sends:

1. a 32-bit unsigned payload length in network byte order;
2. exactly that many payload bytes.

The client first reads exactly four bytes, converts the length with network-byte-order semantics, allocates the required buffer, and then receives the complete payload.

This avoids assuming that a single `recv()` call contains one complete application response.

---

## Reliable TCP I/O

Both sides handle partial TCP operations.

Instead of assuming:

```c
send(socket, data, length, 0);
```

transmits every byte, the project uses loops that continue until the entire requested buffer has been processed.

Conceptually:

```text
sendAll()
    ↓
send remaining bytes
    ↓
partial send?
    ↓ yes
advance pointer
    ↓
send remaining bytes
```

The same approach is used when receiving framed responses.

---

## HTTP Retrieval and Caching

The server supports retrieval of:

```text
/anything
/json
```

from an upstream HTTP service.

For each resource:

```text
client request
    ↓
check local cache
   / \
 hit  miss
 /      \
read     HTTP request
file        ↓
 |       validate status
 |          ↓
 |       cache successful response
 \          /
   send framed response
```

Only successful `2xx` HTTP responses are treated as successful resource retrievals.

For example, an upstream:

```text
HTTP/1.1 502 Bad Gateway
```

is not cached.

Instead, the client receives a framed application-level error such as:

```text
ERROR: Failed to retrieve anything resource.
```

This behavior is covered by automated tests.

---

## RTT Measurement

The original academic implementation measured server-side CPU work and labeled the result as RTT.

The modernization replaces this with an application-level round-trip measurement performed by the client:

```text
Client                               Server

timestamp start
    |
send "RTT" ------------------------->
                                       receive RTT
                                       send RTT_ACK
                  <------------------
receive complete framed response
    |
timestamp end

RTT = end - start
```

The measurement therefore includes:

* client-side command transmission
* TCP transport
* server command processing
* framed acknowledgement transmission
* complete client-side response reception

Example:

```text
RTT: 0.105 ms
```

When both processes run on the same host through `127.0.0.1`, values are naturally very small.

### Platform-specific monotonic clocks

Windows uses:

```text
QueryPerformanceCounter()
```

Linux/POSIX uses:

```text
clock_gettime(CLOCK_MONOTONIC, ...)
```

These differences are hidden behind `timer_platform.h`.

---

## Cross-Platform Design

The same `client.c` and `server.c` sources compile natively on Windows and Linux.

Platform-specific behavior is isolated behind:

```text
socket_platform.h
timer_platform.h
```

### Socket abstraction

Examples of mapped functionality:

| Operation      | Windows             | Linux/POSIX              |
| -------------- | ------------------- | ------------------------ |
| Socket type    | `SOCKET`            | `int`                    |
| Invalid socket | `INVALID_SOCKET`    | `-1`                     |
| Close socket   | `closesocket()`     | `close()`                |
| Last error     | `WSAGetLastError()` | `errno`                  |
| Initialization | `WSAStartup()`      | no global socket startup |
| Cleanup        | `WSACleanup()`      | no global cleanup        |

Linux additionally ignores `SIGPIPE` so failed socket writes are handled through normal error-return paths instead of terminating the process.

The POSIX server also enables `SO_REUSEADDR` to improve reliability when repeatedly restarting the server during development and automated tests.

---

## Modern Address Resolution

The original `gethostbyname()` approach was replaced with:

```c
getaddrinfo()
```

The modernized resolver:

* supports IPv4 and IPv6 candidates;
* returns a list of possible addresses;
* allows the server to attempt multiple resolved addresses;
* works on both Winsock and POSIX.

---

## Configurable HTTP Upstream

Normal execution defaults to:

```text
Host: httpbin.org
Port: 80
```

For automated testing, the upstream service can be overridden using:

```text
TCP_HTTP_HOST
TCP_HTTP_PORT
```

Example:

```bash
TCP_HTTP_HOST=127.0.0.1 TCP_HTTP_PORT=40000 ./build/server
```

Port values are validated before use.

This configuration allows the Python test suite to redirect HTTP traffic toward a deterministic local mock server without changing production code.

---

# Building

## Linux / WSL

Requirements:

* GCC
* GNU Make

On Ubuntu:

```bash
sudo apt update
sudo apt install build-essential
```

Build both programs:

```bash
make
```

Generated binaries:

```text
build/server
build/client
```

Run the server:

```bash
./build/server
```

Run the client from another terminal:

```bash
./build/client
```

Clean generated files:

```bash
make clean
```

---

## Windows

The project can be compiled with GCC/MinGW-w64.

The Makefile automatically links against Winsock:

```text
-lws2_32
```

Build:

```powershell
make
```

Generated binaries:

```text
build/server.exe
build/client.exe
```

Run:

```powershell
.\build\server.exe
```

and, from another terminal:

```powershell
.\build\client.exe
```

---

# Automated Testing

The automated test suite uses Python and `pytest`.

Development dependency:

```text
pytest
```

Install it using:

```bash
python -m pip install -r requirements-dev.txt
```

On Linux, depending on the Python installation:

```bash
python3 -m pip install -r requirements-dev.txt
```

---

## Virtual Environment

A local Python virtual environment is recommended.

### Linux / WSL

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements-dev.txt
```

### Windows PowerShell

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements-dev.txt
```

The `.venv/` directory is intentionally excluded from Git.

---

## Running Tests

Run the complete automated suite with:

```bash
make test
```

or directly with pytest:

```bash
python3 -m pytest -v
```

On Windows:

```powershell
python -m pytest -v
```

The current integration suite contains **9 tests**.

---

## Test Architecture

The test suite does not automate keystrokes into the interactive C client.

Instead, Python acts as a protocol-aware TCP client and communicates directly with the real C server.

The Python protocol helper implements:

```text
send command
    ↓
receive exactly 4 bytes
    ↓
decode network-order payload length
    ↓
receive exactly N payload bytes
    ↓
return decoded response
```

This directly validates the application's TCP framing protocol.

---

## Test Coverage

### Request tests

Verify successful retrieval of:

```text
/anything
/json
```

through the real C server and local mock HTTP server.

### Cache tests

Verify that:

1. the first request reaches the mock HTTP service;
2. the response is written to the cache;
3. the second request returns the same content;
4. the second request does **not** contact the upstream HTTP service again.

The mock HTTP server maintains endpoint request counters so this behavior is explicitly verified.

### RTT protocol tests

Verify that:

```text
RTT
```

returns:

```text
RTT_ACK
```

using the real TCP framing implementation.

Repeated RTT requests also verify that handling one request does not leave the server in an unusable state.

The automated suite tests the server's RTT protocol path. The platform-specific C client timing calculation is verified separately through native Windows and Linux runtime testing.

### Failure tests

The suite includes deterministic upstream HTTP failures.

For example:

```text
mock HTTP → 502
```

must result in:

```text
ERROR: Failed to retrieve anything resource.
```

and no cache file may be created.

Unknown application commands are also tested:

```text
NOT_A_COMMAND
```

must produce:

```text
ERROR: Unknown command.
```

A second valid request is then sent to the same server process to verify that the invalid command did not terminate or corrupt the server.

---

## Test Isolation

Each pytest test gets:

* a fresh C server process;
* a fresh mock HTTP server;
* a dynamically selected HTTP port;
* a separate temporary working directory.

The temporary directory also isolates:

```text
anything.txt
json.txt
```

so tests do not depend on files created by earlier tests.

Only the C build fixture is session-scoped and reused across the complete pytest session.

---

# Continuous Integration

GitHub Actions runs the integration suite automatically.

The workflow contains independent jobs for:

```text
Linux
Windows
```

The Linux job validates the POSIX implementation.

The Windows job validates the native Winsock implementation.

Both jobs:

1. check out the repository;
2. configure Python;
3. install test dependencies;
4. compile the native C server;
5. execute the same Python integration suite.

This continuously verifies both sides of the portability abstractions.

---

## CI Coverage

The two CI environments exercise different platform implementations.

### Linux

```text
POSIX socket descriptors
close()
errno
SIGPIPE handling
SO_REUSEADDR
CLOCK_MONOTONIC
```

### Windows

```text
Winsock SOCKET
WSAStartup()
closesocket()
WSAGetLastError()
ws2_32
QueryPerformanceCounter()
```

Both environments must pass the same application-level integration tests.

---

# Original vs Modernized Implementation

The original course implementation remains intentionally preserved.

```text
main
└── original academic implementation

tag:
v0.1-course-original
```

The modernization work lives on:

```text
modernization
```

Major modernization steps include:

```text
✓ safer client input and receive handling
✓ server correctness fixes
✓ reliable framed TCP responses
✓ reusable resource handling
✓ separated command dispatch
✓ socket ownership cleanup
✓ real application-level RTT
✓ reliable client transmission
✓ modern getaddrinfo() resolution
✓ Windows/POSIX socket abstraction
✓ portable monotonic timing
✓ HTTP status validation
✓ deterministic Python integration testing
✓ automated cache/failure verification
✓ Linux CI
✓ Windows CI
```

The preserved original version makes the evolution of the codebase directly inspectable through Git history and branch/tag comparison.

---

# Engineering Goals

The modernization deliberately focuses on several software-engineering principles:

* understand TCP as a byte stream rather than a message protocol;
* explicitly define application framing;
* handle partial I/O correctly;
* make ownership of sockets and allocated memory clear;
* separate command dispatch from session management;
* isolate operating-system-specific APIs;
* use monotonic clocks for elapsed-time measurements;
* distinguish TCP success from HTTP success;
* avoid caching failed upstream responses;
* design external dependencies for testability;
* keep automated tests deterministic;
* validate recovery after failure, not only successful output;
* continuously test native Windows and Linux implementations.

---

# Technologies

### C / Networking

* C11
* TCP/IP
* HTTP/1.x
* Winsock2
* POSIX sockets
* `getaddrinfo()`
* network byte order
* application-layer framing

### Build / Platform

* GCC
* GNU Make
* MinGW-w64 / MSYS2
* Linux / WSL
* Windows

### Testing / Automation

* Python 3
* pytest
* `http.server`
* Python sockets
* subprocess management
* temporary test environments
* GitHub Actions

---

# Future Improvements

Possible extensions include:

* framing client-to-server commands in addition to server responses;
* dynamic request construction for configurable HTTP `Host` headers;
* graceful signal-driven server shutdown;
* additional malformed-frame and connection-drop tests;
* concurrent client handling;
* socket timeouts;
* IPv6-specific integration tests;
* configurable client/server listening ports;
* more detailed HTTP parsing or use of a dedicated HTTP layer.

These are intentionally separate from the modernization already completed so the project remains focused and understandable.

---

## License

This repository originated as an academic project and is maintained as a learning and portfolio project.
