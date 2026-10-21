import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]


class CLITests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.temp.name) / ("xmlheap.exe" if os.name == "nt" else "xmlheap")
        subprocess.run(
            [os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra",
             "-Werror", "-pedantic", str(ROOT / "src/main.c"),
             "-o", str(cls.binary)], check=True
        )

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def validate_text(self, text):
        path = Path(self.temp.name) / "input.xml"
        path.write_text(text, encoding="utf-8")
        return subprocess.run([str(self.binary), "validate", str(path)],
                              text=True, capture_output=True)

    def test_nested_comments_cdata_and_attributes(self):
        result = self.validate_text(
            '<?xml version="1.0"?><root><x a=">">hi &amp; ok</x>'
            '<!-- comment --><![CDATA[<not-a-tag>]]><empty/></root>'
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_errors(self):
        for text in ("<a><b></a>", "<a/><b/>", "<a>", "text<a/>",
                     "<a><!-- missing", "<a attr='unterminated>"):
            with self.subTest(text=text):
                self.assertNotEqual(self.validate_text(text).returncode, 0)

    def test_heap_is_valid_xml_and_obeys_max_heap_property(self):
        result = subprocess.run(
            [str(self.binary), "heap", str(ROOT / "examples/numbers.txt")],
            text=True, capture_output=True, check=True
        )
        tree = ET.fromstring(result.stdout)
        values = [int(node.attrib["value"]) for node in tree.iter("node")]
        self.assertCountEqual(values, [4, 9, 1, 7, 3, 6])
        for parent in tree.iter("node"):
            for child in parent:
                self.assertGreaterEqual(int(parent.attrib["value"]),
                                        int(child.attrib["value"]))
        self.assertEqual(self.validate_text(result.stdout).returncode, 0)


if __name__ == "__main__":
    unittest.main()

