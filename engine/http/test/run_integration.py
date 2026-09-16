"""Run the compiled HTTP tests against a temporary loopback server."""

import http.server
import os
import subprocess
import sys
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def respond(self):
        try:
            if self.path == "/no-read":
                time.sleep(3)
                self.close_connection = True
                return
            if self.path == "/slow-headers":
                time.sleep(0.3)

            body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
            if self.path == "/redirect":
                self.send_response(302)
                self.send_header("Location", "/echo?redirected=1")
                body = b"redirect body"
            elif self.path == "/missing":
                self.send_response(404)
                body = b"missing"
            else:
                self.send_response(200)
            if self.path == "/slow-body":
                body = b"delayed body"
            self.send_header("X-Method", self.command)
            self.send_header("X-Path", self.path)
            self.send_header("X-Test", self.headers.get("X-Test", ""))
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.flush()
            if self.path == "/slow-body":
                time.sleep(0.3)
            if self.command != "HEAD":
                self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            self.close_connection = True

    do_GET = respond
    do_HEAD = respond
    do_POST = respond
    do_PUT = respond
    do_DELETE = respond
    do_PATCH = respond
    do_OPTIONS = respond
    do_TRACE = respond
    do_CONNECT = respond


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: run_integration.py PATH_TO_CORE_HTTP_TEST")
    with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        environment = os.environ.copy()
        environment["LUASTG_HTTP_TEST_URL"] = f"http://127.0.0.1:{server.server_port}"
        environment["NO_PROXY"] = "127.0.0.1,localhost"
        environment["no_proxy"] = "127.0.0.1,localhost"
        try:
            return subprocess.run([sys.argv[1]], env=environment, timeout=35).returncode
        finally:
            server.shutdown()
            worker.join()


if __name__ == "__main__":
    sys.exit(main())
