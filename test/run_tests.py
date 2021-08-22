#!/usr/bin/env python3

import re
import os
import subprocess
import unittest

from dataclasses import dataclass
from pathlib import Path

EXE_PATH = None
TEST_CASE_DIR = Path(__file__).resolve().parent

ERROR_MESSAGE_RE = re.compile(r'^(.+?):(\d+):(\d+): error: .+ \(([a-z-]+)\)$')
NOTE_MESSAGE_RE = re.compile(r'^(.+?):(\d+):(\d+): note: .+$')

@dataclass
class ErrorMessage:
    line_num: int
    column_num: int
    error_code: str

@dataclass
class NoteMessage:
    line_num: int
    column_num: int

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
    def try_compile_ill_formed_source(self, source_path):
        diagnostics = []

        file_path_arg = str(TEST_CASE_DIR / source_path)

        with subprocess.Popen(
            [str(EXE_PATH), '--', file_path_arg],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
        ) as process:
            for line in process.stderr:
                line = line.rstrip('\n')

                if match := ERROR_MESSAGE_RE.search(line):
                    file_path, line_num, column_num, error_code = match.groups()
                    self.assertEqual(file_path, file_path_arg)
                    diagnostics.append(ErrorMessage(
                        line_num=int(line_num), column_num=int(column_num), error_code=error_code))
                elif match := NOTE_MESSAGE_RE.search(line):
                    file_path, line_num, column_num = match.groups()
                    self.assertEqual(file_path, file_path_arg)
                    diagnostics.append(NoteMessage(
                        line_num=int(line_num), column_num=int(column_num)))
                else:
                    self.fail(f"stderr line {line!r} is not a valid diagnostic")

        self.assertEqual(process.returncode, 1)

        return diagnostics

    def get_expected_diagnostics_from_source(self, source_path):
        expected_diagnostics = []

        with open(source_path) as f:
            for i, line in enumerate(f):
                for match in re.finditer(r'\{\^(.*?)\}', line):
                    directive_text = match.group(1).strip()
                    locus_args = {'line_num': i, 'column_num': match.start() + 2}

                    if directive_text.startswith('error:'):
                        expected_diagnostics.append(ErrorMessage(
                            **locus_args,
                            error_code=directive_text.removeprefix('error:'),
                        ))
                    elif directive_text == 'note':
                        expected_diagnostics.append(NoteMessage(**locus_args))
                    else:
                        raise RuntimeError(f'unrecognized directive: {match.group()}')

        return expected_diagnostics

    def test_auto(self):
        for source_path in (TEST_CASE_DIR / 'ill-formed/').glob('**/*.pas'):
            with self.subTest(source_path=source_path):
                expected_diagnostics = self.get_expected_diagnostics_from_source(source_path)
                actual_diagnostics = self.try_compile_ill_formed_source(source_path)

                for d in expected_diagnostics:
                    self.assertIn(d, actual_diagnostics)

    def test_unexpected_eof(self):
        messages = self.try_compile_ill_formed_source('empty.pas')

        self.assertIn(ErrorMessage(1, 1, 'unexpected-token'), messages)


if __name__ == '__main__':
    EXE_PATH = Path(os.environ['WASPAS_TEST_EXE_PATH'])
    unittest.main()
