#!/usr/bin/env python3

#
# maven_plugin.py
#
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

_ATTRIBUTES = ",".join([
    Gio.FILE_ATTRIBUTE_STANDARD_NAME,
    Gio.FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
    Gio.FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON,
])

class MavenBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'maven'

    def do_get_display_name(self):
        return 'Maven'

    def do_init_async(self, io_priority, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)

        try:
            # Maybe this is a pom.xml
            if self.props.project_file.get_basename() in ('pom.xml',):
                task.return_boolean(True)
                return

            # Maybe this is a directory with a pom.xml
            if self.props.project_file.query_file_type(0) == Gio.FileType.DIRECTORY:
                child = self.props.project_file.get_child('pom.xml')
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
        return 400

class MavenPipelineAddin(Ide.Object, Ide.BuildPipelineAddin):
    """
    The MavenPipelineAddin is responsible for creating the necessary build
    stages and attaching them to phases of the build pipeline.
    """

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = context.get_build_system()

        if type(build_system) != MavenBuildSystem:
            return

        config = pipeline.get_configuration()
        builddir = pipeline.get_builddir()
        runtime = config.get_runtime()
        srcdir = pipeline.get_srcdir()

        if not runtime.contains_program_in_path('mvn'):
            raise OSError('The runtime must contain mvn to build maven projects')

        build_launcher = pipeline.create_launcher()
        build_launcher.set_cwd(srcdir)
        build_launcher.push_argv("mvn")
        build_launcher.push_argv('compile')

        clean_launcher = pipeline.create_launcher()
        clean_launcher.set_cwd(srcdir)
        clean_launcher.push_argv("mvn")
        clean_launcher.push_argv('clean')

        build_stage = Ide.BuildStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Building project"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.connect(Ide.BuildPhase.BUILD, 0, build_stage))

        install_launcher = pipeline.create_launcher()
        install_launcher.set_cwd(srcdir)
        install_launcher.push_argv('mvn')
        install_launcher.push_argv('install')
        install_launcher.push_argv('-Dmaven.test.skip=true')
        install_stage = Ide.BuildStageLauncher.new(context, install_launcher)
        install_stage.set_name(_("Installing project"))
        self.track(pipeline.connect(Ide.BuildPhase.INSTALL, 0, install_stage))

    def _query(self, stage, pipeline, cancellable):
        stage.set_completed(False)

class MavenBuildTarget(Ide.Object, Ide.BuildTarget):

    def do_get_install_directory(self):
        return None

    def do_get_name(self):
        return 'maven-run'

    def do_get_language(self):
        return 'java'

    def do_get_cwd(self):
        context = self.get_context()
        project_file = context.get_project_file()
        return project_file.get_parent().get_path()

    def do_get_argv(self):
        """
        This requires an argument -Dexec.mainClass="my.package.MainClass"
        Use run-opts
        """
        return ["mvn", "exec:java"]

    def do_get_priority(self):
        return 0

class MavenBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = context.get_build_system()

        if type(build_system) != MavenBuildSystem:
            task.return_error(GLib.Error('Not maven build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [MavenBuildTarget(context=self.get_context())]
        task.return_boolean(True)

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets

class MavenIdeTestProvider(Ide.TestProvider):

    def do_run_async(self, test, pipeline, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = context.get_build_system()

        if type(build_system) != MavenBuildSystem:
            task.return_error(GLib.Error('Not maven build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [MavenBuildTarget(context=self.get_context())]

        try:
            runtime = pipeline.get_runtime()
            runner = runtime.create_runner()
            if not runner:
               task.return_error(Ide.NotSupportedError())

            runner.set_flags(Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_PIPE)

            srcdir = pipeline.get_srcdir()
            runner.set_cwd(srcdir)

            commands = test.get_command()

            # DEBUG
            import pdb
            mypdb=pdb.Pdb(stdin=open('fifo_stdin','r'), stdout=open('fifo_stdout','w'))
            mypdb.set_trace()

            for command in commands:
               runner.append_argv(command)

            test.set_status(Ide.TestStatus.RUNNING)

            def run_test_cb (runner, data):
                if runner.run_finish():
                   test.set_status(Ide.TestStatus.FAILED)
                   task.return_error(Ide.NotSupportedError())

                test.set_status(Ide.TestStatus.SUCCESS)
                task.return_boolean(True)

            runner.run_async(run_test_cb)

            return
        except Exception as ex:
            task.return_error(ex)

        task.return_error(Ide.NotSupportedError())

    def do_run_finish(self, result):
        if result.propagate_boolean():
            return result.targets

    def do_reload(self):

        context = self.get_context()
        build_system = context.get_build_system()

        if type(build_system) != MavenBuildSystem:
            task.return_error(GLib.Error('Not maven build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        # find all files in test directory
        # http://maven.apache.org/surefire/maven-surefire-plugin/examples/inclusion-exclusion.html
        build_manager = context.get_build_manager()
        pipeline = build_manager.get_pipeline()
        srcdir = pipeline.get_srcdir()
        test_suite = Gio.File.new_for_path(srcdir + '/src/test/java')
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
                    #TODO Ask java through introspection for classes with TestCase and its public void methods 
                    # or Annotation @Test methods
                    result, contents, etag = gfile.load_contents()
                    tests = [x for x in str(contents).split('\\n') if 'public void' in x]
                    tests = [v.replace("()", "").replace("public void","").strip() for v in tests]
                    classname=name.replace(".java", "")

                    for testname in tests:
                        # http://maven.apache.org/surefire/maven-surefire-plugin/examples/single-test.html
                        command = ["mvn", "-Dtest={}#{}".format(classname, testname), "test"]

                        test = MavenTest(group=classname, id=1, display_name=testname)
                        test.set_command(command)
                        self.add(test)

                info = enumerator.next_file(None)

            enumerator.close()

        except Exception as ex:
            Ide.warning(repr(ex))
            return

class MavenTest(Ide.Test):

    def get_command(self):
        return self.command

    def set_command(self, command):
        self.command = command
