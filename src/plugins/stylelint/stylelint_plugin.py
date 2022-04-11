#!/usr/bin/env python3

#
# __init__.py
#
# Copyright 2017 Georg Vienna <georg.vienna@himbarsoft.com>
# Copyright 2017 Tobias Schönberg <tobias47n9e@gmail.com>
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
    "warning": Ide.DiagnosticSeverity.WARNING,
    "error": Ide.DiagnosticSeverity.ERROR
}

class StylelintDiagnosticProvider(Ide.DiagnosticTool):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_program_name('stylelint')
        self.set_local_program_path(os.path.join('node_modules', '.bin', 'stylelint'))

    def do_configure_launcher(self, launcher, file, contents, language_id):
        launcher.push_args(('--formatter', 'json'))
        if contents is not None:
            launcher.push_args(('--stdin', '--stdin-filename=' + file.get_path()))
        else:
            launcher.push_argv(file.get_path())

    def do_populate_diagnostics(self, diagnostics, file, stdout, stderr):
        try:
            results = json.loads(stdout)
            for result in results:
                for message in result.get('warnings', []):
                    if 'line' not in message or 'column' not in message:
                        continue
                    start_line = max(message['line'] - 1, 0)
                    start_col = max(message['column'] - 1, 0)
                    start = Ide.Location.new(file, start_line, start_col)
                    severity = SEVERITY_MAP[message['severity']]
                    diagnostic = Ide.Diagnostic.new(severity, message['text'], start)
                    diagnostics.add(diagnostic)
        except Exception as e:
            Ide.warning('Failed to decode stylelint json: {}'.format(e))
