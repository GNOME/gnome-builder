#!/usr/bin/env python3

#
# phpize_plugin.py
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

import os

from gi.repository import Ide
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio

_ = Ide.gettext

_TYPE_NONE = 0
_TYPE_C = 1
_TYPE_CPLUSPLUS = 2

_BUILD_FLAGS_STDIN_BUF = """
include Makefile

print-%: ; @echo $* = $($*)
"""

def get_file_type(path):
    suffix = path.split('.')[-1]
    if suffix in ('c', 'h'):
        return _TYPE_C
    elif suffix in ('cpp', 'c++', 'cxx', 'cc',
                    'hpp', 'h++', 'hxx', 'hh'):
        return _TYPE_CPLUSPLUS
    return _TYPE_NONE

class PHPizeBuildSystem(Ide.Object, Ide.BuildSystem):
    """
    This is the the basis of the build system. It provides access to
    some information about the project (like CFLAGS/CXXFLAGS, build targets,
    etc). Of course, this skeleton doesn't do a whole lot right now.
    """
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'phpize'

    def do_get_display_name(self):
        return 'PHPize'

    def do_get_priority(self):
        return 3000

    def do_get_build_flags_async(self, file, cancellable, callback, data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.build_flags = []
        task.type = get_file_type(file.get_path())

        if not task.type:
            task.return_boolean(True)
            return

        # To get the build flags, we run make with some custom code to
        # print variables, and then extract the values based on the file type.
        # But before, we must advance the pipeline through CONFIGURE.
        build_manager = Ide.BuildManager.from_context(context)
        build_manager.build_async(Ide.PipelinePhase.CONFIGURE, None, self._get_build_flags_build_cb, task)

    def do_get_build_flags_finish(self, result):
        return result.build_flags

    def _get_build_flags_build_cb(self, build_manager, result, task):
        """
        Completes the asynchronous call to advance the pipeline to CONFIGURE phase
        and then runs a make subprocess to extract build flags from Makefile.
        """
        try:
            build_manager.build_finish(result)

            pipeline = build_manager.get_pipeline()

            # Launcher defaults to $builddir
            launcher = pipeline.create_launcher()
            launcher.set_flags(Gio.SubprocessFlags.STDIN_PIPE |
                               Gio.SubprocessFlags.STDOUT_PIPE |
                               Gio.SubprocessFlags.STDERR_PIPE)
            launcher.push_argv('make')
            launcher.push_argv('-f')
            launcher.push_argv('-')
            launcher.push_argv('print-CFLAGS')
            launcher.push_argv('print-CXXFLAGS')
            launcher.push_argv('print-INCLUDES')
            subprocess = launcher.spawn()
            subprocess.communicate_utf8_async(_BUILD_FLAGS_STDIN_BUF,
                                              task.get_cancellable(),
                                              self._get_build_flags_build_communicate_cb,
                                              task)
        except Exception as ex:
            print(repr(ex))
            task.return_error(GLib.Error(message=repr(ex)))

    def _get_build_flags_build_communicate_cb(self, subprocess, result, task):
        """
        Completes the asynchronous request to get the build flags from the make
        helper subprocess.
        """
        try:
            _, stdout, stderr = subprocess.communicate_utf8_finish(result)

            info = {}
            for line in stdout.split('\n'):
                if '=' in line:
                    k,v = line.split('=', 1)
                    info[k.strip()] = v.strip()

            if task.type == _TYPE_C:
                flags = info.get('CFLAGS', '') + " " + info.get('INCLUDES', '')
            elif task.type == _TYPE_CPLUSPLUS:
                flags = info.get('CXXFLAGS', '') + " " + info.get('INCLUDES', '')
            else:
                raise RuntimeError

            _, build_flags = GLib.shell_parse_argv(flags)

            task.build_flags = build_flags
            task.return_boolean(True)

        except Exception as ex:
            print(repr(ex))
            task.return_error(GLib.Error(message=repr(ex)))

class PHPizeBuildSystemDiscovery(GObject.Object, Ide.BuildSystemDiscovery):
    """
    This is used to discover the build system based on the files within
    the project. This can be useful if someone just clones the project and
    we have to discover the build system dynamically (as opposed to opening
    a particular build project file).
    """

    def do_discover(self, directory, cancellable):
        try:
            config_m4 = directory.get_child('config.m4')
            if config_m4.query_exists():
                stream = open(config_m4.get_path(), encoding='UTF-8')
                if 'PHP_ARG_ENABLE' in stream.read():
                    return ('phpize', 1000)
        except:
            pass

        return (None, 0)

class PHPizePipelineAddin(Ide.Object, Ide.PipelineAddin):
    """
    This class is responsible for attaching the various build operations
    to the pipeline at the appropriate phase.

    We need to bootstrap the project with phpize, and use make to build
    the project.
    """
    def do_load(self, pipeline):
        context = pipeline.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != PHPizeBuildSystem:
            return

        config = pipeline.get_config()
        runtime = config.get_runtime()

        srcdir = pipeline.get_srcdir()
        builddir = pipeline.get_builddir()

        # Bootstrap by calling phpize in the source directory
        bootstrap_launcher = pipeline.create_launcher()
        bootstrap_launcher.push_argv('phpize')
        bootstrap_launcher.set_cwd(srcdir)
        bootstrap_stage = Ide.PipelineStageLauncher.new(context, bootstrap_launcher)
        bootstrap_stage.set_name(_("Bootstrapping project"))
        bootstrap_stage.set_completed(os.path.exists(os.path.join(srcdir, 'configure')))
        self.track(pipeline.attach(Ide.PipelinePhase.AUTOGEN, 0, bootstrap_stage))

        # Configure the project using autoconf. We run from builddir.
        config_launcher = pipeline.create_launcher()
        config_launcher.set_flags(Gio.SubprocessFlags.STDIN_PIPE |
                                  Gio.SubprocessFlags.STDOUT_PIPE |
                                  Gio.SubprocessFlags.STDERR_PIPE)
        config_launcher.push_argv(os.path.join(srcdir, 'configure'))
        config_launcher.push_argv("--prefix={}".format(config.get_prefix()))
        config_opts = config.get_config_opts()
        if config_opts:
            _, config_opts = GLib.shell_parse_argv(config_opts)
            config_launcher.push_args(config_opts)
        config_stage = Ide.PipelineStageLauncher.new(context, config_launcher)
        config_stage.set_name(_("Configuring project"))
        self.track(pipeline.attach(Ide.PipelinePhase.CONFIGURE, 0, config_stage))

        # Build the project using make.
        build_launcher = pipeline.create_launcher()
        build_launcher.push_argv('make')
        if config.props.parallelism > 0:
            build_launcher.push_argv('-j{}'.format(config.props.parallelism))
        clean_launcher = pipeline.create_launcher()
        clean_launcher.push_argv('make')
        clean_launcher.push_argv('clean')
        build_stage = Ide.PipelineStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Building project"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.attach(Ide.PipelinePhase.BUILD, 0, build_stage))

        # Use "make install" to install the project.
        install_launcher = pipeline.create_launcher()
        install_launcher.push_argv('make')
        install_launcher.push_argv('install')
        install_stage = Ide.PipelineStageLauncher.new(context, install_launcher)
        install_stage.set_name(_("Installing project"))
        self.track(pipeline.attach(Ide.PipelinePhase.INSTALL, 0, install_stage))

    def _query(self, stage, pipeline, targets, cancellable):
        # Always defer to make for completion status
        stage.set_completed(False)
