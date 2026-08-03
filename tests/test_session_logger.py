#!/usr/bin/env python3
import os
import pathlib
import stat
import subprocess
import tempfile
import unittest


class SessionLoggerTests(unittest.TestCase):
    def test_records_pty_output_with_utc_audit_metadata(self) -> None:
        logger = os.environ["WAYPANE_SESSION_LOGGER"]
        with tempfile.TemporaryDirectory() as directory:
            log = pathlib.Path(directory) / "session.log"
            result = subprocess.run(
                [
                    logger,
                    "--log",
                    str(log),
                    "--host",
                    "audit.example",
                    "--",
                    "/bin/sh",
                    "sh",
                    "-c",
                    'printf "ready\\n"; read value; printf "received:%s\\n" "$value"',
                ],
                input=b"safe-command\n",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=5,
                check=True,
            )
            contents = log.read_text(encoding="utf-8")
            self.assertIn("# Host: audit.example", contents)
            self.assertRegex(contents, r"# Started UTC: \d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z")
            self.assertIn("received:safe-command", contents)
            self.assertIn("# Ended UTC:", contents)
            self.assertIn(b"received:safe-command", result.stdout)
            self.assertEqual(stat.S_IMODE(log.stat().st_mode), 0o600)


if __name__ == "__main__":
    unittest.main()
