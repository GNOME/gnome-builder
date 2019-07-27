#
# valgrind_plugin.py
#
# Copyright 2017-2018 Christian Hergert <chergert@redhat.com>
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

from gi.repository import Ide
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject

_ = Ide.gettext

class ValgrindWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):
    build_manager = None
    workbench = None
    has_handler = False
    notify_handler = None

    def do_load(self, workbench):
        self.workbench = workbench

    def do_project_loaded(self, project_info):
        self.build_manager = Ide.BuildManager.from_context(self.workbench.get_context())
        self.notify_handler = self.build_manager.connect('notify::pipeline', self.notify_pipeline)
        self.notify_pipeline(self.build_manager, None)

    def notify_pipeline(self, build_manager, pspec):
        run_manager = Ide.RunManager.from_context(self.workbench.get_context())

        # When the pipeline changes, we need to check to see if we can find
        # valgrind inside the runtime environment.
        pipeline = build_manager.get_pipeline()
        if pipeline is not None:
            runtime = pipeline.get_config().get_runtime()
            if runtime and runtime.contains_program_in_path('valgrind'):
                if not self.has_handler:
                    run_manager.add_handler('valgrind', _('Run with Valgrind'), 'system-run-symbolic', '<primary>F10', self.valgrind_handler)
                    self.has_handler = True
                return

        if self.has_handler:
            run_manager.remove_handler('valgrind')

    def do_unload(self, workbench):
        if self.build_manager is not None:
            if self.notify_handler is not None:
                self.build_manager.disconnect(self.notify_handler)
                self.notify_handler = None

        if self.has_handler:
            run_manager = Ide.RunManager.from_context(workbench.get_context())
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
            gfile = Gio.File.new_for_path(name)
            self.workbench.open_async(gfile, 'editor', 0, None, None, None)
