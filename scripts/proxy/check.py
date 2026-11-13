"""Portable proxy tests, including on Windows without an Amiga C compiler."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'amiga'))
suite = unittest.defaultTestLoader.loadTestsFromNames([
    'test_cli_relay', 'test_codex_relay.AsyncRelayTests',
    'test_codex_relay.RelayTests.test_job_boundaries',
    'test_codex_relay.RelayTests.test_tool_permissions_and_text',
    'test_codex_relay.RelayTests.test_auth_stays_local'])
result = unittest.TextTestRunner(verbosity=2).run(suite)
sys.exit(not result.wasSuccessful())
