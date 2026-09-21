from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import threading

class MockHttpServer(ThreadingHTTPServer):

    def __init__(self, server_address, handler_class):
        super().__init__(
            server_address,
            handler_class
        )

        self.request_counts = {}
        self.request_counts_lock = threading.Lock()

        self.response_status = {
            "/anything": 200,
            "/json": 200,
        }

    def set_response_status(self, path, status_code):
        self.response_status[path] = status_code

    def record_request(self, path):
        with self.request_counts_lock:
            self.request_counts[path] = (
                self.request_counts.get(path, 0) + 1
            )

    def get_request_count(self, path):
        with self.request_counts_lock:
            return self.request_counts.get(path, 0)

class MockHttpHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        self.server.record_request(self.path)
        
        if self.path == "/anything":
            status_code = self.server.response_status.get(
                "/anything",
                200
            )

            self.send_response(status_code)

            if status_code == 200:
                self.send_header(
                    "Content-Type",
                    "text/plain"
                )
                self.end_headers()

                self.wfile.write(
                    b"mock anything response"
                )

            else:
                self.send_header(
                    "Content-Type",
                    "text/plain"
                )
                self.end_headers()

                self.wfile.write(
                    b"mock upstream failure"
                )

        elif self.path == "/json":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()

            self.wfile.write(
                b'{"message":"mock json response"}'
            )

        elif self.path == "/status/502":
            self.send_response(502)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()

            self.wfile.write(
                b"mock bad gateway"
            )

        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        # Keep automated test output clean.
        return


def create_mock_http_server():
    # port 0:= Ask the operating system to choose an available ephemeral port.
    server = MockHttpServer(
        ("127.0.0.1", 0),
        MockHttpHandler
    )

    return server


if __name__ == "__main__":
    server = create_mock_http_server()

    host, port = server.server_address

    print(
        f"Mock HTTP server listening on "
        f"{host}:{port}"
    )

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()