#!/usr/bin/env python3

#
# gradle_plugin.py
#
# Copyright 2018 danigm <danigm@wadobo.com>
# Copyright 2018 Alberto Fanjul <albfan@gnome.org>
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
import threading
import os

gi.require_version('Ide', '1.0')

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Ide

_ = Ide.gettext


class GradleBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'gradle'

    def do_get_display_name(self):
        return 'Gradle'

    def do_init_async(self, io_priority, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)

        try:
            # Maybe this is a gradlew
            if self.props.project_file.get_basename() in ('build.gradle',):
                task.return_boolean(True)
                return

            # Maybe this is a directory with a gradlew
            if self.props.project_file.query_file_type(0) == Gio.FileType.DIRECTORY:
                child = self.props.project_file.get_child('build.gradle')
                if child.query_exists(None):
                    self.props.project_file = child
                    task.return_boolean(True)
                    return
        except Exception as ex:
            task.return_error(ex)

        task.return_error(Ide.NotSupportedError())

    def do_init_finish(self, task):
        return task.propagate_boolean()

    def do_get_priority(self):
        return -200

class GradlePipelineAddin(Ide.Object, Ide.BuildPipelineAddin):
    """
    The GradlePipelineAddin is responsible for creating the necessary build
    stages and attaching them to phases of the build pipeline.
    """

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = context.get_build_system()

        if type(build_system) != GradleBuildSystem:
            return

        config = pipeline.get_configuration()
        builddir = pipeline.get_builddir()
        runtime = config.get_runtime()
        srcdir = pipeline.get_srcdir()

        if not runtime.contains_program_in_path('gradle'):
            raise OSError('The runtime must contain gradle to build gradle projects')

        wrapper_launcher = pipeline.create_launcher()
        wrapper_launcher.set_cwd(srcdir)
        wrapper_launcher.push_argv("gradle")
        wrapper_launcher.push_argv('wrapper')
        self.track(pipeline.connect_launcher(Ide.BuildPhase.AUTOGEN, 0, wrapper_launcher))

        build_launcher = pipeline.create_launcher()
        build_launcher.set_cwd(srcdir)
        build_launcher.push_argv("./gradlew")
        build_launcher.push_argv('build')

        clean_launcher = pipeline.create_launcher()
        clean_launcher.set_cwd(srcdir)
        clean_launcher.push_argv("./gradlew")
        clean_launcher.push_argv('clean')

        build_stage = Ide.BuildStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Building project"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.connect(Ide.BuildPhase.BUILD, 0, build_stage))

    def _query(self, stage, pipeline, cancellable):
        stage.set_completed(False)

class GradleBuildTarget(Ide.Object, Ide.BuildTarget):

    def do_get_install_directory(self):
        return None

    def do_get_name(self):
        return 'gradle-run'

    def do_get_language(self):
        return 'java'

    def do_get_cwd(self):
        context = self.get_context()
        project_file = context.get_project_file()
        return project_file.get_parent().get_path()

    def do_get_argv(self):
        context = self.get_context()
        project_file = context.get_project_file()
        path = project_file.get_parent().get_path()
        return [path + "/gradlew", "run"]

    def do_get_priority(self):
        return 0

class GradleBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = context.get_build_system()

        if type(build_system) != GradleBuildSystem:
            task.return_error(GLib.Error('Not gradle build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [GradleBuildTarget(context=self.get_context())]
        task.return_boolean(True)

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets
