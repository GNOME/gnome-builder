#!/usr/bin/env python3

#
# __init__.py
#
# Copyright (C) 2017 Georg Vienna <georg.vienna@himbarsoft.com>
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
import re

gi.require_version('Ide', '1.0')

from gi.repository import Dzl
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Gtk
from gi.repository import Ide

import threading

_ = Ide.gettext

def severtity_from_eslint(severity):
    if 'Warning' in severity:
        return Ide.DiagnosticSeverity.WARNING
    # eslint has only warning and error, so default to error
    return Ide.DiagnosticSeverity.ERROR

class ESLintDiagnosticProvider(Ide.Object, Ide.DiagnosticProvider):
    def do_load(self):
        self.diagnostics_list = []

    def do_diagnose_async(self, file, buffer, cancellable, callback, user_data):
        self.diagnostics_list = []
        task = Gio.Task.new(self, cancellable, callback)

        unsaved_files = self.get_context().get_unsaved_files()
        unsaved_file = unsaved_files.get_unsaved_file (file.get_file ())
        if unsaved_file:
            file_content = unsaved_file.get_content().get_data().decode('utf-8')
        else:
            file_content = None

        settings = Gio.Settings.new('org.gnome.builder.plugins.eslint')
        if not settings.get_boolean('enable-eslint'):
            task.return_boolean(True)
        else:
            threading.Thread(target=self.execute, args=[task, file, file_content], name='eslint-thread').start()


    def execute(self, task, file, file_content):
        try:
            launcher = Ide.SubprocessLauncher.new(Gio.SubprocessFlags.STDOUT_PIPE|Gio.SubprocessFlags.STDIN_PIPE)
            launcher.push_argv('eslint')
            launcher.push_argv('-f')
            launcher.push_argv('compact')
            if file_content:
                launcher.push_argv('--stdin')
                launcher.push_argv('--stdin-filename=' + file.get_path())
            else:
                launcher.push_argv(file.get_path())

            sub_process = launcher.spawn()

            result, stdout, stderr = sub_process.communicate_utf8(file_content, None)

            for line in iter(stdout.splitlines()):
                m = re.search('.*: line (\d+), col (\d+), (.*) - (.*)', line)
                if m is None:
                    break
                line_number = max(0, int(m.group(1)) - 1)
                column_number = max(0, int(m.group(2)) - 1)
                severity = severtity_from_eslint(m.group(3))
                message = m.group(4)
                source_location = Ide.SourceLocation.new(file, line_number, column_number, 0)
                self.diagnostics_list.append(Ide.Diagnostic.new(severity, message, source_location))
        except Exception as e:
            pass
        task.return_boolean(True)

    def do_diagnose_finish(self, result):
        return Ide.Diagnostics.new(self.diagnostics_list)

class ESLintPreferencesAddin(GObject.Object, Ide.PreferencesAddin):
    def do_load(self, preferences):
        self.eslint = preferences.add_switch("code-insight",
                                             "diagnostics",
                                             "org.gnome.builder.plugins.eslint",
                                             "enable-eslint",
                                             None,
                                             "false",
                                             _("ESlint"),
                                             _("Enable the use of ESLint, which may execute code in your project"),
                                             # translators: these are keywords used to search for preferences
                                             _("eslint javascript lint code execute execution"),
                                             500)
    def do_unload(self, preferences):
        preferences.remove_id(self.eslint)
