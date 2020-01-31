# __init__.py
#
# Copyright 2017 Matthew Leeds <mleeds@redhat.com>
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

import os
from os import path

from gi.repository import GObject
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GtkSource
from gi.repository import Ide
from gi.repository import Template

_ = Ide.gettext

class MakeBuildSystemDiscovery(Ide.SimpleBuildSystemDiscovery):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.props.glob = '+(GNUmakefile|makefile|Makefile)'
        self.props.hint = 'make_plugin'
        self.props.priority = 1000

class MakeBuildSystem(Ide.Object, Ide.BuildSystem):
    project_file = GObject.Property(type=Gio.File)
    make_dir = GObject.Property(type=Gio.File)
    run_args = None

    def do_parent_set(self, parent):
        if self.project_file.query_file_type(0, None) == Gio.FileType.DIRECTORY:
            self.make_dir = self.project_file
        else:
            self.make_dir = self.project_file.get_parent()

    def do_get_id(self):
        return 'make'

    def do_get_display_name(self):
        return 'Make'

    def do_get_priority(self):
        return 0

    def do_get_builddir(self, pipeline):
        return self.get_context().ref_workdir().get_path()

    def get_make_dir(self):
        return self.make_dir

class MakePipelineAddin(Ide.Object, Ide.PipelineAddin):
    """
    The MakePipelineAddin registers stages to be executed when various
    phases of the build pipeline are requested.
    """

    def do_load(self, pipeline):
        context = pipeline.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        # Only register stages if we are a makefile project
        if type(build_system) != MakeBuildSystem:
            return

        config = pipeline.get_config()
        runtime = config.get_runtime()

        # If the configuration has set $MAKE, then use it.
        make = config.getenv('MAKE') or "make"

        srcdir = context.ref_workdir().get_path()
        builddir = pipeline.get_builddir()

        # Register the build launcher which will perform the incremental
        # build of the project when the Ide.PipelinePhase.BUILD phase is
        # requested of the pipeline.
        build_launcher = pipeline.create_launcher()
        build_launcher.set_cwd(build_system.get_make_dir().get_path())
        build_launcher.push_argv(make)
        if config.props.parallelism > 0:
            build_launcher.push_argv('-j{}'.format(config.props.parallelism))

        clean_launcher = pipeline.create_launcher()
        clean_launcher.set_cwd(build_system.get_make_dir().get_path())
        clean_launcher.push_argv(make)
        clean_launcher.push_argv('clean')

        build_stage = Ide.PipelineStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Build project"))
        build_stage.set_clean_launcher(clean_launcher)
        build_stage.connect('query', self._query)
        self.track(pipeline.attach(Ide.PipelinePhase.BUILD, 0, build_stage))

        # Register the install launcher which will perform our
        # "make install" when the Ide.PipelinePhase.INSTALL phase
        # is requested of the pipeline.
        install_launcher = pipeline.create_launcher()
        install_launcher.set_cwd(build_system.get_make_dir().get_path())
        install_launcher.push_argv(make)
        install_launcher.push_argv('install')

        install_stage = Ide.PipelineStageLauncher.new(context, install_launcher)
        install_stage.set_name(_("Install project"))
        self.track(pipeline.attach(Ide.PipelinePhase.INSTALL, 0, install_stage))

        # Determine what it will take to "make run" for this pipeline
        # and stash it on the build_system for use by the build target.
        # This allows us to run Make projects as long as the makefile
        # has a "run" target.
        build_system.run_args = [make, '-C', build_system.get_make_dir().get_path(), 'run']

    def _query(self, stage, pipeline, targets, cancellable):
        stage.set_completed(False)

class MakeBuildTarget(Ide.Object, Ide.BuildTarget):

    def do_get_install_directory(self):
        return None

    def do_get_name(self):
        return 'make-run'

    def do_get_language(self):
        # Not meaningful, since we have an indirect process.
        return 'make'

    def do_get_argv(self):
        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)
        assert type(build_system) == MakeBuildSystem
        return build_system.run_args

    def do_get_priority(self):
        return 0

class MakeBuildTargetProvider(Ide.Object, Ide.BuildTargetProvider):
    """
    The MakeBuildTargetProvider just wraps a "make run" command. If the
    Makefile doesn't have a run target, we'll just fail to execute and the user
    should get the warning in their application output (and can update their
    Makefile appropriately).
    """

    def do_get_targets_async(self, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.set_priority(GLib.PRIORITY_LOW)

        context = self.get_context()
        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != MakeBuildSystem:
            task.return_error(GLib.Error('Not a make build system',
                                         domain=GLib.quark_to_string(Gio.io_error_quark()),
                                         code=Gio.IOErrorEnum.NOT_SUPPORTED))
            return

        task.targets = [MakeBuildTarget()]
        task.return_boolean(True)

    def do_get_targets_finish(self, result):
        if result.propagate_boolean():
            return result.targets

class MakeProjectTemplateProvider(GObject.Object, Ide.TemplateProvider):
    def do_get_project_templates(self):
        return [SimpleMakefileTemplate()]

class MakeTemplateLocator(Template.TemplateLocator):
    license = None

    def empty(self):
        return Gio.MemoryInputStream()

    def do_locate(self, path):
        if path.startswith('license.'):
            filename = GLib.basename(path)
            manager = GtkSource.LanguageManager.get_default()
            language = manager.guess_language(filename, None)

            if self.license is None or language is None:
                return self.empty()

            header = Ide.language_format_header(language, self.license)
            gbytes = GLib.Bytes(header.encode())

            return Gio.MemoryInputStream.new_from_bytes(gbytes)

        return super().do_locate(self, path)

class MakeTemplateBase(Ide.TemplateBase, Ide.ProjectTemplate):
    def __init__(self, id, name, icon_name, description, languages):
        super().__init__()
        self.id = id
        self.name = name
        self.icon_name = icon_name
        self.description = description
        self.languages = languages

        # Work around https://bugzilla.gnome.org/show_bug.cgi?id=687522
        self.locator = MakeTemplateLocator()
        self.props.locator = self.locator

    def do_get_id(self):
        return self.id

    def do_get_name(self):
        return self.name

    def do_get_icon_name(self):
        return self.icon_name

    def do_get_description(self):
        return self.description

    def do_get_languages(self):
        return self.languages

    def do_expand_async(self, params, cancellable, callback, data):
        self.reset()

        task = Gio.Task.new(self, cancellable, callback)

        if 'language' in params:
            self.language = params['language'].get_string().lower()
        else:
            self.language = 'c'

        if self.language not in ('c', 'c++'):
            task.return_error(GLib.Error('Language %s not supported' % self.language))
            return

        if 'versioning' in params:
            self.versioning = params['versioning'].get_string()
        else:
            self.versioning = ''

        if 'author' in params:
            author_name = params['author'].get_string()
        else:
            author_name = GLib.get_real_name()

        scope = Template.Scope.new()
        scope.get('template').assign_string(self.id)

        name = params['name'].get_string().lower()
        name_ = name.lower().replace('-', '_')
        scope.get('name').assign_string(name)
        scope.get('name_').assign_string(name_)
        scope.get('NAME').assign_string(name.upper().replace('-','_'))

        prefix = name if not name.endswith('-glib') else name[:-5]
        PREFIX = prefix.upper().replace('-','_')
        prefix_ = prefix.lower().replace('-','_')
        PreFix = ''.join([word.capitalize() for word in prefix.lower().split('-')])

        scope.get('prefix').assign_string(prefix)
        scope.get('Prefix').assign_string(prefix.capitalize())
        scope.get('PreFix').assign_string(PreFix)
        scope.get('prefix_').assign_string(prefix_)
        scope.get('PREFIX').assign_string(PREFIX)

        scope.get('language').assign_string(self.language)
        scope.get('author').assign_string(author_name)
        scope.get('exec_name').assign_string(name)

        expands = {
            'prefix': prefix,
            'name_': name_,
            'name': name,
            'exec_name': name,
        }

        files = {
            'resources/.gitignore': '.gitignore',
            'resources/Makefile': 'Makefile',
            'resources/main.c': '%(exec_name)s.c',
        }
        self.prepare_files(files)

        modes = {}

        if 'license_full' in params:
            license_full_path = params['license_full'].get_string()
            files[license_full_path] = 'COPYING'

        if 'license_short' in params:
            license_short_path = params['license_short'].get_string()
            license_base = Gio.resources_lookup_data(license_short_path[11:], 0).get_data().decode()
            self.locator.license = license_base

        if 'path' in params:
            dir_path = params['path'].get_string()
        else:
            dir_path = name
        directory = Gio.File.new_for_path(dir_path)
        scope.get('project_path').assign_string(directory.get_path())

        for src, dst in files.items():
            destination = directory.get_child(dst % expands)
            if src.startswith('resource://'):
                self.add_resource(src[11:], destination, scope, modes.get(src, 0))
            else:
                path = os.path.join('/plugins/make_plugin', src)
                self.add_resource(path, destination, scope, modes.get(src, 0))

        self.expand_all_async(cancellable, self.expand_all_cb, task)

    def do_expand_finish(self, result):
        return result.propagate_boolean()

    def expand_all_cb(self, obj, result, task):
        try:
            self.expand_all_finish(result)
            task.return_boolean(True)
        except Exception as exc:
            if isinstance(exc, GLib.Error):
                task.return_error(exc)
            else:
                task.return_error(GLib.Error(repr(exc)))

class SimpleMakefileTemplate(MakeTemplateBase):
    def __init__(self):
        super().__init__(
            'empty-makefile',
            _('Empty Makefile Project'),
            'pattern-cli',
            _('Create a new empty project using a simple Makefile'),
            ['C', 'C++'],
         )

    def do_get_priority(self):
        return 10000

    def prepare_files(self, files):
        pass
