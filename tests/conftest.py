import os
import socket
import subprocess
import threading
import time
from pathlib import Path

import pytest

from tests.mock_http_server import create_mock_http_server


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SERVER_BINARY = PROJECT_ROOT / "build" / (
    "server.exe" if os.name == "nt" else "server"
)

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 27015

# Run this fixture only once for the entire pytest invocation.
# Run this automatically even if individual tests don't explicitly request it.
@pytest.fixture(scope="session", autouse=True)
def build_server():
    subprocess.run(
        ["make", "server"],
        cwd=PROJECT_ROOT,
        check=True
    )


@pytest.fixture
def mock_http_server():
    server = create_mock_http_server()

    thread = threading.Thread(
        target=server.serve_forever,
        daemon=True
    )

    thread.start()

    try:
        yield server
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


def wait_for_server(process, timeout=3.0):
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:

        if process.poll() is not None:
            output, _ = process.communicate()

            raise RuntimeError(
                "C server terminated during startup:\n"
                + output
            )

        try:
            with socket.create_connection(
                (SERVER_HOST, SERVER_PORT),
                timeout=0.1
            ):
                return

        except OSError:
            time.sleep(0.05)

    raise RuntimeError(
        "Timed out waiting for C server to start"
    )


@pytest.fixture
def c_server(
    build_server,
    mock_http_server,
    tmp_path
):
    mock_host, mock_port = (
        mock_http_server.server_address
    )

    environment = os.environ.copy()

    environment["TCP_HTTP_HOST"] = mock_host
    environment["TCP_HTTP_PORT"] = str(mock_port)

    process = subprocess.Popen(
        [str(SERVER_BINARY)],
        cwd=tmp_path,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True
    )

    try:
        wait_for_server(process)

        yield {
            "host": SERVER_HOST,
            "port": SERVER_PORT,
            "workdir": tmp_path,
            "process": process,
        }

    finally:
        process.terminate()

        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()