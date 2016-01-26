#!/usr/bin/env python3

#
# __init__.py
#
# Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

from gettext import gettext as _

import gi
import os

gi.require_version('Ide', '1.0')
gi.require_version('Template', '1.0')

from gi.repository import Ide
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Peas
from gi.repository import Template

def get_module_data_path(name):
    engine = Peas.Engine.get_default()
    plugin = engine.get_plugin_info('library_template')
    data_dir = plugin.get_data_dir()
    return GLib.build_filenamev([data_dir, name])

class LibraryTemplateProvider(GObject.Object, Ide.TemplateProvider):
    def do_get_project_templates(self):
        return [LibraryProjectTemplate()]

class LibraryProjectTemplate(Ide.TemplateBase, Ide.ProjectTemplate):
    def do_get_id(self):
        return 'shared-library'

    def do_get_name(self):
        return _("Shared Library")

    def do_get_description(self):
        return _("Create a new autotools project with a shared library")

    def do_get_languages(self):
        return ['C', 'Python']

    def do_get_icon_name(self):
        return 'template-shared-library'

    def do_expand_async(self, params, cancellable, callback, data):
        name = params['name'].get_string()
        directory = Gio.File.new_for_path(name)

        scope = Template.Scope.new()

        prefix = name if not name.endswith('-glib') else name[:-5]
        PREFIX = prefix.upper().replace('-','_')
        prefix_ = prefix.lower().replace('-','_')

        scope.get('name').assign_string(name)
        scope.get('name_').assign_string(name.lower().replace('-','_'))
        scope.get('NAME').assign_string(name.upper().replace('-','_'))

        scope.get('prefix').assign_string(prefix)
        scope.get('prefix_').assign_string(prefix_)
        scope.get('PREFIX').assign_string(PREFIX)

        scope.get('packages').assign_string("gio-2.0 >= 2.42")
        scope.get('major_version').assign_string('0')
        scope.get('minor_version').assign_string('1')
        scope.get('micro_version').assign_string('0')
        scope.get('enable_i18n').assign_boolean(True)
        scope.get('enable_gtk_doc').assign_boolean(False)
        scope.get('enable_gobject_introspection').assign_boolean(True)
        scope.get('enable_vala').assign_boolean(True)
        scope.get('license').assign_string('/* license */')

        expands = {
            'name': name,
            'prefix': prefix,
        }

        files = {
            'shared-library/CONTRIBUTING.md':                'CONTRIBUTING.md',
            'shared-library/Makefile.am':                    'Makefile.am',
            'shared-library/NEWS':                           'NEWS',
            'shared-library/README.md':                      'README.md',
            'shared-library/autogen.sh':                     'autogen.sh',
            'shared-library/configure.ac':                   'configure.ac',
            'shared-library/git.mk':                         'git.mk',

            'shared-library/m4/Makefile.am':                 'm4/Makefile.am',
            'shared-library/m4/appstream-xml.m4':            'm4/appstream-xml.m4',
            'shared-library/m4/ax_append_compile_flags.m4':  'm4/ax_append_compile_flags.m4',
            'shared-library/m4/ax_append_flag.m4':           'm4/ax_append_flag.m4',
            'shared-library/m4/ax_check_compile_flag.m4':    'm4/ax_check_compile_flag.m4',
            'shared-library/m4/ax_check_link_flag.m4':       'm4/ax_check_link_flag.m4',
            'shared-library/m4/ax_compiler_vendor.m4':       'm4/ax_compiler_vendor.m4',
            'shared-library/m4/ax_cxx_compile_stdcxx_11.m4': 'm4/ax_cxx_compile_stdcxx_11.m4',
            'shared-library/m4/ax_require_defined.m4':       'm4/ax_require_defined.m4',
            'shared-library/m4/glib-gettext.m4':             'm4/glib-gettext.m4',
            'shared-library/m4/gsettings.m4':                'm4/gsettings.m4',
            'shared-library/m4/intltool.m4':                 'm4/intltool.m4',
            'shared-library/m4/introspection.m4':            'm4/introspection.m4',
            'shared-library/m4/libtool.m4':                  'm4/libtool.m4',
            'shared-library/m4/pkg.m4':                      'm4/pkg.m4',
            'shared-library/m4/vala.m4':                     'm4/vala.m4',
            'shared-library/m4/vapigen.m4':                  'm4/vapigen.m4',

            'shared-library/data/package.pc.in':             'data/%(name)s-1.0.pc.in',
            'shared-library/data/Makefile.am':               'data/Makefile.am',
            'shared-library/po/POTFILES.in':                 'po/POTFILES.in',

            'shared-library/src/Makefile.am':                'src/Makefile.am',
            'shared-library/src/package.h':                  'src/%(name)s.h',
            'shared-library/src/package-version.h.in':       'src/%(prefix)s-version.h.in',

        }

        modes = { 'shared-library/autogen.sh': 0o750 }

        for src,dst in files.items():
            path = get_module_data_path(src)
            destination = directory.get_child(dst % expands)
            self.add_path(path, destination, scope, modes.get(src, 0))

        task = Gio.Task.new(self, cancellable, callback)
        self.expand_all_async(cancellable, self.expand_all_cb, task)

    def do_expand_finish(self, result):
        return result.propagate_boolean()

    def expand_all_cb(self, obj, result, task):
        try:
            self.expand_all_finish(result)
            task.return_boolean(True)
        except Exception as exc:
            print(exc)
            task.return_error(GLib.Error(exc))
