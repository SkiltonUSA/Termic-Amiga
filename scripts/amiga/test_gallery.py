"""A missing assign must fail before CLI acceptance can open a volume requester."""
import unittest
from accept_gallery import require_cli_assign

class GalleryPreflightTests(unittest.TestCase):
    def test_recognizes_case_insensitive_termic_assign(self):
        require_cli_assign('[OK]\nTermic         Work:Termic-Amiga\n')
        require_cli_assign('TERMIC\tWork:Termic-Amiga\n')

    def test_missing_or_similarly_named_assign_is_rejected(self):
        for reply in ('[OK]\n', 'TermicBackup Work:Archive\n', 'Termic\n'):
            with self.assertRaisesRegex(RuntimeError, 'no test job was launched'):
                require_cli_assign(reply)

if __name__ == '__main__':
    unittest.main()
