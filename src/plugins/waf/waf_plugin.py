#!/usr/bin/env python3

#
# waf_plugin.py
#
# Copyright 2019 Alex Mitchell
# Copyright 2019 Christian Hergert <chergert@redhat.com>
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

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import Ide
from gi.repository import GObject

_ = Ide.gettext

def sniff_python_version(path):
    """
    Use python3 if specified, otherwise python2.
    """
    try:
        f = open(path, 'r')
        line = f.readline()
        if 'python3' in line:
            return 'python3'
    except:
        pass
    return 'python2'

class WafBuildSystemDiscovery(Ide.SimpleBuildSystemDiscovery):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.props.glob = 'wscript'
        self.props.hint = 'waf_plugin'
        self.props.priority = 1000

class WafBuildSystem(Ide.Object, Ide.BuildSystem):
    project_file = GObject.Property(type=Gio.File)
    python = None
    waf_local = None

    def do_get_id(self):
        return 'waf'

    def do_get_display_name(self):
        return 'Waf'

    def do_get_priority(self):
        return 1000

class WafPipelineAddin(Ide.Object, Ide.PipelineAddin):
    """
    The WafPipelineAddin is responsible for creating the necessary build
    stages and attaching them to phases of the build pipeline.
    """
    python = None
    waf_local = None

    def _create_launcher(self, pipeline):
        launcher = pipeline.create_launcher()
        if self.waf_local:
            launcher.push_argv(self.python)
        launcher.push_argv('waf')
        return launcher

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)
        srcdir = pipeline.get_srcdir()
        config = pipeline.get_config()
        config_opts = config.get_config_opts()

        # Ignore pipeline unless this is a waf project
        if type(build_system) != WafBuildSystem:
            return

        waf = os.path.join(srcdir, 'waf')
        self.python = sniff_python_version(waf)

        # If waf is in project directory use that
        self.waf_local = os.path.isfile(waf)

        # Avoid sniffing again later in targets provider
        build_system.python = self.python
        build_system.waf_local = self.waf_local

        # Launcher for project configuration
        config_launcher = self._create_launcher(pipeline)
        config_launcher.set_cwd(srcdir)
        config_launcher.push_argv('configure')
        config_launcher.push_argv('--prefix=%s' % config.get_prefix())
        if config_opts:
            try:
                ret, argv = GLib.shell_parse_argv(config_opts)
                config_launcher.push_args(argv)
            except Exception as ex:
                print(repr(ex))
        self.track(pipeline.attach_launcher(Ide.PipelinePhase.CONFIGURE, 0, config_launcher))

        # Now create our launcher to build the project
        build_launcher = self._create_launcher(pipeline)
        build_launcher.set_cwd(srcdir)
        build_launcher.push_argv('build')

        clean_launcher = self._create_launcher(pipeline)
        clean_launcher.set_cwd(srcdir)
        clean_launcher.push_argv('clean')

        build_stage = Ide.PipelineStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Building project…"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.attach(Ide.PipelinePhase.BUILD, 0, build_stage))

        install_launcher = self._create_launcher(pipeline)
        install_launcher.set_cwd(srcdir)
        install_launcher.push_argv('install')

        install_stage = Ide.PipelineStageLauncher.new(context, install_launcher)
        install_stage.set_name(_("Installing project…"))
        install_stage.connect('query', self._query)
        self.track(pipeline.attach(Ide.PipelinePhase.INSTALL, 0, install_stage))

    def do_unload(self, application):
        pass

    def _query(self, stage, pipeline, targets, cancellable):
        # Defer to waf to determine if building is necessary
        stage.set_completed(False)

class WafBuildTarget(Ide.Object, Ide.BuildTarget):
    name = None

    def __init__(self, name, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.name = name

    def do_get_install_directory(self):
        # TODO: We pretend that everything is installed, how can we determine
        #       if that is really the case? This allows us to choose a target
        #       in the project-tree to run.
        context = self.get_context()
        config_manager = Ide.ConfigManager.from_context(context)
        config = config_manager.get_current()
        directory = config.get_prefix()
        return Gio.File.new_for_path(os.path.join(directory, 'bin'))

    def do_get_display_name(self):
        return self.name

    def do_get_name(self):
        return self.name

    def do_get_kind(self):
        # TODO: How can we determine this? We fake executable so the user
        # can right-click "Run" from the project-tree.
        return Ide.ArtifactKind.EXECUTABLE

    def do_get_language(self):
        return None

    def do_get_argv(self):
        # TODO: Better way to discovery this, we just pretend the
        #       target is installed to $prefix/bin because I don't
        #       immediately see another way to do it.
        context = self.get_context()
        config_manager = Ide.ConfigManager.from_context(context)
        config = config_manager.get_current()
        directory = config.get_prefix()
        return [os.path.join(directory, 'bin', self.name)]

    def do_get_priority(self):
        return 0

class WafBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)
        task.targets = []

        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)
        build_manager = Ide.BuildManager.from_context(context)
        pipeline = build_manager.get_pipeline()

        if pipeline is None or type(build_system) != WafBuildSystem:
            task.return_error(GLib.Error('No access to waf build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        # For some reason, "waf list" outputs on stderr
        launcher = build_system._create_launcher(pipeline)
        launcher.set_flags(Gio.SubprocessFlags.STDOUT_SILENCE | Gio.SubprocessFlags.STDERR_PIPE)
        launcher.set_cwd(pipeline.get_srcdir())
        launcher.push_argv('list')

        try:
            subprocess = launcher.spawn(cancellable)
            subprocess.communicate_utf8_async(None, cancellable, self.communicate_cb, task)
        except Exception as ex:
            task.return_error(GLib.Error(repr(ex),
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.FAILED))

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets

    def communicate_cb(self, subprocess, result, task):
        try:
            ret, stdout, stderr = subprocess.communicate_utf8_finish(result)
            lines = stderr.strip().split('\n')
            if len(lines) > 0:
                # Trim 'list' finished ... line
                del lines[-1]
            for line in lines:
                task.targets.append(WafBuildTarget(line.strip()))
            task.return_boolean(True)
        except Exception as ex:
            task.return_error(GLib.Error(repr(ex),
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.FAILED))
