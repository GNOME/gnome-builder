#
# __init__.py
#
# Copyright 2017 Christian Hergert <chergert@redhat.com>
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
import os

gi.require_version('Ide', '1.0')

from gi.repository import Ide
from gi.repository import GLib
from gi.repository import GObject

_ = Ide.gettext

class ValgrindWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):
    workbench = None
    has_handler = False
    notify_handler = None

    def do_load(self, workbench):
        self.workbench = workbench

        build_manager = workbench.get_context().get_build_manager()
        self.notify_handler = build_manager.connect('notify::pipeline', self.notify_pipeline)
        self.notify_pipeline(build_manager, None)

    def notify_pipeline(self, build_manager, pspec):
        run_manager = self.workbench.get_context().get_run_manager()

        # When the pipeline changes, we need to check to see if we can find
        # valgrind inside the runtime environment.
        pipeline = build_manager.get_pipeline()
        if pipeline is not None:
            runtime = pipeline.get_configuration().get_runtime()
            if runtime and runtime.contains_program_in_path('valgrind'):
                if not self.has_handler:
                    run_manager.add_handler('valgrind', _('Run with Valgrind'), 'system-run-symbolic', '<primary>F10', self.valgrind_handler)
                    self.has_handler = True
                return

        if self.has_handler:
            run_manager.remove_handler('valgrind')

    def do_unload(self, workbench):
        build_manager = workbench.get_context().get_build_manager()
        build_manager.disconnect(self.notify_handler)
        self.notify_handler = None

        if self.has_handler:
            run_manager = workbench.get_context().get_run_manager()
            run_manager.remove_handler('valgrind')

        self.workbench = None

    def valgrind_handler(self, run_manager, runner):
        # We want to run with valgrind --log-fd=N so that we get the valgrind
        # output redirected to our temp file. Then when the process exits, we
        # we will open the temp file in the builder editor.
        source_fd, name = GLib.file_open_tmp('gnome-builder-valgrind-XXXXXX.txt')
        map_fd = runner.take_fd(source_fd, -1)
        runner.prepend_argv('--track-origins=yes')
        runner.prepend_argv('--log-fd='+str(map_fd))
        runner.prepend_argv('valgrind')
        runner.connect('exited', self.runner_exited, name)

    def runner_exited(self, runner, name):
        # If we weren't unloaded in the meantime, we can open the file using
        # the "editor" hint to ensure the editor opens the file.
        if self.workbench:
            uri = Ide.Uri.new('file://'+name, 0)
            self.workbench.open_uri_async(uri, 'editor', 0, None, None, None)
