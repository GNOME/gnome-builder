#!/usr/bin/env python3
# __init__.py
#
# Copyright (C) 2016 Patrick Griffis <tingping@tingping.se>
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

import gi
import os
from os import path

gi.require_version('Ide', '1.0')
gi.require_version('Template', '1.0')

from gi.repository import (
    Ide,
    Gio,
    GLib,
    GObject,
    GtkSource,
    Peas,
    Template,
)

_ = Ide.gettext

def get_module_data_path(name):
    engine = Peas.Engine.get_default()
    plugin = engine.get_plugin_info('meson_templates')
    data_dir = plugin.get_data_dir()
    return path.join(data_dir, name)

class LibraryTemplateProvider(GObject.Object, Ide.TemplateProvider):
    def do_get_project_templates(self):
        return [EmptyProjectTemplate()]

class MesonTemplateLocator(Template.TemplateLocator):
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

# Map builder langs to meson ones
LANGUAGE_MAP = {
    'c': 'c',
    'c++': 'cpp',
}

class MesonTemplate(Ide.TemplateBase, Ide.ProjectTemplate):
    def __init__(self, id, name, icon_name, description, languages):
        super().__init__()
        self.id = id
        self.name = name
        self.icon_name = icon_name
        self.description = description
        self.languages = languages
        self.locator = MesonTemplateLocator()

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
            task.return_error(GLib.Error('Language %s not supported' %
                                         self.language))
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
        name_ = name.lower().replace('-','_')
        scope.get('name').assign_string(name)
        scope.get('name_').assign_string(name_)

        scope.get('project_version').assign_string('0.1.0')
        scope.get('enable_i18n').assign_boolean(True)
        scope.get('language').assign_string(LANGUAGE_MAP[self.language])
        scope.get('author').assign_string(author_name)

        modes = {
        }

        expands = {
        }

        files = {
            'resources/meson.build': 'meson.build',
            'resources/src/meson.build': 'src/meson.build',

            # Translations
            'resources/po/LINGUAS': 'po/LINGUAS',
            'resources/po/meson.build': 'po/meson.build',
            'resources/po/POTFILES': 'po/POTFILES',
        }

        if self.language == 'c':
            files['resources/src/main.c'] = 'src/main.c'
        elif self.language == 'c++':
            files['resources/src/main.c'] = 'src/main.cpp'

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


class EmptyProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            'empty-meson',
            _('Empty Meson Project'),
            'pattern-library',
            _('Create a new empty meson project'),
            ['C', 'C++']
         )


