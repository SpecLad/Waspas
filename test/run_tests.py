#!/usr/bin/env python3

import re
import os
import subprocess
import unittest

from dataclasses import dataclass
from pathlib import Path

EXE_PATH = None
TEST_CASE_DIR = Path(__file__).resolve().parent

ERROR_MESSAGE_RE = re.compile(r'^(.+?):(\d+):(\d+): .+ \(([a-z-]+)\)$')

@dataclass
class ErrorMessage:
    line_num: int
    column_num: int
    error_code: str

class TestBasicErrors(unittest.TestCase):
    def _test(self, args):
        cp = subprocess.run(
            [str(EXE_PATH), *args], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        self.assertEqual(cp.returncode, 1);

    def test_no_args(self):
        self._test([])

    def test_bad_path(self):
        self._test(['--', str(TEST_CASE_DIR / 'nonexistent.pas')])

    def test_unknown_arg(self):
        self._test(['-x'])

class TestErrorMessages(unittest.TestCase):
    def try_compile_ill_formed_source(self, source_name):
        error_messages = []

        file_path_arg = str(TEST_CASE_DIR / source_name)

        with subprocess.Popen(
            [str(EXE_PATH), '--', file_path_arg],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
        ) as process:
            for line in process.stderr:
                line = line.rstrip('\n')

                self.assertRegex(line, ERROR_MESSAGE_RE)
                match = ERROR_MESSAGE_RE.search(line)
                
                file_path, line_num, column_num, error_code = match.groups()
                self.assertEqual(file_path, file_path_arg)
                error_messages.append(ErrorMessage(
                    line_num=int(line_num), column_num=int(column_num), error_code=error_code))

        self.assertEqual(process.returncode, 1)

        return error_messages

    def test_non_ascii_char(self):
        self.assertIn(ErrorMessage(2, 1, 'non-ascii-char'),
            self.try_compile_ill_formed_source('non_ascii.pas'))

    def test_invalid_token(self):
        messages = self.try_compile_ill_formed_source('bad_tokens.pas')

        self.assertIn(ErrorMessage(1, 1, 'invalid-token'), messages)
        self.assertIn(ErrorMessage(3, 1, 'invalid-token'), messages)

        # The locus is 4:4 instead of 4:1 here, because when the lexer fails to
        # lex the entire number as a real, it lexes the integer part as an integer,
        # and the dot as the operator, and only then fails on the fractional part.
        # The lexer could be improved to fail at 4:1.
        self.assertIn(ErrorMessage(4, 4, 'invalid-token'), messages)

        self.assertIn(ErrorMessage(5, 1, 'invalid-token'), messages)
        self.assertIn(ErrorMessage(6, 1, 'invalid-token'), messages)
        self.assertIn(ErrorMessage(7, 26, 'invalid-token'), messages)
        self.assertIn(ErrorMessage(8, 1, 'invalid-token'), messages)

    def test_invalid_token(self):
        messages = self.try_compile_ill_formed_source('empty.pas')

        self.assertIn(ErrorMessage(1, 1, 'unexpected-token'), messages)

    def test_invalid_label(self):
        messages = self.try_compile_ill_formed_source('bad_label_1.pas')

        self.assertIn(ErrorMessage(2, 7, 'invalid-label'), messages)

        messages = self.try_compile_ill_formed_source('bad_label_2.pas')

        self.assertIn(ErrorMessage(2, 7, 'invalid-label'), messages)

    def test_invalid_int(self):
        messages = self.try_compile_ill_formed_source('bad_int.pas')

        self.assertIn(ErrorMessage(2, 11, 'invalid-integer'), messages)

    def test_invalid_label_declaration(self):
        messages = self.try_compile_ill_formed_source('bad_label_declaration.pas')

        self.assertIn(ErrorMessage(2, 12, 'unexpected-token'), messages)


if __name__ == '__main__':
    EXE_PATH = Path(os.environ['WASPAS_TEST_EXE_PATH'])
    unittest.main()
