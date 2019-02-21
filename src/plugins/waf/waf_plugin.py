#!/usr/bin/env-python3

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
        self.props.glob = 'waf'
        self.props.hint = 'waf_plugin'
        self.props.priority = 1000

class WafBuildSystem(Ide.Object, Ide.BuildSystem):
    project_file = GObject.Property(type=Gio.File)

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

    def do_load(self, pipeline):
        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)
        srcdir = pipeline.get_srcdir()

        # Ignore pipeline unless this is a waf project
        if type(build_system) != WafBuildSystem:
            return

        # Sniff the required python version
        waf = os.path.join(srcdir, 'waf')
        python = sniff_python_version(waf)

        # Launcher for project configuration
        config_launcher = pipeline.create_launcher()
        config_launcher.set_cwd(srcdir)
        config_launcher.push_argv(python)
        config_launcher.push_argv('waf')
        config_launcher.push_argv('configure')
        self.track(pipeline.attach_launcher(Ide.PipelinePhase.CONFIGURE, 0, config_launcher))

        # Now create our launcher to build the project
        build_launcher = pipeline.create_launcher()
        build_launcher.set_cwd(srcdir)
        build_launcher.push_argv(python)
        build_launcher.push_argv('waf')

        clean_launcher = pipeline.create_launcher()
        clean_launcher.set_cwd(srcdir)
        clean_launcher.push_argv(python)
        clean_launcher.push_argv('waf')
        clean_launcher.push_argv('clean')

        build_stage = Ide.PipelineStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Building project"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.attach(Ide.PipelinePhase.BUILD, 0, build_stage))

    def do_unload(self, application):
        pass

    def _query(self, stage, pipeline, targets, cancellable):
        # Defer to waf to determine if building is necessary
        stage.set_completed(False)

