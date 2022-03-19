#!/usr/bin/env python3

#
# __init__.py
#
# Copyright 2021 Jeremy Wilkins <jeb@jdwilkins.co.uk>
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

import gi
import json

from gi.repository import Gtk
from gi.repository import Ide


SEVERITY_MAP = {
    'info': Ide.DiagnosticSeverity.NOTE,
    'refactor': Ide.DiagnosticSeverity.NOTE,
    'convention': Ide.DiagnosticSeverity.NOTE,
    'warning': Ide.DiagnosticSeverity.WARNING,
    'error': Ide.DiagnosticSeverity.ERROR,
    'fatal': Ide.DiagnosticSeverity.FATAL
}

class RubocopDiagnosticProvider(Ide.DiagnosticTool):
    is_stdin = False

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_program_name('rubocop')

    def do_configure_launcher(self, launcher, file, contents):
        launcher.push_args(('--format', 'json'))
        if contents is not None:
            self.is_stdin = True
            launcher.push_argv('--stdin')
        launcher.push_argv(file.get_path())

    def do_populate_diagnostics(self, diagnostics, file, stdout, stderr):
        try:
            results = json.loads(stdout)
            for result in results.get('files', []):
                for offense in result.get('offenses', []):
                    if 'location' not in offense:
                        continue

                    location = offense['location']

                    if 'start_line' not in location or 'start_column' not in location:
                        continue

                    start_line = max(location['start_line'] - 1, 0)
                    start_col = max(location['start_column'] - 1, 0)
                    start = Ide.Location.new(file, start_line, start_col)

                    end = None
                    if 'last_line' in location:
                        end_line = max(location['last_line'] - 1, 0)
                        end_col = max(location['last_column'], 0)
                        end = Ide.Location.new(file, end_line, end_col)
                    elif 'length' in location:
                        end_line = start_line
                        end_col = start_col + location['length']
                        end = Ide.Location.new(file, end_line, end_col)

                    if self.is_stdin:
                        message = offense['cop_name'] + ': ' + offense['message']
                    else:
                        message = offense['message'] # Already prefixed when not --stdin

                    severity = SEVERITY_MAP[offense['severity']]
                    diagnostic = Ide.Diagnostic.new(severity, message, start)
                    if end is not None:
                        range_ = Ide.Range.new(start, end)
                        diagnostic.add_range(range_)

                    diagnostics.add(diagnostic)
        except Exception as e:
            Ide.warning('Failed to decode rubocop json: {}'.format(e))

