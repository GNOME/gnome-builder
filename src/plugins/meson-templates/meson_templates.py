#!/usr/bin/env python3
# __init__.py
#
# Copyright 2016 Patrick Griffis <tingping@tingping.se>
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
import time
from os import path

from gi.repository import (
    Ide,
    Gio,
    GLib,
    GObject,
    GtkSource,
    Template,
)

_ = Ide.gettext

class LibraryTemplateProvider(GObject.Object, Ide.TemplateProvider):
    def do_get_project_templates(self):
        return [GnomeAdwaitaProjectTemplate(),
                GnomeGTK4ProjectTemplate(),
                GnomeProjectTemplate(),
                LibraryProjectTemplate(),
                CLIProjectTemplate(),
                EmptyProjectTemplate()]

class MesonTemplate(Ide.ProjectTemplate):

    def do_validate_name(self, name):
        # meson reserves the name 'test'
        if name == 'test':
            return False
        return Ide.ProjectTemplate.do_validate_name(self, name);

    def do_expand_async(self, input, scope, cancellable, callback, data):
        self.reset()

        task = Ide.Task.new(self, cancellable, callback)

        prefix = scope.dup_string('prefix')
        appid = scope.dup_string('appid')
        name_ = scope.dup_string('name_')
        name = scope.dup_string('name')
        language = input.props.language.lower()

        enable_gnome = (isinstance(self, GnomeProjectTemplate) or
                        isinstance(self, GnomeGTK4ProjectTemplate) or
                        isinstance(self, GnomeAdwaitaProjectTemplate))
        scope.get('enable_i18n').assign_boolean(enable_gnome)
        scope.get('enable_gnome').assign_boolean(enable_gnome)

        scope.get('is_adwaita').assign_boolean(True if isinstance(self, GnomeAdwaitaProjectTemplate) else False)

        # Just avoiding dealing with template bugs
        if language in ('c', 'c++'):
            scope.set_string('ui_file', prefix + '-window.ui')
        elif language in ('C♯',):
            scope.set_string('ui_file', '')
        else:
            scope.set_string('ui_file', 'window.ui')

        exec_name = appid if language == 'javascript' else name
        scope.get('exec_name').assign_string(exec_name)

        modes = {
            'resources/src/hello.js.in': 0o750,
            'resources/src/hello.py.in': 0o750,
            'resources/src/hello-cli.py.in': 0o750,
            'resources/src/application.in': 0o750,
        }

        expands = {
            'prefix': prefix,
            'appid': appid,
            'name_': name_,
            'name': name,
            'exec_name': exec_name,
        }

        files = {
            # Build files
            'resources/meson.build': 'meson.build',

        }
        self.prepare_files(files, language)

        license_resource = input.get_license_path()
        if license_resource is not None:
            files[license_resource] = 'COPYING'

        directory = input.props.directory.get_child(name)

        for src, dst in files.items():
            #print('adding', src, '=>', dst % expands)
            destination = directory.get_child(dst % expands)
            if src[0] == '/':
                self.add_resource(src, destination, scope, modes.get(src, 0))
            elif src.startswith('resource://'):
                self.add_resource(src[11:], destination, scope, modes.get(src, 0))
            else:
                path = os.path.join('/plugins/meson_templates', src)
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
            id='gnome-app',
            name=_('GTK Application (Legacy)'),
            description=_('Create a GTK application with GTK 3'),
            languages=['C', 'C++', 'C♯', 'Python', 'JavaScript', 'Vala', 'Rust'],
            priority=0
         )

    def prepare_files(self, files, language):
        # Shared files
        files['resources/flatpak.json'] = '%(appid)s.json'
        files['resources/data/hello.desktop.in'] = 'data/%(appid)s.desktop.in'
        files['resources/data/hello.appdata.xml.in'] = 'data/%(appid)s.appdata.xml.in'
        files['resources/data/hello.gschema.xml'] = 'data/%(appid)s.gschema.xml'
        files['resources/data/meson.build'] = 'data/meson.build'
        files['resources/data/icons/meson.build'] = 'data/icons/meson.build'
        files['resources/data/icons/hicolor/scalable/apps/hello.svg'] = 'data/icons/hicolor/scalable/apps/%(appid)s.svg'
        files['resources/data/icons/hicolor/symbolic/apps/hello-symbolic.svg'] = 'data/icons/hicolor/symbolic/apps/%(appid)s-symbolic.svg'
        files['resources/po/LINGUAS'] = 'po/LINGUAS'
        files['resources/po/meson.build'] = 'po/meson.build'
        files['resources/po/POTFILES'] = 'po/POTFILES'
        window_ui_name = 'src/window.ui'
        resource_name = 'src/%(prefix)s.gresource.xml'
        meson_file = 'resources/src/meson-c-vala.build'

        if language == 'c':
            files['resources/src/main.c'] = 'src/main.c'
            files['resources/src/window.c'] = 'src/%(prefix)s-window.c'
            files['resources/src/window.h'] = 'src/%(prefix)s-window.h'
            window_ui_name = 'src/%(prefix)s-window.ui'
        elif language == 'c++':
            files['resources/src/main.cpp'] = 'src/main.cpp'
            files['resources/src/window.cpp'] = 'src/%(prefix)s-window.cpp'
            files['resources/src/window.hpp'] = 'src/%(prefix)s-window.h'
            window_ui_name = 'src/%(prefix)s-window.ui'
        elif language == 'c♯':
            files['resources/src/main.cs'] = 'src/main.cs'
            files['resources/src/application.in'] = 'src/%(exec_name)s.in'
            files['resources/flatpak-gtksharp.json.tmpl'] = '%(appid)s.json'
            meson_file = 'resources/src/meson-cs.build'
            resource_name = None
            window_ui_name = None
        elif language == 'vala':
            files['resources/src/main.vala'] = 'src/main.vala'
            files['resources/src/window.vala'] = 'src/window.vala'
        elif language == 'javascript':
            files['resources/src/main.js.tmpl'] = 'src/main.js'
            files['resources/src/hello.js.in'] = 'src/%(appid)s.in'
            files['resources/src/window.js.tmpl'] = 'src/window.js'
            files['resources/src/hello.src.gresource.xml'] = 'src/%(appid)s.src.gresource.xml'
            resource_name = 'src/%(appid)s.data.gresource.xml'
            meson_file = 'resources/src/meson-js.build'
        elif language == 'python':
            files['resources/src/hello.py.in'] = 'src/%(name)s.in'
            files['resources/src/__init__.py'] = 'src/__init__.py'
            files['resources/src/window.py'] = 'src/window.py'
            files['resources/src/main.py'] = 'src/main.py'
            meson_file = 'resources/src/meson-py.build'
        elif language == 'rust':
            files['resources/src/config.rs.in'] = 'src/config.rs.in'
            files['resources/src/main.rs'] = 'src/main.rs'
            files['resources/src/window.rs'] = 'src/window.rs'
            files['resources/src/Cargo.lock'] = 'Cargo.lock'
            files['resources/src/Cargo.toml'] = 'Cargo.toml'
            files['resources/build-aux/cargo.sh'] = 'build-aux/cargo.sh'
            meson_file = 'resources/src/meson-rs.build'

        if resource_name:
            files['resources/src/hello.gresource.xml'] = resource_name
        if window_ui_name:
            files['resources/src/window.ui'] = window_ui_name

        files[meson_file] = 'src/meson.build'

class GnomeGTK4ProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            id='gnome-app-gtk4',
            name=_('GTK Application'),
            description=_('Create a GTK application with GTK 4'),
            languages=['C', 'JavaScript', 'Rust', 'Python', 'Vala'],
            priority=0
         )

    def prepare_files(self, files, language):
        # Shared files
        files['resources/flatpak.json'] = '%(appid)s.json'
        files['resources/data/hello.desktop.in'] = 'data/%(appid)s.desktop.in'
        files['resources/data/hello.appdata.xml.in'] = 'data/%(appid)s.appdata.xml.in'
        files['resources/data/hello.gschema.xml'] = 'data/%(appid)s.gschema.xml'
        files['resources/data/meson.build'] = 'data/meson.build'
        files['resources/data/icons/meson.build'] = 'data/icons/meson.build'
        files['resources/data/icons/hicolor/scalable/apps/hello.svg'] = 'data/icons/hicolor/scalable/apps/%(appid)s.svg'
        files['resources/data/icons/hicolor/symbolic/apps/hello-symbolic.svg'] = 'data/icons/hicolor/symbolic/apps/%(appid)s-symbolic.svg'
        files['resources/po/LINGUAS'] = 'po/LINGUAS'
        files['resources/po/meson.build'] = 'po/meson.build'
        files['resources/po/POTFILES'] = 'po/POTFILES'
        files['resources/src/help-overlay.ui'] = 'src/gtk/help-overlay.ui'
        window_ui_name = 'src/window.ui'
        resource_name = 'src/%(prefix)s.gresource.xml'
        meson_file = 'resources/src/meson-c-vala.build'

        if language == 'c':
            files['resources/src/main-gtk4.c'] = 'src/main.c'
            files['resources/src/window.c'] = 'src/%(prefix)s-window.c'
            files['resources/src/window.h'] = 'src/%(prefix)s-window.h'
            files['resources/src/application.c'] = 'src/%(prefix)s-application.c'
            files['resources/src/application.h'] = 'src/%(prefix)s-application.h'
            window_ui_name = 'src/%(prefix)s-window.ui'
        elif language == 'c++':
            files['resources/src/main.cpp'] = 'src/main.cpp'
            files['resources/src/window.cpp'] = 'src/%(prefix)s-window.cpp'
            files['resources/src/window.hpp'] = 'src/%(prefix)s-window.h'
            window_ui_name = 'src/%(prefix)s-window.ui'
        elif language == 'c♯':
            files['resources/src/main.cs'] = 'src/main.cs'
            files['resources/src/application.in'] = 'src/%(exec_name)s.in'
            files['resources/flatpak-gtksharp.json.tmpl'] = '%(appid)s.json'
            meson_file = 'resources/src/meson-cs.build'
            resource_name = None
            window_ui_name = None
        elif language == 'vala':
            files['resources/src/main-gtk4.vala'] = 'src/main.vala'
            files['resources/src/window-gtk4.vala'] = 'src/window.vala'
            files['resources/src/application-gtk4.vala'] = 'src/application.vala'
        elif language == 'javascript':
            files['resources/src/main-gtk4.js.tmpl'] = 'src/main.js'
            files['resources/src/hello.js.in'] = 'src/%(appid)s.in'
            files['resources/src/window.js.tmpl'] = 'src/window.js'
            files['resources/src/hello.src.gresource.xml'] = 'src/%(appid)s.src.gresource.xml'
            resource_name = 'src/%(appid)s.data.gresource.xml'
            meson_file = 'resources/src/meson-js.build'
        elif language == 'python':
            files['resources/src/hello.py.in'] = 'src/%(name)s.in'
            files['resources/src/__init__.py'] = 'src/__init__.py'
            files['resources/src/window-gtk4.py'] = 'src/window.py'
            files['resources/src/main-gtk4.py'] = 'src/main.py'
            meson_file = 'resources/src/meson-py-gtk4.build'
        elif language == 'rust':
            files['resources/src/application.rs'] = 'src/application.rs'
            files['resources/src/config-gtk4.rs.in'] = 'src/config.rs.in'
            files['resources/src/main-gtk4.rs'] = 'src/main.rs'
            files['resources/src/window-gtk4.rs'] = 'src/window.rs'
            files['resources/src/Cargo.lock'] = 'Cargo.lock'
            files['resources/src/Cargo-gtk4.toml'] = 'Cargo.toml'
            files['resources/build-aux/cargo.sh'] = 'build-aux/cargo.sh'
            meson_file = 'resources/src/meson-rs-gtk4.build'

        if resource_name:
            files['resources/src/hello.gresource.xml'] = resource_name
        if window_ui_name:
            files['resources/src/window-gtk4.ui'] = window_ui_name

        files[meson_file] = 'src/meson.build'

class GnomeAdwaitaProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            id='gnome-app-adwaita',
            name=_('GNOME Application'),
            description=_('Create a GNOME application with libadwaita'),
            languages=['C', 'JavaScript', 'Rust', 'Python', 'Vala'],
            priority=0
         )

    def prepare_files(self, files, language):
        # Shared files
        files['resources/flatpak.json'] = '%(appid)s.json'
        files['resources/data/hello.desktop.in'] = 'data/%(appid)s.desktop.in'
        files['resources/data/hello.appdata.xml.in'] = 'data/%(appid)s.appdata.xml.in'
        files['resources/data/hello.gschema.xml'] = 'data/%(appid)s.gschema.xml'
        files['resources/data/meson.build'] = 'data/meson.build'
        files['resources/data/icons/meson.build'] = 'data/icons/meson.build'
        files['resources/data/icons/hicolor/scalable/apps/hello.svg'] = 'data/icons/hicolor/scalable/apps/%(appid)s.svg'
        files['resources/data/icons/hicolor/symbolic/apps/hello-symbolic.svg'] = 'data/icons/hicolor/symbolic/apps/%(appid)s-symbolic.svg'
        files['resources/po/LINGUAS'] = 'po/LINGUAS'
        files['resources/po/meson.build'] = 'po/meson.build'
        files['resources/po/POTFILES'] = 'po/POTFILES'
        files['resources/src/help-overlay.ui'] = 'src/gtk/help-overlay.ui'
        window_ui_name = 'src/window.ui'
        resource_name = 'src/%(prefix)s.gresource.xml'
        meson_file = 'resources/src/meson-c-vala.build'

        if language == 'c':
            files['resources/src/main-gtk4.c'] = 'src/main.c'
            files['resources/src/window.c'] = 'src/%(prefix)s-window.c'
            files['resources/src/window.h'] = 'src/%(prefix)s-window.h'
            files['resources/src/application.c'] = 'src/%(prefix)s-application.c'
            files['resources/src/application.h'] = 'src/%(prefix)s-application.h'
            window_ui_name = 'src/%(prefix)s-window.ui'
        elif language == 'vala':
            files['resources/src/main-gtk4.vala'] = 'src/main.vala'
            files['resources/src/window-gtk4.vala'] = 'src/window.vala'
            files['resources/src/application-gtk4.vala'] = 'src/application.vala'
        elif language == 'javascript':
            files['resources/src/main-gtk4.js.tmpl'] = 'src/main.js'
            files['resources/src/hello.js.in'] = 'src/%(appid)s.in'
            files['resources/src/window.js.tmpl'] = 'src/window.js'
            files['resources/src/hello.src.gresource.xml'] = 'src/%(appid)s.src.gresource.xml'
            resource_name = 'src/%(appid)s.data.gresource.xml'
            meson_file = 'resources/src/meson-js.build'
        elif language == 'python':
            files['resources/src/hello.py.in'] = 'src/%(name)s.in'
            files['resources/src/__init__.py'] = 'src/__init__.py'
            files['resources/src/window-gtk4.py'] = 'src/window.py'
            files['resources/src/main-gtk4.py'] = 'src/main.py'
            meson_file = 'resources/src/meson-py-gtk4.build'
        elif language == 'rust':
            files['resources/src/application.rs'] = 'src/application.rs'
            files['resources/src/config-gtk4.rs.in'] = 'src/config.rs.in'
            files['resources/src/main-gtk4.rs'] = 'src/main.rs'
            files['resources/src/window-gtk4.rs'] = 'src/window.rs'
            files['resources/src/Cargo.lock'] = 'Cargo.lock'
            files['resources/src/Cargo-gtk4.toml'] = 'Cargo.toml'
            files['resources/build-aux/cargo.sh'] = 'build-aux/cargo.sh'
            meson_file = 'resources/src/meson-rs-gtk4.build'

        if resource_name:
            files['resources/src/hello.gresource.xml'] = resource_name
        if window_ui_name:
            files['resources/src/window-gtk4.ui'] = window_ui_name

        files[meson_file] = 'src/meson.build'

class LibraryProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            id='shared-library',
            name=_("Shared Library"),
            description=_("Create a new project with a shared library"),
            languages=['C'],
            priority=100
         )

    def prepare_files(self, files, language):
        if language == 'c':
            files['resources/src/meson-clib.build'] = 'src/meson.build'
            files['resources/src/hello.c'] = 'src/%(name)s.c'
            files['resources/src/hello.h'] = 'src/%(name)s.h'
            files['resources/src/hello-version.h.in'] = 'src/%(prefix)s-version.h.in'


class EmptyProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            id='empty',
            name=_('Empty Project'),
            description=_('Create a new empty project'),
            languages=['C', 'C++', 'C♯', 'JavaScript', 'Python', 'Vala', 'Rust'],
            priority=200
         )

    def prepare_files(self, files, language):
        files['resources/src/meson-empty.build'] = 'src/meson.build'

        if language == 'rust':
            files['resources/src/Cargo.lock'] = 'Cargo.lock'
            files['resources/src/Cargo-cli.toml'] = 'Cargo.toml'


class CLIProjectTemplate(MesonTemplate):
    def __init__(self):
        super().__init__(
            id='cli',
            name=_('Command Line Tool'),
            description=_('Create a new command line project'),
            languages=['C', 'C++', 'Vala', 'Rust', 'Python'],
            priority=200
         )

    def prepare_files(self, files, language):
        if language == 'python':
            files['resources/src/meson-py-cli.build'] = 'src/meson.build'
        else:
            files['resources/src/meson-cli.build'] = 'src/meson.build'

        if language == 'c':
            files['resources/src/main-cli.c'] = 'src/main.c'
        elif language == 'c++':
            files['resources/src/main-cli.cpp'] = 'src/main.cpp'
        elif language == 'vala':
            files['resources/src/main-cli.vala'] = 'src/main.vala'
        elif language == 'rust':
            files['resources/src/main-cli.rs'] = 'src/main.rs'
            files['resources/src/Cargo.lock'] = 'Cargo.lock'
            files['resources/src/Cargo-cli.toml'] = 'Cargo.toml'
            files['resources/build-aux/cargo.sh'] = 'build-aux/cargo.sh'
        elif language == 'python':
            files['resources/src/hello-cli.py.in'] = 'src/%(name)s.in'
            files['resources/src/__init__.py'] = 'src/__init__.py'
            files['resources/src/main-cli.py'] = 'src/main.py'
