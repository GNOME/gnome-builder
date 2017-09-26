#!/usr/bin/env python3

#
# html_preview.py
#
# Copyright (C) 2015 Christian Hergert <chris@dronelabs.com>
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

import builtins
import gi
import io
import locale
import os
import shutil
import sys
import subprocess
import threading

gi.require_version('Gtk', '3.0')
gi.require_version('Ide', '1.0')
gi.require_version('WebKit2', '4.0')

from gi.repository import Dazzle
from gi.repository import GLib
from gi.repository import Gio
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Ide
from gi.repository import WebKit2
from gi.repository import Peas

try:
    locale.setlocale(locale.LC_ALL, '')
except:
    pass

can_preview_rst = True
can_preview_sphinx = True
old_open = None

try:
    from docutils.core import publish_string
except ImportError:
    can_preview_rst = False

try:
    import sphinx
except ImportError:
    can_preview_sphinx = False

sphinx_states = {}
sphinx_override = {}


def add_override_file(path, content):
    if path in sphinx_override:
        return False
    else:
        sphinx_override[path] = content.encode('utf-8')
        return True


def remove_override_file(path):
    try:
        del sphinx_override[path]
    except KeyError:
        return False

    return True


def new_open(*args, **kwargs):
    path = args[0]
    if path in sphinx_override:
        return io.BytesIO(sphinx_override[path])

    return old_open(*args, **kwargs)

old_open = builtins.open
builtins.open = new_open

_ = Ide.gettext


class SphinxState():
    def __init__(self, builddir):
        self.builddir = builddir
        self.is_running = False
        self.need_build = False

class HtmlPreviewData(GObject.Object, Ide.ApplicationAddin):
    MARKDOWN_CSS = None
    MARKED_JS = None
    MARKDOWN_VIEW_JS = None

    def do_load(self, app):
        HtmlPreviewData.MARKDOWN_CSS = self.get_data('css/markdown.css')
        HtmlPreviewData.MARKED_JS = self.get_data('js/marked.js')
        HtmlPreviewData.MARKDOWN_VIEW_JS = self.get_data('js/markdown-view.js')

        assert HtmlPreviewData.MARKDOWN_CSS
        assert HtmlPreviewData.MARKED_JS
        assert HtmlPreviewData.MARKDOWN_VIEW_JS

    def do_unload(self, app):
        for state in sphinx_states.items():
            # Be extra sure that we are in the tmp dir
            tmpdir = GLib.get_tmp_dir()
            if state.builddir.startswith(tmpdir):
                shutil.rmtree(state.builddir)

    def get_data(self, name):
        # Hold onto the GBytes to avoid copying the buffer
        path = os.path.join('/org/gnome/builder/plugins/html_preview', name)
        return Gio.resources_lookup_data(path, 0)

class HtmlWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):
    def do_load(self, workbench):
        self.workbench = workbench

        group = Gio.SimpleActionGroup()

        self.install_action = Gio.SimpleAction(name='install-docutils', enabled=True)
        self.install_action.connect('activate', lambda *_: self.install_docutils())
        group.insert(self.install_action)

        self.install_action = Gio.SimpleAction(name='install-sphinx', enabled=True)
        self.install_action.connect('activate', lambda *_: self.install_sphinx())
        group.insert(self.install_action)

        self.workbench.insert_action_group('html-preview', group)

    def do_unload(self, workbench):
        self.workbench = None

    def install_docutils(self):
        transfer = Ide.PkconTransfer(packages=['python3-docutils'])
        manager = Gio.Application.get_default().get_transfer_manager()

        manager.execute_async(transfer, None, self.docutils_installed, None)

    def install_sphinx(self):
        transfer = Ide.PkconTransfer(packages=['python3-sphinx'])
        manager = Gio.Application.get_default().get_transfer_manager()

        manager.execute_async(transfer, None, self.sphinx_installed, None)

    def docutils_installed(self, object, result, data):
        global can_preview_rst
        global publish_string

        try:
            from docutils.core import publish_string
        except ImportError:
            Ide.warning("Failed to load docutils.core module")
            return

        can_preview_rst = True
        self.workbench.pop_message('org.gnome.builder.docutils.install')

    def sphinx_installed(self, object, result, data):
        global can_preview_sphinx
        global sphinx

        try:
            import sphinx
        except ImportError:
            Ide.warning("Failed to load sphinx module")
            return

        can_preview_sphinx = True
        self.workbench.pop_message('org.gnome.builder.sphinx.install')


class HtmlPreviewAddin(GObject.Object, Ide.EditorViewAddin):
    def do_load(self, view):
        self.workbench = view.get_ancestor(Ide.Workbench)
        self.view = view
        self.can_preview = False
        self.sphinx_basedir = None
        self.sphinx_builddir = None

        group = view.get_action_group('editor-view')

        self.action = Gio.SimpleAction(name='preview-as-html', enabled=True)
        self.action.connect('activate', self.preview_activated)
        group.add_action(self.action)

        document = view.get_buffer()
        language = document.get_language()
        language_id = language.get_id() if language else None

        self.do_language_changed(language_id)

        # Add a shortcut for activation inside the editor
        controller = Dazzle.ShortcutController.find(view)
        controller.add_command_action('org.gnome.builder.html-preview.preview',
                                      '<Control><Alt>p',
                                      Dazzle.ShortcutPhase.CAPTURE,
                                      'editor-view.preview-as-html')

    def do_unload(self, view):
        group = view.get_action_group('editor-view')
        group.remove_action('preview-as-html')

        self.view = None
        self.workbench = None

    def do_language_changed(self, language_id):
        enabled = (language_id in ('html', 'markdown', 'rst'))
        self.action.set_enabled(enabled)
        self.lang_id = language_id
        self.can_preview = enabled

        if self.lang_id == 'rst':
            if not self.sphinx_basedir:
                document = self.view.get_buffer()
                path = document.get_file().get_file().get_path()
                self.sphinx_basedir = self.search_sphinx_base_dir(path)

            if self.sphinx_basedir:
                self.sphinx_builddir = self.setup_sphinx_states(self.sphinx_basedir)

        if not enabled:
            self.sphinx_basedir = None
            self.sphinx_builddir = None

    def setup_sphinx_states(self, basedir):
        global sphinx_states

        if basedir in sphinx_states:
            state = sphinx_states[basedir]
            sphinx_builddir = state.builddir
        else:
            sphinx_builddir = GLib.Dir.make_tmp('gnome-builder-sphinx-build-XXXXXX')
            state = SphinxState(sphinx_builddir)
            sphinx_states[basedir] = state

        return sphinx_builddir

    def preview_activated(self, *args):
        global can_preview_rst

        view = self.view
        if view is None:
            return

        if self.lang_id == 'rst':
            if self.sphinx_basedir:
                if not can_preview_sphinx:
                    self.show_missing_sphinx_message(view)
                    return
            elif not can_preview_rst:
                self.show_missing_docutils_message(view)
                return

        document = view.get_buffer()
        web_view = HtmlPreviewView(document,
                                   self.sphinx_basedir,
                                   self.sphinx_builddir,
                                   visible=True)

        column = view.get_ancestor(Ide.LayoutGridColumn)
        grid = column.get_ancestor(Ide.LayoutGrid)
        index = grid.child_get_property(column, 'index')

        # If we are past first stack, use the 0 column stack
        # otherwise create or reuse a stack to the right.
        index += -1 if index > 0 else 1
        column = grid.get_nth_column(index)
        column.add(web_view)

        self.action.set_enabled(False)
        web_view.connect('destroy', self.web_view_destroyed)

    def web_view_destroyed(self, web_view, *args):
        self.action.set_enabled(True)

    def search_sphinx_base_dir(self, path):
        context = self.workbench.get_context()
        vcs = context.get_vcs()
        working_dir = vcs.get_working_directory().get_path()

        try:
            if os.path.commonpath([working_dir, path]) != working_dir:
                working_dir = '/'
        except:
            working_dir = '/'

        folder = os.path.dirname(path)
        level = 10

        while level > 0:
            files = os.scandir(folder)
            for file in files:
                if file.name == 'conf.py':
                    return folder

            if folder == working_dir:
                return None

            level -= 1
            folder = os.path.dirname(folder)

    def show_missing_docutils_message(self, view):
        message = Ide.WorkbenchMessage(
            id='org.gnome.builder.docutils.install',
            title=_('Your computer is missing python3-docutils'),
            show_close_button=True,
            visible=True)

        message.add_action(_('Install'), 'html-preview.install-docutils')
        self.workbench.push_message(message)

    def show_missing_sphinx_message(self, view):
        message = Ide.WorkbenchMessage(
            id='org.gnome.builder.sphinx.install',
            title=_('Your computer is missing python3-sphinx'),
            show_close_button=True,
            visible=True)

        message.add_action(_('Install'), 'html-preview.install-sphinx')
        self.workbench.push_message(message)


class HtmlPreviewView(Ide.LayoutView):
    markdown = False
    rst = False

    def __init__(self, document, sphinx_basedir, sphinx_builddir, *args, **kwargs):
        global old_open

        super().__init__(*args, **kwargs)

        self.sphinx_basedir = sphinx_basedir
        self.sphinx_builddir = sphinx_builddir
        self.document = document

        self.webview = WebKit2.WebView(visible=True, expand=True)
        self.add(self.webview)

        settings = self.webview.get_settings()
        settings.enable_html5_local_storage = False

        language = document.get_language()
        if language:
            id = language.get_id()
            if id == 'markdown':
                self.markdown = True
            elif id == 'rst':
                self.rst = True

        document.connect('notify::title', self.on_title_changed)
        document.connect('changed', self.on_changed)
        self.webview.connect('destroy', self.web_view_destroyed)

        self.on_changed(document)
        self.on_title_changed(document)

    def on_title_changed(self, buffer):
        self.set_title("%s %s" % (buffer.get_title(), _("(Preview)")))

    def web_view_destroyed(self, web_view):
        self.document.disconnect_by_func(self.on_changed)

    def get_markdown(self, text):
        text = text.replace("\"", "\\\"").replace("\n", "\\n")
        params = (HtmlPreviewData.MARKDOWN_CSS.get_data().decode('UTF-8'),
                  text,
                  HtmlPreviewData.MARKED_JS.get_data().decode('UTF-8'),
                  HtmlPreviewData.MARKDOWN_VIEW_JS.get_data().decode('UTF-8'))

        return """
<html>
 <head>
  <style>%s</style>
  <script>var str="%s";</script>
  <script>%s</script>
  <script>%s</script>
 </head>
 <body onload="preview()">
  <div class="markdown-body" id="preview">
  </div>
 </body>
</html>
""" % params

    def get_rst(self, text, path):
        return publish_string(text,
                              writer_name='html5',
                              source_path=path,
                              destination_path=path)

    def get_sphinx_rst_async(self, text, path, basedir, builddir, cancellable, callback):
        task = Gio.Task.new(self, cancellable, callback)
        threading.Thread(target=self.get_sphinx_rst_worker,
                         args=[task, text, path, basedir, builddir],
                         name='sphinx-rst-thread').start()

    def purge_cache(self, basedir, builddir, document):
        path = document.get_file().get_file().get_path()
        rel_path = os.path.relpath(path, start=basedir)
        rel_path_doctree = os.path.splitext(rel_path)[0] + '.doctree'
        doctree_path = os.path.join(builddir, '.doctrees', rel_path_doctree)

        tmpdir = GLib.get_tmp_dir()
        if doctree_path.startswith(tmpdir):
            try:
                os.remove(doctree_path)
            except:
                pass

    def get_sphinx_rst_worker(self, task, text, path, basedir, builddir):
        add_override_file(path, text)

        rel_path = os.path.relpath(path, start=basedir)
        command = ['sphinx-build', '-Q', '-b', 'html', basedir, builddir, path]

        rel_path_html = os.path.splitext(rel_path)[0] + '.html'
        builddir_path = os.path.join(builddir, rel_path_html)

        result = not sphinx.build_main(command)
        remove_override_file(path)

        if not result:
            task.builddir_path = None
            task.return_error(GLib.Error('\'sphinx-build\' command error for {}'.format(path)))
            return

        task.builddir_path = builddir_path
        task.return_boolean(True)

    def get_sphinx_rst_finish(self, result):
        succes = result.propagate_boolean()
        builddir_path = result.builddir_path

        return builddir_path

    def get_sphinx_state(self, basedir):
        global sphinx_states

        try:
            state = sphinx_states[basedir]
        except KeyError:
            return None

        return state

    def reload(self):
        state = self.get_sphinx_state(self.sphinx_basedir)
        if state and state.is_running:
            state.need_build = True
            return

        gfile = self.document.get_file().get_file()
        base_uri = gfile.get_uri()

        begin, end = self.document.get_bounds()
        text = self.document.get_text(begin, end, True)

        if self.markdown:
            text = self.get_markdown(text)
        elif self.rst:
            if self.sphinx_basedir:
                self.purge_cache(self.sphinx_basedir, self.sphinx_builddir, self.document)
                state.is_running = True

                self.get_sphinx_rst_async(text,
                                          gfile.get_path(),
                                          self.sphinx_basedir,
                                          self.sphinx_builddir,
                                          None,
                                          self.get_sphinx_rst_cb)

                return
            else:
                text = self.get_rst(text, gfile.get_path()).decode("utf-8")

        self.webview.load_html(text, base_uri)

    def get_sphinx_rst_cb(self, obj, result):
        builddir_path = self.get_sphinx_rst_finish(result)
        if builddir_path:
            uri = 'file:///' + builddir_path
            self.webview.load_uri(uri)

        state = self.get_sphinx_state(self.sphinx_basedir)
        state.is_running = False
        if state.need_build:
            state.need_build = False
            self.reload()

    def on_changed(self, document):
        self.reload()
