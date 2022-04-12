#!/usr/bin/env python3

#
# __init__.py
#
# Copyright 2017 Georg Vienna <georg.vienna@himbarsoft.com>
# Copyright 2022 Veli Tasalı <me@velitasali.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import os
import gi
import json

from gi.repository import GObject
from gi.repository import Ide

SEVERITY_MAP = {
    1: Ide.DiagnosticSeverity.WARNING,
    2: Ide.DiagnosticSeverity.ERROR
}

# Comes from typescript-language-server
BUNDLED_ESLINT = '/app/lib/yarn/global/node_modules/typescript-language-server/node_modules/eslint/bin/eslint.js'

class ESLintDiagnosticProvider(Ide.DiagnosticTool):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_program_name('eslint')
        self.set_bundled_program_path(BUNDLED_ESLINT)
        self.set_local_program_path(os.path.join('node_modules', '.bin', 'eslint'))

    def do_configure_launcher(self, launcher, file, contents, language_id):
        launcher.push_args(('-f', 'json',
            '--ignore-pattern', '!node_modules/*',
            '--ignore-pattern', '!bower_components/*'))
        if contents is not None:
            launcher.push_args(('--stdin', '--stdin-filename=' + file.get_path()))
        else:
            launcher.push_argv(file.get_path())

    def do_populate_diagnostics(self, diagnostics, file, stdout, stderr):
        try:
            results = json.loads(stdout)
            for result in results:
                for message in result.get('messages', []):
                    if 'line' not in message or 'column' not in message:
                        continue
                    start_line = max(message['line'] - 1, 0)
                    start_col = max(message['column'] - 1, 0)
                    start = Ide.Location.new(file, start_line, start_col)
                    end = None
                    if 'endLine' in message:
                        end_line = max(message['endLine'] - 1, 0)
                        end_col = max(message['endColumn'] - 1, 0)
                        end = Ide.Location.new(file, end_line, end_col)

                    severity = SEVERITY_MAP[message['severity']]
                    diagnostic = Ide.Diagnostic.new(severity, message['message'], start)
                    if end is not None:
                        range_ = Ide.Range.new(start, end)
                        diagnostic.add_range(range_)
                        # if 'fix' in message:
                        # Fixes often come without end* information so we
                        # will rarely get here, instead it has a file offset
                        # which is not actually implemented in IdeSourceLocation
                        # fixit = Ide.Fixit.new(range_, message['fix']['text'])
                        # diagnostic.take_fixit(fixit)

                    diagnostics.add(diagnostic)
        except Exception as e:
            Ide.warning('Failed to decode eslint json: {}'.format(e))
