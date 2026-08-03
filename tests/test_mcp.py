#!/usr/bin/env python3
from __future__ import annotations

import json
import os
import socket
import subprocess
import tempfile
import threading
import unittest


class McpTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        env = dict(os.environ)
        env["WAYPANE_DATA_DIR"] = self.temp.name
        self.control_socket = os.path.join(self.temp.name, "control.sock")
        env["WAYPANE_CONTROL_SOCKET"] = self.control_socket
        self.process = subprocess.Popen(
            [env["WAYPANE_MCP"]],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
        )

    def tearDown(self) -> None:
        self.process.terminate()
        self.process.wait(timeout=2)
        self.temp.cleanup()

    def rpc(self, method: str, params: dict | None = None, request_id: int = 1) -> dict:
        request = {"jsonrpc": "2.0", "id": request_id, "method": method}
        if params is not None:
            request["params"] = params
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        self.process.stdin.write(json.dumps(request) + "\n")
        self.process.stdin.flush()
        return json.loads(self.process.stdout.readline())

    def test_initialize_and_tools(self) -> None:
        initialized = self.rpc("initialize", {"protocolVersion": "2025-11-25"})
        self.assertEqual(initialized["result"]["serverInfo"]["name"], "waypane")
        tools = self.rpc("tools/list", request_id=2)["result"]["tools"]
        self.assertEqual(len(tools), 13)
        self.assertIn("waypane_split_terminal", {tool["name"] for tool in tools})
        self.assertIn("waypane_edit_remote_file", {tool["name"] for tool in tools})
        self.assertIn("waypane_open_tunnels", {tool["name"] for tool in tools})

    def test_profile_lifecycle_and_safe_argv(self) -> None:
        created = self.rpc(
            "tools/call",
            {
                "name": "waypane_upsert_connection",
                "arguments": {
                    "name": "Production DB",
                    "host": "db.example.test",
                    "username": "deploy",
                    "port": 2222,
                    "identityFile": "/tmp/key with spaces",
                    "jumpHosts": ["edge", "bastion"],
                },
            },
        )
        profile = json.loads(created["result"]["content"][0]["text"])
        self.assertNotIn("secretId", profile)
        built = self.rpc(
            "tools/call",
            {"name": "waypane_build_ssh_argv", "arguments": {"id": profile["id"]}},
            request_id=2,
        )
        argv = json.loads(built["result"]["content"][0]["text"])["argv"]
        self.assertEqual(
            argv,
            ["ssh", "-p", "2222", "-i", "/tmp/key with spaces", "-J", "edge,bastion", "deploy@db.example.test"],
        )

    def test_live_runtime_bridge(self) -> None:
        ready = threading.Event()

        def serve_once() -> None:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
                listener.bind(self.control_socket)
                listener.listen(1)
                ready.set()
                connection, _ = listener.accept()
                with connection:
                    request = json.loads(connection.makefile("r", encoding="utf-8").readline())
                    self.assertEqual(request, {"method": "status"})
                    connection.sendall(b'{"ok":true,"running":true,"terminalCount":2}\n')

        server = threading.Thread(target=serve_once)
        server.start()
        self.assertTrue(ready.wait(timeout=2))
        response = self.rpc(
            "tools/call",
            {"name": "waypane_runtime_status", "arguments": {}},
        )
        status = json.loads(response["result"]["content"][0]["text"])
        self.assertTrue(status["running"])
        self.assertEqual(status["terminalCount"], 2)
        server.join(timeout=2)

    def test_live_split_and_remote_editor_routing(self) -> None:
        created = self.rpc(
            "tools/call",
            {
                "name": "waypane_upsert_connection",
                "arguments": {
                    "name": "Files",
                    "host": "files.example.test",
                    "localForwards": ["127.0.0.1:8080:web:80"],
                },
            },
        )
        profile_id = json.loads(created["result"]["content"][0]["text"])["id"]
        expected = [
            {"method": "splitTerminal", "direction": "down"},
            {"method": "editRemoteFile", "id": profile_id, "path": "/etc/hosts"},
            {"method": "openTunnels", "id": profile_id},
        ]
        ready = threading.Event()

        def serve() -> None:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener:
                listener.bind(self.control_socket)
                listener.listen(3)
                ready.set()
                for request_expected in expected:
                    connection, _ = listener.accept()
                    with connection:
                        request = json.loads(connection.makefile("r", encoding="utf-8").readline())
                        self.assertEqual(request, request_expected)
                        connection.sendall(b'{"ok":true}\n')

        server = threading.Thread(target=serve)
        server.start()
        self.assertTrue(ready.wait(timeout=2))
        split = self.rpc(
            "tools/call",
            {"name": "waypane_split_terminal", "arguments": {"direction": "down"}},
            request_id=2,
        )
        self.assertTrue(json.loads(split["result"]["content"][0]["text"])["ok"])
        editor = self.rpc(
            "tools/call",
            {"name": "waypane_edit_remote_file", "arguments": {"id": profile_id, "path": "/etc/hosts"}},
            request_id=3,
        )
        self.assertTrue(json.loads(editor["result"]["content"][0]["text"])["ok"])
        tunnels = self.rpc(
            "tools/call",
            {"name": "waypane_open_tunnels", "arguments": {"id": profile_id}},
            request_id=4,
        )
        self.assertTrue(json.loads(tunnels["result"]["content"][0]["text"])["ok"])
        server.join(timeout=2)


if __name__ == "__main__":
    unittest.main()
