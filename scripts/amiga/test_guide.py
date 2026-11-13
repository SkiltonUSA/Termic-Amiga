"""Check the shipped AmigaGuide graph, not just its file extension."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class GuideTests(unittest.TestCase):
    def test_nodes_links_and_packaging(self):
        guide = (ROOT / "amiga/native/Termic.guide").read_text(encoding="ascii")
        self.assertTrue(guide.startswith("@DATABASE Termic.guide\n"))
        nodes = []
        current = None
        edges = {}
        for line in guide.splitlines():
            if line.startswith("@NODE "):
                self.assertIsNone(current, "nested node")
                current = line.split()[1].lower()
                self.assertNotIn(current, nodes)
                nodes.append(current)
                edges[current] = []
            elif line.startswith("@ENDNODE"):
                self.assertIsNotNone(current, "unpaired ENDNODE")
                current = None
            for link in re.findall(r'@\{"[^"]+" LINK ([A-Za-z0-9_]+)\}', line):
                self.assertIsNotNone(current)
                edges[current].append(link.lower())
            self.assertEqual(line.count("@{"), line.count("}"), line)
        self.assertIsNone(current)
        self.assertEqual(nodes[0], "main")
        self.assertIn("index", nodes)
        self.assertIn("@INDEX Index", guide)
        version = re.search(
            r'#define TERMIC_VERSION "([^"]+)"',
            (ROOT / "amiga/native/version.h").read_text(),
        ).group(1)
        self.assertIn("Termic Amiga " + version, guide)
        visited, pending = set(), ["main"]
        while pending:
            node = pending.pop()
            self.assertIn(node, nodes, "broken link: " + node)
            if node not in visited:
                visited.add(node)
                pending.extend(edges[node])
        self.assertEqual(visited, set(nodes), "unreachable manual topic")
        self.assertNotRegex(guide, r"@\{[^}]*\b(?:SYSTEM|RX|RXS)\b")
        self.assertIn(
            '"Termic.guide",', (ROOT / "scripts/amiga/package_native.py").read_text()
        )
