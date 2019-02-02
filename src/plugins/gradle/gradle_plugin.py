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

import threading
import os

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Ide

_ = Ide.gettext

_ATTRIBUTES = ",".join([
    Gio.FILE_ATTRIBUTE_STANDARD_NAME,
    Gio.FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
    Gio.FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON,
])

class GradleBuildSystemDiscovery(Ide.SimpleBuildSystemDiscovery):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.props.glob = 'build.gradle'
        self.props.hint = 'gradle_plugin'
        self.props.priority = 2000

class GradleBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'gradle'

    def do_get_display_name(self):
        return 'Gradle'

    def do_get_priority(self):
        return 2000

class GradlePipelineAddin(Ide.Object, Ide.PipelineAddin):
    """
    The GradlePipelineAddin is responsible for creating the necessary build
    stages and attaching them to phases of the build pipeline.
    """

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != GradleBuildSystem:
            return

        config = pipeline.get_config()
        builddir = pipeline.get_builddir()
        runtime = config.get_runtime()
        srcdir = pipeline.get_srcdir()

        if not runtime.contains_program_in_path('gradle'):
            raise OSError('The runtime must contain gradle to build gradle projects')

        wrapper_launcher = pipeline.create_launcher()
        wrapper_launcher.set_cwd(srcdir)
        wrapper_launcher.push_argv("gradle")
        wrapper_launcher.push_argv('wrapper')

        wrapper_stage = Ide.PipelineStageLauncher.new(context, wrapper_launcher)
        wrapper_stage.set_name(_('Gradle Wrapper'))
        self.track(pipeline.attach(Ide.PipelinePhase.AUTOGEN, 0, wrapper_stage))

        build_launcher = pipeline.create_launcher()
        build_launcher.set_cwd(srcdir)
        build_launcher.push_argv("./gradlew")
        build_launcher.push_argv('build')

        clean_launcher = pipeline.create_launcher()
        clean_launcher.set_cwd(srcdir)
        clean_launcher.push_argv("./gradlew")
        clean_launcher.push_argv('clean')

        build_stage = Ide.PipelineStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Building project"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.attach(Ide.PipelinePhase.BUILD, 0, build_stage))

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
        project_file = Ide.BuildSystem.from_context(context).project_file
        return project_file.get_parent().get_path()

    def do_get_argv(self):
        context = self.get_context()
        project_file = Ide.BuildSystem.from_context(context).project_file
        path = project_file.get_parent().get_path()
        return [path + "/gradlew", "run"]

    def do_get_priority(self):
        return 0

class GradleBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != GradleBuildSystem:
            task.return_error(GLib.Error('Not gradle build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [build_system.ensure_child_typed(GradleBuildTarget)]
        task.return_boolean(True)

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets

class GradleIdeTestProvider(Ide.TestProvider):

    def do_run_async(self, test, pipeline, cancellable, callback, data):
        task = Ide.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != GradleBuildSystem:
            task.return_error(GLib.Error('Not gradle build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [build_system.ensure_child_typed(GradleBuildTarget)]

        try:
            runtime = pipeline.get_runtime()
            runner = runtime.create_runner()
            if not runner:
               task.return_error(Ide.NotSupportedError())

            runner.set_flags(Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_PIPE)

            srcdir = pipeline.get_srcdir()
            runner.set_cwd(srcdir)

            commands = test.get_command()

            for command in commands:
               runner.append_argv(command)

            test.set_status(Ide.TestStatus.RUNNING)

            def run_test_cb(runner, result, data):
                try:
                    runner.run_finish(result)
                    test.set_status(Ide.TestStatus.SUCCESS)
                except:
                    test.set_status(Ide.TestStatus.FAILED)
                finally:
                    task.return_boolean(True)

            runner.run_async(cancellable, run_test_cb, task)
        except Exception as ex:
            task.return_error(ex)

    def do_run_finish(self, result):
        if result.propagate_boolean():
            return result.targets

    def do_reload(self):

        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != GradleBuildSystem:
            return

        # find all files in test directory
        build_manager = Ide.BuildManager.from_context(context)
        pipeline = build_manager.get_pipeline()
        srcdir = pipeline.get_srcdir()
        test_suite = Gio.File.new_for_path(os.path.join(srcdir, 'src/test/java'))
        test_suite.enumerate_children_async(_ATTRIBUTES,
                                            Gio.FileQueryInfoFlags.NONE,
                                            GLib.PRIORITY_LOW,
                                            None,
                                            self.on_enumerator_loaded,
                                            None)

    def on_enumerator_loaded(self, parent, result, data):
        try:
            enumerator = parent.enumerate_children_finish(result)
            info = enumerator.next_file(None)

            while info is not None:
                name = info.get_name()
                gfile = parent.get_child(name)
                if info.get_file_type() == Gio.FileType.DIRECTORY:
                    gfile.enumerate_children_async(_ATTRIBUTES,
                                                    Gio.FileQueryInfoFlags.NONE,
                                                    GLib.PRIORITY_LOW,
                                                    None,
                                                    self.on_enumerator_loaded,
                                                    None)
                else:
                    # TODO: Ask java through introspection for classes with
                    # TestCase and its public void methods or Annotation @Test
                    # methods
                    result, contents, etag = gfile.load_contents()
                    tests = [x for x in str(contents).split('\\n') if 'public void' in x]
                    tests = [v.replace("()", "").replace("public void","").strip() for v in tests]
                    classname=name.replace(".java", "")

                    for testname in tests:
                        # it has to be junit 4.x
                        command = ["./gradlew", "test", "--tests", "{}.{}".format(classname, testname)]

                        test = GradleTest(group=classname, id=classname+testname, display_name=testname)
                        test.set_command(command)
                        self.add(test)

                info = enumerator.next_file(None)

            enumerator.close()

        except Exception as ex:
            Ide.warning(repr(ex))

class GradleTest(Ide.Test):

    def get_command(self):
        return self.command

    def set_command(self, command):
        self.command = command
