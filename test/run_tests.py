#!/usr/bin/env python3

# SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>
#
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import functools
import re
import os
import subprocess
import textwrap
import unittest

from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Protocol

if TYPE_CHECKING:
    from _typeshed import StrPath

EXE_PATH: Path
TEST_CASE_DIR = Path(__file__).resolve().parent

ERROR_MESSAGE_RE = re.compile(r'^(.+?):(\d+):(\d+): error: .+ \(([a-z-]+)\)$')
NOTE_MESSAGE_RE = re.compile(r'^(.+?):(\d+):(\d+): note: .+$')

@dataclass
class DiagnosticMessage:
    line_num: int
    column_num: int

class DiagnosticMessageFactory(Protocol):
    def __call__(self, *, line_num: int, column_num: int) -> DiagnosticMessage:
        ...

@dataclass
class ErrorMessage(DiagnosticMessage):
    error_code: str

@dataclass
class NoteMessage(DiagnosticMessage):
    pass

class TestBasicErrors(unittest.TestCase):
    def _test(self, args: list[str]) -> None:
        cp = subprocess.run(
            [str(EXE_PATH), *args], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        self.assertEqual(cp.returncode, 1);

    def test_no_path_default(self) -> None:
        self._test([])

    def test_no_path_dump_ast(self) -> None:
        self._test(['--dump-ast'])

    def test_bad_path(self) -> None:
        self._test(['--', str(TEST_CASE_DIR / 'nonexistent.pas')])

    def test_unknown_arg(self) -> None:
        self._test(['-x'])

class TestErrorMessages(unittest.TestCase):
    def try_compile_ill_formed_source(self, source_path: StrPath) -> list[DiagnosticMessage]:
        diagnostics: list[DiagnosticMessage] = []

        file_path_arg = str(TEST_CASE_DIR / source_path)

        with subprocess.Popen(
            [str(EXE_PATH), '--', file_path_arg],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
        ) as process:
            assert process.stderr
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

    def get_expected_diagnostics_from_source(self, source_path: StrPath) -> list[DiagnosticMessage]:
        expected_diagnostics: list[DiagnosticMessage] = []

        with open(source_path, encoding='UTF-8') as f:
            for line_num, line in enumerate(f):
                for match in re.finditer(r'\{(\^+)(.*?)\}', line):
                    num_diagnostics = len(match.group(1))
                    directive_text = match.group(2).strip()

                    diag_constructor: DiagnosticMessageFactory

                    if directive_text.startswith('error:'):
                        diag_constructor = functools.partial(
                            ErrorMessage,
                            error_code=directive_text.removeprefix('error:'),
                        )
                    elif directive_text == 'note':
                        diag_constructor = NoteMessage
                    else:
                        raise RuntimeError(f'unrecognized directive: {match.group()}')

                    for diag_num in range(num_diagnostics):
                        expected_diagnostics.append(diag_constructor(
                            line_num=line_num, column_num=match.start() + 2 + diag_num,
                        ))

        return expected_diagnostics

    def test_auto(self) -> None:
        for source_path in (TEST_CASE_DIR / 'ill-formed/').glob('**/*.pas'):
            with self.subTest(source_path=source_path):
                expected_diagnostics = self.get_expected_diagnostics_from_source(source_path)
                actual_diagnostics = self.try_compile_ill_formed_source(source_path)

                actual_diagnostics.sort(
                    key=lambda diag: (diag.line_num, diag.column_num),
                )
                # expected_diagnostics is already ordered by locus, since we get them
                # by scanning the source from beginning to end.

                self.assertEqual(actual_diagnostics, expected_diagnostics)

    def test_unexpected_eof(self) -> None:
        messages = self.try_compile_ill_formed_source('empty.pas')

        self.assertIn(ErrorMessage(1, 1, 'unexpected-token'), messages)

class TestDumpAst(unittest.TestCase):
    def test_minimal(self) -> None:
        file_path_arg = str(TEST_CASE_DIR / 'minimal.pas')

        cp = subprocess.run(
            [str(EXE_PATH), '--dump-ast', file_path_arg],
            stdout=subprocess.PIPE, text=True,
        )

        self.assertEqual(cp.returncode, 0)

        self.assertEqual(cp.stdout, textwrap.dedent('''\
            Program(7:1-9:5):
                name = Identifier(7:9-7:16): minimal
                parameter_declarations = []
                block = Block(8:1-9:4):
                    label_declarations = []
                    constant_definitions = []
                    type_definitions = []
                    variable_declarations = []
                    subroutine_declarations = []
                    statement = CompoundStatement(8:1-9:4):
                        statements = [
                            Statement(9:1-9:1):
                                label = []
                                unlabeled = EmptyStatement(9:1-9:1)
                        ]
        '''))

    def test_file_with_dash(self) -> None:
        cp = subprocess.run(
            [str(EXE_PATH.resolve()), '--dump-ast', '--', '-dash.pas'],
            stdout=subprocess.PIPE, text=True, cwd=TEST_CASE_DIR,
        )

        self.assertEqual(cp.returncode, 0)
        self.assertIn('dash', cp.stdout)

if __name__ == '__main__':
    EXE_PATH = Path(os.environ['WASPAS_TEST_EXE_PATH'])
    unittest.main()
