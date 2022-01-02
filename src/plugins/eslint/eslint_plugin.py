#!/usr/bin/env python3

#
# __init__.py
#
# Copyright 2017 Georg Vienna <georg.vienna@himbarsoft.com>
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
    1: Ide.DiagnosticSeverity.WARNING,
    2: Ide.DiagnosticSeverity.ERROR
}

# Comes from typescript-language-server
BUNDLED_ESLINT = '/app/lib/yarn/global/node_modules/typescript-language-server/node_modules/eslint/bin/eslint.js'

class ESLintDiagnosticProvider(Ide.Object, Ide.DiagnosticProvider):

    def create_launcher(self):
        flags = (Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_SILENCE)

        context = self.get_context()
        srcdir = context.ref_workdir().get_path()

        build_manager = None
        pipeline = None
        launcher = None
        host = None

        # We prefer to use the eslint from the projeects node_modules
        local_eslint = os.path.join(srcdir, 'node_modules', '.bin', 'eslint')

        # If we have a project, use the pipeline to access the build container
        if context.has_project():
            build_manager = Ide.BuildManager.from_context(context)
            pipeline = build_manager.get_pipeline()
            host = Ide.RuntimeManager.from_context(context).get_runtime('host')
            srcdir = pipeline.get_srcdir()

        if os.path.exists(local_eslint):
            # If we have a project, use the build container to execute
            if pipeline is not None:
                launcher = pipeline.create_launcher()
                launcher.set_flags(flags)
                launcher.set_cwd(srcdir)
                launcher.push_argv(local_eslint)
                return launcher

            # There is no project, so just try to execute within the host
            # environment since that is likely where things were installed
            # and likely need access to host libraries/etc at known
            # locations/paths.
            if host is not None:
                launcher = host.create_launcher()
                launcher.set_flags(flags)
            else:
                launcher = Ide.SubprocessLauncher.new(flags)
                launcher.set_run_on_host(True)
            launcher.set_cwd(srcdir)
            launcher.push_argv(local_eslint)
            return launcher

        # At this point we want to see if we can run 'eslint' on the host
        # since the developer does not have eslint setup within their
        # node_modules directory. We can only ensure this if a project
        # is loaded, otherwise we'll have to fallback to something bundled.
        if host is not None and host.contains_program_in_path('eslint', None):
            launcher = host.create_launcher()
            launcher.set_flags(flags)
            launcher.set_cwd(srcdir)
            launcher.push_argv('eslint')
            return launcher

        # We can hit this if we're not running in Flatpak or if we
        # have eslint bundled (we do but not in $PATH under flatpak).
        if GLib.find_program_in_path('eslint'):
            launcher = Ide.SubprocessLauncher.new(flags)
            launcher.set_cwd(srcdir)
            launcher.push_argv('eslint')
            return launcher

        # Okay, last resort. Try to get this thing working from our
        # bundled typescript-language-server.
        if os.path.exists('/.flatpak-info'):
            launcher = Ide.SubprocessLauncher.new(flags)
            launcher.set_cwd(srcdir)
            launcher.push_argv(BUNDLED_ESLINT)
            return launcher

        # Meh, not much hope, but give a launcher anyway
        launcher = Ide.SubprocessLauncher.new(flags)
        launcher.set_cwd(srcdir)
        launcher.push_argv('eslint')
        return launcher

    def do_diagnose_async(self, file, file_content, lang_id, cancellable, callback, user_data):
        self.diagnostics_list = []
        task = Gio.Task.new(self, cancellable, callback)
        task.diagnostics_list = []

        launcher = self.create_launcher()
        srcdir = launcher.get_cwd()

        threading.Thread(target=self.execute, args=(task, launcher, srcdir, file, file_content),
                         name='eslint-thread').start()

    def execute(self, task, launcher, srcdir, file, file_content):
        try:
            launcher.push_args(('-f', 'json',
                                '--ignore-pattern', '!node_modules/*',
                                '--ignore-pattern', '!bower_components/*'))

            if file_content:
                launcher.push_argv('--stdin')
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

                    task.diagnostics_list.append(diagnostic)
        except GLib.Error as err:
            task.return_error(err)
        except (json.JSONDecodeError, UnicodeDecodeError, IndexError) as e:
            task.return_error(GLib.Error('Failed to decode eslint json: {}'.format(e)))
        else:
            task.return_boolean(True)

    def do_diagnose_finish(self, result):
        if result.propagate_boolean():
            diagnostics = Ide.Diagnostics()
            for diag in result.diagnostics_list:
                diagnostics.add(diag)
            return diagnostics


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
