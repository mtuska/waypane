#!/usr/bin/env python3
import os
import subprocess
import unittest


class SshRunnerTests(unittest.TestCase):
    def run_runner(self, command: str, *, legacy: bool = False) -> subprocess.CompletedProcess[bytes]:
        return subprocess.run(
            [
                os.environ["WAYPANE_SSH_RUNNER"],
                "--profile",
                "Legacy appliance",
                "--legacy-enabled",
                "true" if legacy else "false",
                "--",
                "/bin/sh",
                "sh",
                "-c",
                command,
            ],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=5,
            check=False,
        )

    def test_success_exits_without_failure_banner(self) -> None:
        result = self.run_runner('printf "connected\\n"')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b"connected\n")
        self.assertNotIn(b"could not establish", result.stderr)

    def test_failed_connection_preserves_diagnostic_and_suggests_legacy_mode(self) -> None:
        result = self.run_runner(
            'printf "Unable to negotiate: no matching host key type found\\n" >&2; exit 255'
        )
        self.assertEqual(result.returncode, 255)
        self.assertIn(b"no matching host key type found", result.stderr)
        self.assertIn(b"Waypane could not establish the SSH session", result.stderr)
        self.assertIn(b"enable\nLegacy server compatibility", result.stderr)
        self.assertIn(b"Press Enter to close", result.stderr)

    def test_enabled_legacy_mode_does_not_suggest_enabling_it_again(self) -> None:
        result = self.run_runner('printf "connection refused\\n" >&2; exit 255', legacy=True)
        self.assertEqual(result.returncode, 255)
        self.assertIn(b"Legacy compatibility is already enabled", result.stderr)
        self.assertNotIn(b"edit this connection and enable", result.stderr)


if __name__ == "__main__":
    unittest.main()
