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

class LibraryTemplateProvider(GObject.Object, Ide.TemplateProvider):
    def do_get_project_templates(self):
        return [GnomeProjectTemplate(), LibraryProjectTemplate(), EmptyProjectTemplate()]


class MesonTemplateLocator(Template.TemplateLocator):
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

        if self.language not in ('c', 'c++', 'javascript', 'python', 'vala'):
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
        name_ = name.lower().replace('-', '_')
        scope.get('name').assign_string(name)
        scope.get('name_').assign_string(name_)
        scope.get('NAME').assign_string(name.upper().replace('-','_'))

        # TODO: Support setting app id
        appid = 'org.gnome.' + name.title()
        appid_path = '/' + appid.replace('.', '/')
        scope.get('appid').assign_string(appid)
        scope.get('appid_path').assign_string(appid_path)

        prefix = name if not name.endswith('-glib') else name[:-5]
        PREFIX = prefix.upper().replace('-','_')
        prefix_ = prefix.lower().replace('-','_')
        PreFix = ''.join([word.capitalize() for word in prefix.lower().split('-')])

        scope.get('prefix').assign_string(prefix)
        scope.get('Prefix').assign_string(prefix.capitalize())
        scope.get('PreFix').assign_string(PreFix)
        scope.get('prefix_').assign_string(prefix_)
        scope.get('PREFIX').assign_string(PREFIX)


        enable_gnome = isinstance(self, GnomeProjectTemplate)
        scope.get('project_version').assign_string('0.1.0')
        scope.get('enable_i18n').assign_boolean(enable_gnome)
        scope.get('enable_gnome').assign_boolean(enable_gnome)
        scope.get('language').assign_string(self.language)
        scope.get('author').assign_string(author_name)

        # Just avoiding dealing with template bugs
        if self.language == 'c':
            ui_file = prefix + '-window.ui'
        else:
            ui_file = 'window.ui'
        scope.get('ui_file').assign_string(ui_file)

        exec_name = appid if self.language == 'javascript' else name
        scope.get('exec_name').assign_string(exec_name)

        modes = {
            'resources/src/hello.js.in': 0o750,
            'resources/src/hello.py.in': 0o750,
            'resources/build-aux/meson/postinstall.py': 0o750,
        }

        expands = {
            'prefix': prefix,
            'appid': appid,
            'name_': name_,
            'name': name,
        }

        files = {
            # Build files
            'resources/meson.build': 'meson.build',

        }
        self.prepare_files(files)

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
                path = os.path.join('/org/gnome/builder/plugins/meson_templates', src)
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


class GnomeProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            'gnome-app',
            _('GNOME Application'),
            'pattern-gnome',
            _('Create a new GNOME application'),
            ['C', 'Python', 'JavaScript', 'Vala']
         )

    def prepare_files(self, files):
        # Shared files
        files['resources/flatpak.json'] = '%(appid)s.json'
        files['resources/data/hello.desktop.in'] = 'data/%(appid)s.desktop.in'
        files['resources/data/hello.appdata.xml.in'] = 'data/%(appid)s.appdata.xml.in'
        files['resources/data/hello.gschema.xml'] = 'data/%(appid)s.gschema.xml'
        files['resources/data/meson.build'] = 'data/meson.build'
        files['resources/build-aux/meson/postinstall.py'] = 'build-aux/meson/postinstall.py'
        files['resources/po/LINGUAS'] = 'po/LINGUAS'
        files['resources/po/meson.build'] = 'po/meson.build'
        files['resources/po/POTFILES'] = 'po/POTFILES'
        window_ui_name = 'src/window.ui'
        resource_name = 'src/%(prefix)s.gresource.xml'
        meson_file = 'resources/src/meson-c-vala.build'

        if self.language == 'c':
            files['resources/src/main.c'] = 'src/main.c'
            files['resources/src/window.c'] = 'src/%(prefix)s-window.c'
            files['resources/src/window.h'] = 'src/%(prefix)s-window.h'
            window_ui_name = 'src/%(prefix)s-window.ui'
        elif self.language == 'vala':
            files['resources/src/main.vala'] = 'src/main.vala'
            files['resources/src/window.vala'] = 'src/window.vala'
        elif self.language == 'javascript':
            files['resources/src/main.js'] = 'src/main.js'
            files['resources/src/hello.js.in'] = 'src/%(appid)s.in'
            files['resources/src/window.js'] = 'src/window.js'
            files['resources/src/hello.src.gresource.xml'] = 'src/%(appid)s.src.gresource.xml'
            resource_name = 'src/%(appid)s.data.gresource.xml'
            meson_file = 'resources/src/meson-js.build'
        elif self.language == 'python':
            files['resources/src/hello.py.in'] = 'src/%(name)s.in'
            files['resources/src/gi_composites.py'] = 'src/gi_composites.py'
            files['resources/src/__init__.py'] = 'src/__init__.py'
            files['resources/src/window.py'] = 'src/window.py'
            files['resources/src/main.py'] = 'src/main.py'
            meson_file = 'resources/src/meson-py.build'

        files['resources/src/hello.gresource.xml'] = resource_name
        files['resources/src/window.ui'] = window_ui_name
        files[meson_file] = 'src/meson.build'


class LibraryProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            'shared-library',
            _("Shared Library"),
            'pattern-library',
            _("Create a new project with a shared library"),
            ['C']
         )

    def prepare_files(self, files):
        if self.language == 'c':
            files['resources/src/meson-clib.build'] = 'src/meson.build'
            files['resources/src/hello.h'] = 'src/%(name)s.h'
            files['resources/src/hello-version.h.in'] = 'src/%(prefix)s-version.h.in'


class EmptyProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            'empty',
            _('Empty Project'),
            'pattern-library',
            _('Create a new empty project'),
            ['C', 'C++', 'JavaScript', 'Python', 'Vala'],
         )

    def prepare_files(self, files):
        files['resources/src/meson-empty.build'] = 'src/meson.build'
