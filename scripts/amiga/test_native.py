#!/usr/bin/env python3
"""Portable protocol tests. Hardware acceptance is a separate explicit command."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "amiga/native"


class NativeProtocolTests(unittest.TestCase):
    def test_streaming_archive_checksum(self):
        import zlib
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);binary=work/'checksum';data=work/'fixture.bin'
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined',str(SOURCE/'tests/check_archive.c'),
                '-o',str(binary)],check=True)
            for raw in (b'',b'123456789',bytes(range(256))*1024):
                data.write_bytes(raw)
                result=subprocess.check_output([str(binary),str(data)],text=True)
                self.assertEqual(result.strip(),f'{zlib.crc32(raw):08x} {len(raw)} {data}')
            self.assertEqual(subprocess.run([str(binary),str(work/'absent')]).returncode,20)

    def test_diagnostics(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'diagnostics'
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I',str(SOURCE),
                *[str(SOURCE/f) for f in ['diagnostics.c','workspace.c','tests/test_diagnostics.c']],
                '-o',str(binary)],check=True)
            subprocess.run([str(binary)],cwd=directory,check=True)

    def test_review_diff(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'review-diff'
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I',str(SOURCE),
                str(SOURCE/'tests/test_review_diff.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

    def test_change_review(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'review'
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
                '-fsanitize=address,undefined','-I',str(SOURCE),
                *[str(SOURCE/f) for f in ['change_review.c','workspace.c','files.c','vendor/cJSON.c','tests/test_change_review.c']],
                '-o',str(binary)],check=True)
            subprocess.run([str(binary)],cwd=directory,check=True)

    def test_taskflow(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'taskflow'
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
                '-fsanitize=address,undefined','-I',str(SOURCE),
                *[str(SOURCE/f) for f in ['taskflow.c','workspace.c','files.c','vendor/cJSON.c','tests/test_taskflow.c']],
                '-o',str(binary)],check=True)
            subprocess.run([str(binary)],cwd=directory,check=True)

    def test_prompts(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "prompts")
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                "-Wno-deprecated-declarations", "-fsanitize=address,undefined", "-I", str(SOURCE)]
                + [str(SOURCE / f) for f in ["prompts.c", "files.c", "vendor/cJSON.c", "tests/test_prompts.c"]]
                + ["-o", binary], check=True)
            subprocess.run([binary, str(SOURCE / "Prompts/defaults.json")], check=True)

    def test_shortcuts(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "shortcuts")
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-deprecated-declarations",
                "-fsanitize=address,undefined", "-I", str(SOURCE),
                str(SOURCE / "tests/test_shortcuts.c"), str(SOURCE / "vendor/cJSON.c"), "-lm", "-o", binary], check=True)
            subprocess.run([binary], check=True)

    def test_profiles(self):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory)
            (work/"Config").mkdir(); (work/"Env").mkdir()
            binary=work/"profiles"
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-deprecated-declarations", "-fsanitize=address,undefined", "-I", str(SOURCE), '-DTERMIC_CONFIG="Config/"', '-DTERMIC_ENV="Env/"'] + [str(SOURCE/f) for f in ["profiles.c", "files.c", "workspace.c", "vendor/cJSON.c", "tests/test_profiles.c"]] + ["-o",str(binary)],check=True)
            subprocess.run([str(binary)],cwd=work,check=True)

    def test_chrome_geometry(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "chrome")
            subprocess.run(
                [
                    "cc",
                    "-std=c99",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-fsanitize=address,undefined",
                    "-I",
                    str(SOURCE),
                    str(SOURCE / "tests/test_chrome.c"),
                    "-o",
                    binary,
                ],
                check=True,
            )
            subprocess.run([binary], check=True)

    def test_preferences(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            (work / "Config").mkdir()
            (work / "Env").mkdir()
            binary = work / "prefs"
            subprocess.run(
                [
                    "cc",
                    "-std=c99",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-fsanitize=address,undefined",
                    "-I",
                    str(SOURCE),
                    '-DTERMIC_CONFIG="Config/"',
                    '-DTERMIC_ENV="Env/"',
                    str(SOURCE / "files.c"),
                    str(SOURCE / "tests/test_preferences.c"),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], cwd=work, check=True)

    def test_syntax_colours(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "syntax")
            subprocess.run(
                [
                    "cc",
                    "-std=c99",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-fsanitize=address,undefined",
                    "-I",
                    str(SOURCE),
                    str(SOURCE / "syntax.c"),
                    str(SOURCE / "tests/test_syntax.c"),
                    "-o",
                    binary,
                ],
                check=True,
            )
            subprocess.run([binary], check=True)

    def test_http_and_agent_loop(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            flags = [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Wno-deprecated-declarations",
                "-fsanitize=address,undefined",
                "-g",
                "-I",
                str(SOURCE),
            ]
            subprocess.run(
                flags
                + [
                    str(SOURCE / "http.c"),
                    str(SOURCE / "tests/test_http.c"),
                    "-o",
                    str(work / "http"),
                ],
                check=True,
            )
            subprocess.run([str(work / "http")], check=True)
            subprocess.run(
                flags
                + ['-DTERMIC_PROJECT="Project/"', '-DTERMIC_CONFIG="Config/"']
                + [
                    str(SOURCE / f)
                    for f in [
                        "agent.c",
                        "change_review.c",
                        "profiles.c",
                        "files.c",
                        "workspace.c",
                        "vendor/cJSON.c",
                        "tests/agent_harness.c",
                    ]
                ]
                + ["-o", str(work / "agent")],
                check=True,
            )
            (work / "Project").mkdir()
            (work / "Project/.agents/skills/termic").mkdir(parents=True)
            (work / "Config").mkdir()
            (work / "Config/model").write_text("fixture-model\n")
            (work / "Project/task.txt").write_text("Compile a synthetic fixture.\n")
            (work / "Project/build.log").write_text("Fixture compiler succeeded.\n")
            for mode in range(15):
                with self.subTest(mode=mode):
                    for journal in (work / "Project").glob(".review*"): journal.unlink()
                    (work / "Config/api-key").write_text(
                        "" if mode == 6 else "test-key\n"
                    )
                    (work / "Config/agents.json").unlink(missing_ok=True)
                    (work / "Project/agent-profile").write_text("openai")
                    original = "/* fixture source */\n"
                    (work / "Project/main.c").write_text(original)
                    (work / "Project/session.txt").unlink(missing_ok=True)
                    (work / "Project/util.h").unlink(missing_ok=True)
                    if mode == 10:
                        (work / "Project/session.txt").write_text("fixture_resume")
                    subprocess.run(
                        [str(work / "agent"), str(mode)], cwd=work, check=True
                    )
                    if mode == 7:
                        self.assertEqual(
                            (work / "Project/util.h").read_text(), "#define VALUE 2\n"
                        )
                    if mode == 0:
                        self.assertEqual(
                            (work / "Project/main.c.bak").read_text(), original
                        )
                        self.assertIn("return 0", (work / "Project/main.c").read_text())
                    else:
                        self.assertEqual(
                            (work / "Project/main.c").read_text(), original
                        )
                    self.assertFalse((work / "escape.c").exists())
                    self.assertNotIn(
                        "test-key", (work / "Project/transcript.txt").read_text()
                    )

            subprocess.run([str(work / "agent"), "15"], cwd=work, check=True)

    def test_workspace_files(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            subprocess.run(
                [
                    "cc",
                    "-std=c99",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-fsanitize=address,undefined",
                    "-g",
                    "-I",
                    str(SOURCE),
                ]
                + [
                    str(SOURCE / f)
                    for f in ["workspace.c", "files.c", "tests/test_workspace.c"]
                ]
                + ["-o", str(work / "workspace")],
                check=True,
            )
            subprocess.run([str(work / "workspace")], cwd=work, check=True)


if __name__ == "__main__":
    unittest.main()
