#!/usr/bin/env python3

#
# __init__.py
#
# Copyright © 2016 Christian Hergert <chergert@redhat.com>
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
gi.require_version('Template', '1.0')

from gi.repository import Ide
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import GtkSource
from gi.repository import Peas
from gi.repository import Template

_ = Ide.gettext

def get_module_data_path(name):
    engine = Peas.Engine.get_default()
    plugin = engine.get_plugin_info('autotools_templates')
    data_dir = plugin.get_data_dir()
    return GLib.build_filenamev([data_dir, name])

class LibraryTemplateProvider(GObject.Object, Ide.TemplateProvider):
    def do_get_project_templates(self):
        return [LibraryProjectTemplate(), EmptyProjectTemplate(), GnomeProjectTemplate()]

class AutotoolsTemplateLocator(Template.TemplateLocator):
    license = None

    def empty(self):
        return Gio.MemoryInputStream()

    def do_locate(self, path):
        if path.startswith('license.'):
            filename = GLib.basename(path)
            manager = GtkSource.LanguageManager.get_default()
            language = manager.guess_language(filename, None)

            if self.license == None or language == None:
                return self.empty()

            header = Ide.language_format_header(language, self.license)
            gbytes = GLib.Bytes(header.encode())

            return Gio.MemoryInputStream.new_from_bytes(gbytes)

        return super().do_locate(self, path)

class AutotoolsTemplate(Ide.TemplateBase, Ide.ProjectTemplate):
    def __init__(self, id, name, icon_name, description, languages):
        super().__init__()
        self.id = id
        self.name = name
        self.icon_name = icon_name
        self.description = description
        self.languages = languages
        self.locator = AutotoolsTemplateLocator()

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

        name = params['name'].get_string().lower()

        if 'path' in params:
            dir_path = params['path'].get_string()
        else:
            dir_path = name

        self.language = 'c'
        if 'language' in params:
            self.language = params['language'].get_string().lower()

        if self.language not in ('c', 'c++', 'vala', 'python'):
            task.return_error(GLib.Error("Language %s not supported" %
                                         self.language))
            return

        self.versioning = ''
        if 'versioning' in params:
            self.versioning = params['versioning'].get_string()

        if 'author' in params:
            author_name = params['author'].get_string()
        else:
            author_name = GLib.get_real_name()

        directory = Gio.File.new_for_path(dir_path)

        scope = Template.Scope.new()

        scope.get('template').assign_string(self.id)

        prefix = name if not name.endswith('-glib') else name[:-5]
        PREFIX = prefix.upper().replace('-','_')
        prefix_ = prefix.lower().replace('-','_')
        PreFix = ''.join([word.capitalize() for word in prefix.lower().split('-')])

        name_ = name.lower().replace('-','_')

        scope.get('name').assign_string(name)
        scope.get('name_').assign_string(name_)
        scope.get('NAME').assign_string(name.upper().replace('-','_'))

        scope.get('prefix').assign_string(prefix)
        scope.get('Prefix').assign_string(prefix.capitalize())
        scope.get('PreFix').assign_string(PreFix)
        scope.get('prefix_').assign_string(prefix_)
        scope.get('PREFIX').assign_string(PREFIX)

        scope.get('project_path').assign_string(directory.get_path())
        scope.get('packages').assign_string(self.get_packages())
        scope.get('major_version').assign_string('0')
        scope.get('minor_version').assign_string('1')
        scope.get('micro_version').assign_string('0')
        scope.get('enable_c').assign_boolean(self.language in ('c', 'vala', 'c++'))
        scope.get('enable_python').assign_boolean(self.language == 'python')
        scope.get('enable_cplusplus').assign_boolean(self.language == 'c++')
        scope.get('enable_i18n').assign_boolean(True)
        scope.get('enable_gtk_doc').assign_boolean(False)
        scope.get('enable_gobject_introspection').assign_boolean(self.language in ('c', 'vala', 'c++'))
        scope.get('enable_vapi').assign_boolean(self.language == 'c')
        scope.get('enable_vala').assign_boolean(self.language == 'vala')
        scope.get('translation_copyright').assign_string('Translation copyright holder')
        scope.get('language').assign_string(self.language)

        scope.get('author').assign_string(author_name)

        self.prepare_scope(scope)

        expands = {
            'name': name,
            'name_': name_,
            'prefix': prefix,
            'PreFix': PreFix,
        }

        files = {
            'resources/CONTRIBUTING.md':                'CONTRIBUTING.md',
            'resources/Makefile.am':                    'Makefile.am',
            'resources/NEWS':                           'NEWS',
            'resources/README.md':                      'README.md',
            'resources/autogen.sh':                     'autogen.sh',
            'resources/configure.ac':                   'configure.ac',
            'resources/git.mk':                         'git.mk',

            'resources/m4/Makefile.am':                 'm4/Makefile.am',
            'resources/m4/appstream-xml.m4':            'm4/appstream-xml.m4',
            'resources/m4/ax_append_compile_flags.m4':  'm4/ax_append_compile_flags.m4',
            'resources/m4/ax_append_flag.m4':           'm4/ax_append_flag.m4',
            'resources/m4/ax_check_compile_flag.m4':    'm4/ax_check_compile_flag.m4',
            'resources/m4/ax_check_link_flag.m4':       'm4/ax_check_link_flag.m4',
            'resources/m4/ax_compiler_vendor.m4':       'm4/ax_compiler_vendor.m4',
            'resources/m4/ax_compiler_flags_cxxflags.m4':'m4/ax_compiler_flags_cxxflags.m4',
            'resources/m4/ax_cxx_compile_stdcxx_11.m4': 'm4/ax_cxx_compile_stdcxx_11.m4',
            'resources/m4/ax_require_defined.m4':       'm4/ax_require_defined.m4',
            'resources/m4/glib-gettext.m4':             'm4/glib-gettext.m4',
            'resources/m4/gsettings.m4':                'm4/gsettings.m4',
            'resources/m4/intltool.m4':                 'm4/intltool.m4',
            'resources/m4/introspection.m4':            'm4/introspection.m4',
            'resources/m4/libtool.m4':                  'm4/libtool.m4',
            'resources/m4/pkg.m4':                      'm4/pkg.m4',
            'resources/m4/vala.m4':                     'm4/vala.m4',
            'resources/m4/vapigen.m4':                  'm4/vapigen.m4',

            'resources/data/Makefile.am':               'data/Makefile.am',

            'resources/po/LINGUAS':                     'po/LINGUAS',
            'resources/po/Makevars':                    'po/Makevars',
            'resources/po/POTFILES.in':                 'po/POTFILES.in',
        }

        if 'license_full' in params:
            license_full_path = params['license_full'].get_string()
            files[license_full_path] = 'COPYING'

        if 'license_short' in params:
            license_short_path = params['license_short'].get_string()
            license_base = Gio.resources_lookup_data(license_short_path[11:], 0).get_data().decode()
            self.locator.license = license_base

        self.prepare_files(files)

        modes = { 'resources/autogen.sh': 0o750 }
        self.prepare_file_modes(modes)

        for src,dst in files.items():
            destination = directory.get_child(dst % expands)
            if src.startswith("resource://"):
                self.add_resource(src[11:], destination, scope, modes.get(src, 0))
            else:
                path = get_module_data_path(src)
                self.add_path(path, destination, scope, modes.get(src, 0))

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

    def prepare_scope(self, scope):
        pass

    def prepare_files(self, files):
        pass

    def prepare_file_modes(self, modes):
        pass

    def get_packages(self):
        pass

class LibraryProjectTemplate(AutotoolsTemplate):
    def __init__(self):
        super().__init__(
            'shared-library',
            _("Shared Library (Autotools)"),
            'pattern-library',
            _("Create a new autotools project with a shared library"),
            ['C', 'C++', 'Vala']
         )

    def get_packages(self):
        return "gio-2.0 >= 2.42"

    def prepare_files(self, files):
        files['resources/data/package.pc.in'] = 'data/%(name)s.pc.in'

        if self.language in ('c', 'c++'):
            files['resources/src/Makefile.shared-library-c'] = 'src/Makefile.am'
            files['resources/src/package.h'] = 'src/%(name)s.h'
            files['resources/src/package-version.h.in'] = 'src/%(prefix)s-version.h.in'

        elif self.language == 'vala':
            files['resources/src/Makefile.shared-library-vala'] = 'src/Makefile.am'
            files['resources/src/package.vala'] = 'src/%(prefix)s.vala'


class EmptyProjectTemplate(AutotoolsTemplate):
    def __init__(self):
        super().__init__(
            'empty',
            _("Empty Project (Autotools)"),
            # it would be nice to have a different icon here.
            'pattern-library',
            _("Create a new empty autotools project"),
            ['C', 'C++', 'Vala']
         )

    def get_packages(self):
        return "gio-2.0 >= 2.50"

    def prepare_files(self, files):
        files['resources/src/Makefile.empty'] = 'src/Makefile.am'

class GnomeProjectTemplate(AutotoolsTemplate):
    def __init__(self):
        super().__init__(
            'gnome-app',
            _("GNOME Application (Autotools)"),
            'pattern-gnome',
            _("Create a new flatpak-ready GNOME application"),
            ['C', 'C++', 'Vala', 'Python']
         )

    def get_packages(self):
        if self.language == "c" or self.language == "vala":
            return "gio-2.0 >= 2.50 gtk+-3.0 >= 3.22"
        elif self.language == "c++":
            return "giomm-2.4 >= 2.49.7 gtkmm-3.0 >= 3.22"
        else:
            return ""

    def prepare_files(self, files):
        if self.language in ('c', 'c++', 'vala'):
            files['resources/src/Makefile.gnome-app'] = 'src/Makefile.am'
        elif self.language == 'python':
            files['resources/bin/Makefile.gnome-app'] = 'bin/Makefile.am'
            files['resources/src/Makefile.gnome-app-python'] = '%(name_)s/Makefile.am'

        if self.versioning == 'git':
            files['resources/FlatpakManifestTemplate.json'] = 'org.gnome.%(PreFix)s.json'

        if self.language == 'c':
            files['resources/src/main.c'] = 'src/main.c'
        elif self.language == 'c++':
            files['resources/src/main.cpp'] = 'src/main.cpp'
        elif self.language == 'vala':
            files['resources/src/main.vala'] = 'src/main.vala'
        elif self.language == 'python':
            files['resources/src/__main__.py'] = '%(name_)s/__main__.py'
            files['resources/bin/wrapper.py'] = 'bin/%(name)s.in'

