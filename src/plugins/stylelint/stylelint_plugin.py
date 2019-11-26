#!/usr/bin/env python3

#
# __init__.py
#
# Copyright 2017 Georg Vienna <georg.vienna@himbarsoft.com>
# Copyright 2017 Tobias Schönberg <tobias47n9e@gmail.com>
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
import threading

from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Gtk
from gi.repository import Ide

_ = Ide.gettext


SEVERITY_MAP = {
    "warning": Ide.DiagnosticSeverity.WARNING,
    "error": Ide.DiagnosticSeverity.ERROR
}


class StylelintDiagnosticProvider(Ide.Object, Ide.DiagnosticProvider):
    @staticmethod
    def _get_stylelint(srcdir):
        local_stylelint = os.path.join(srcdir, 'node_modules', '.bin', 'stylelint')
        if os.path.exists(local_stylelint):
            return local_stylelint
        else:
            return 'stylelint'  # Just rely on PATH

    def create_launcher(self):
        context = self.get_context()
        srcdir = context.ref_workdir().get_path()
        launcher = None

        if context.has_project():
            build_manager = Ide.BuildManager.from_context(context)
            pipeline = build_manager.get_pipeline()
            if pipeline is not None:
                srcdir = pipeline.get_srcdir()
            runtime = pipeline.get_config().get_runtime()
            launcher = runtime.create_launcher()

        if launcher is None:
            launcher = Ide.SubprocessLauncher.new(0)

        launcher.set_flags(Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDOUT_PIPE)
        launcher.set_cwd(srcdir)

        return launcher

    def do_diagnose_async(self, file, file_content, lang_id, cancellable, callback, user_data):
        self.diagnostics_list = []
        task = Gio.Task.new(self, cancellable, callback)
        task.diagnostics_list = []

        launcher = self.create_launcher()
        srcdir = launcher.get_cwd()

        threading.Thread(target=self.execute, args=(task, launcher, srcdir, file, file_content),
                         name='stylelint-thread').start()

    def execute(self, task, launcher, srcdir, file, file_content):
        try:
            launcher.push_args((self._get_stylelint(srcdir), '--formatter', 'json'))

            if file_content:
                launcher.push_argv('--stdin-filename=' + file.get_path())
            else:
                launcher.push_argv(file.get_path())

            sub_process = launcher.spawn()
            stdin = file_content.get_data().decode('UTF-8')
            success, stdout, stderr = sub_process.communicate_utf8(stdin, None)

            if not success:
                task.return_boolean(False)
                return

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
                    task.diagnostics_list.append(diagnostic)
        except GLib.Error as err:
            task.return_error(err)
        except (json.JSONDecodeError, UnicodeDecodeError, IndexError) as e:
            task.return_error(GLib.Error('Failed to decode stylelint json: {}'.format(e)))
        else:
            task.return_boolean(True)

    def do_diagnose_finish(self, result):
        if result.propagate_boolean():
            diagnostics = Ide.Diagnostics()
            for diag in result.diagnostics_list:
                diagnostics.add(diag)
            return diagnostics


class StylelintPreferencesAddin(GObject.Object, Ide.PreferencesAddin):
    def do_load(self, preferences):
        self.stylelint = preferences.add_switch("code-insight",
                                             "diagnostics",
                                             "org.gnome.builder.plugins.stylelint",
                                             "enable-stylelint",
                                             None,
                                             "false",
                                             _("Stylelint"),
                                             _("Enable the use of Stylelint, which may execute code in your project"),
                                             # translators: these are keywords used to search for preferences
                                             _("stylelint stylesheet lint code execute execution"),
                                             500)

    def do_unload(self, preferences):
        preferences.remove_id(self.stylelint)
