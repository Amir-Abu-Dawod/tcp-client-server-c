import socket
import struct


def recv_exact(sock, byte_count):
    data = bytearray()

    while len(data) < byte_count:
        chunk = sock.recv(
            byte_count - len(data)
        )

        if not chunk:
            raise ConnectionError(
                "Connection closed before complete response was received"
            )

        data.extend(chunk)

    return bytes(data)


def send_command(host, port, command):
    with socket.create_connection(
        (host, port),
        timeout=2.0
    ) as sock:

        sock.sendall(
            command.encode("utf-8")
        )

        length_bytes = recv_exact(sock, 4)

        payload_length = struct.unpack(
            "!I",
            length_bytes
        )[0]

        payload = recv_exact(
            sock,
            payload_length
        )

        return payload.decode("utf-8")