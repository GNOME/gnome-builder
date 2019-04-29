#!/usr/bin/env python3

#
# npm_plugin.py
#
# Copyright 2016-2018 Christian Hergert <chergert@redhat.com>
# Copyright 2017 Giovanni Campagna <gcampagn@cs.stanford.edu>
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
import json

from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Ide

Ide.g_file_add_ignored_pattern('node_modules')

_ = Ide.gettext

class NPMBuildSystemDiscovery(Ide.SimpleBuildSystemDiscovery):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.props.glob = 'package.json'
        self.props.hint = 'npm_plugin'
        self.props.priority = 100

class NPMBuildSystem(Ide.Object, Ide.BuildSystem):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'npm'

    def do_get_display_name(self):
        return 'NPM (node.js)'

    def do_get_priority(self):
        return 100

    def get_package_json(self):
        if self.props.project_file.get_basename() != 'package.json':
            return self.props.project_file.get_child('package.json')
        else:
            return self.props.project_file

class NPMPipelineAddin(Ide.Object, Ide.PipelineAddin):
    """
    The NPMPipelineAddin is responsible for creating the necessary build
    stages and attaching them to phases of the build pipeline.
    """

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        # Ignore pipeline unless this is a npm/nodejs project
        if type(build_system) != NPMBuildSystem:
            return

        package_json = build_system.get_package_json()
        config = pipeline.get_config()
        builddir = pipeline.get_builddir()
        runtime = config.get_runtime()

        npm = 'npm'
        if config.getenv('NPM'):
            npm = config.getenv('NPM')
        elif not runtime.contains_program_in_path('npm'):
            raise OSError('The runtime must contain nodejs/npm to build npm modules')

        # Fetch dependencies so that we no longer need network access
        fetch_launcher = pipeline.create_launcher()
        fetch_launcher.set_cwd(package_json.get_parent().get_path())
        fetch_launcher.push_argv(npm)
        if not pipeline.is_native():
            fetch_launcher.push_argv('--arch')
            fetch_launcher.push_argv(pipeline.get_host_triplet().get_arch())
        fetch_launcher.push_argv('install')
        stage = Ide.PipelineStageLauncher.new(context, fetch_launcher)
        stage.set_name(_("Downloading npm dependencies"))
        self.track(pipeline.attach(Ide.PipelinePhase.DOWNLOADS, 0, stage))


# The scripts used by the npm build system during build
# we don't want to include these as build target
NPM_SPECIAL_SCRIPTS = set(['prepare', 'publish', 'prepublishOnly', 'install', 'uninstall',
                           'version', 'shrinkwrap'])
# Standard name of user defined scripts
# We do want to include these as build targets, but we don't want
# to include the corresponding "pre" and "post" variants
NPM_STANDARD_SCRIPTS = set(['test', 'start', 'stop', 'restart'])

class NPMBuildTarget(Ide.Object, Ide.BuildTarget):
    def __init__(self, script, **kw):
        super().__init__(**kw)
        self._script = script

    def do_get_install_directory(self):
        return None

    def do_get_display_name(self):
        return 'npm run ' + self._script

    def do_get_name(self):
        return 'npm-run-' + self._script

    def do_get_argv(self):
        return ['npm', 'run', '--silent', self._script]

    def do_get_cwd(self):
        context = self.get_context()
        return context.ref_workdir().get_path()

    def do_get_language(self):
        return 'js'

    def do_get_priority(self):
        if self._script == 'start':
            return -10
        elif self._script in ('stop', 'restart'):
            return 5
        elif self._script == 'test':
            return 10
        else:
            return 0

def is_ignored_script(script, all_scripts):
    if script in NPM_SPECIAL_SCRIPTS:
        return True
    if script.startswith('pre'):
        without_pre = script[3:]
        if without_pre in NPM_SPECIAL_SCRIPTS or \
            without_pre in NPM_STANDARD_SCRIPTS or \
            without_pre in all_scripts:
            return True
    if script.startswith('post'):
        without_post = script[4:]
        if without_post in NPM_SPECIAL_SCRIPTS or \
            without_post in NPM_STANDARD_SCRIPTS or \
            without_post in all_scripts:
            return True
    return False

class NPMBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):

    def _on_package_json_loaded(self, project_file, result, task):
        try:
            ok, contents, _etag = project_file.load_contents_finish(result)
        except GLib.Error as e:
            task.return_error(e)

        package_json = json.loads(contents.decode('utf-8'))
        if 'scripts' not in package_json:
            task.targets = []
            task.return_boolean(True)
            return

        task.targets = []
        for name in package_json['scripts']:
            if is_ignored_script(name, package_json['scripts']):
                continue
            task.targets.append(NPMBuildTarget(name))

        # if no start script is specified, but server.js exists,
        # we can still run "npm start"
        if 'start' not in package_json['scripts'] and \
            project_file.get_parent().get_child('server.js').query_exists(None):
                task.targets.append(NPMBuildTarget('start'))

        task.return_boolean(True)

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != NPMBuildSystem:
            task.return_error(GLib.Error('Not NPM build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        project_file = build_system.get_package_json()
        project_file.load_contents_async(cancellable, self._on_package_json_loaded, task)

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets
