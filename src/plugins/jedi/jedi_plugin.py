#!/usr/bin/env python3

#
# jedi_plugin.py
#
# Copyright 2015 Elad Alfassa <elad@fedoraproject.org>
# Copyright 2015-2019 Christian Hergert <chris@dronelabs.com>
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
# lxml is faster than the Python standard library xml, and can ignore invalid
# characters (which occur in some .gir files).
# gnome-code-assistance also needs lxml, so I think it's okay to use it here.
try:
    import lxml.etree
    HAS_LXML = True
except ImportError:
    HAS_LXML = False
    print('Warning: python3-lxml is not installed, no documentation will be available in Python auto-completion')
import os
import os.path
import sqlite3
import threading

from collections import OrderedDict

from gi.importer import DynamicImporter
from gi.module import IntrospectionModule
from gi.module import FunctionInfo

from gi.repository import GIRepository
from gi.repository import Gio
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gtk
from gi.repository import GtkSource
from gi.repository import Ide
from gi.types import GObjectMeta
from gi.types import StructMeta

_ = Ide.gettext

gi_importer = DynamicImporter('gi.repository')

_TYPE_KEYWORD = 1
_TYPE_FUNCTION = 2
_TYPE_CLASS = 3
_TYPE_INSTANCE = 4
_TYPE_PARAM = 5
_TYPE_IMPORT = 6
_TYPE_MODULE = 7

_TYPES = {
    'class': _TYPE_CLASS,
    'function': _TYPE_FUNCTION,
    'import': _TYPE_IMPORT,
    'instance': _TYPE_INSTANCE,
    'keyword': _TYPE_KEYWORD,
    'module':  _TYPE_MODULE,
    'param': _TYPE_PARAM,
}

_ICONS = {
    _TYPE_KEYWORD: 'lang-class-symbolic',
    _TYPE_FUNCTION: 'lang-function-symbolic',
    _TYPE_CLASS: 'lang-class-symbolic',
    _TYPE_INSTANCE: 'lang-variable-symbolic',
    _TYPE_PARAM: 'lang-variable-symbolic',
    _TYPE_IMPORT: 'lang-namespace-symbolic',
    _TYPE_MODULE: 'lang-namespace-symbolic',
}

_PROXY_TASK = None

def return_error(task, error):
    #print(repr(error))
    task.return_error(GLib.Error(message=repr(error)))

def patch_jedi():
    try:
        from jedi.evaluate.compiled import CompiledObject
        from jedi.evaluate.compiled import get_special_object
        try:
            # 0.12
            from jedi.evaluate.compiled import create_from_name
            from jedi.evaluate.base_context import Context
        except ImportError:
            # Pre 0.12
            from jedi.evaluate.compiled import _create_from_name as create_from_name
            from jedi.evaluate.context import Context
        from jedi.evaluate.docstrings import _evaluate_for_statement_string
        from jedi.evaluate.imports import Importer

        class PatchedJediCompiledObject(CompiledObject):
            "A modified version of Jedi CompiledObject to work with GObject Introspection modules"

            def __init__(self, evaluator, obj, parent_context=None, faked_class=None):
                # we have to override __init__ to change super(CompiledObject, self)
                # to Context, in order to prevent an infinite recursion
                Context.__init__(self, evaluator, parent_context)
                self.obj = obj
                self.tree_node = faked_class

            def _cls(self):
                if self.obj.__class__ == IntrospectionModule:
                    return self
                else:
                    return super()._cls(self)

            @property
            def py__call__(self):
                def actual(params):
                    # Parse the docstring to find the return type:
                    ret_type = ''
                    if '->' in self.obj.__doc__:
                        ret_type = self.obj.__doc__.split('->')[1].strip()
                        ret_type = ret_type.replace(' or None', '')
                    if ret_type.startswith('iter:'):
                        ret_type = ret_type[len('iter:'):]  # we don't care if it's an iterator

                    if hasattr(__builtins__, ret_type):
                        # The function we're inspecting returns a builtin python type, that's easy
                        # (see test/test_evaluate/test_compiled.py in the jedi source code for usage)
                        builtins = get_special_object(self.evaluator, 'BUILTINS')
                        builtin_obj = builtins.py__getattribute__(ret_type)
                        obj = _create_from_name(self.evaluator, builtins, builtin_obj, "")
                        return self.evaluator.execute(obj, params)
                    else:
                        # The function we're inspecting returns a GObject type
                        parent = self.parent_context.obj.__name__
                        if parent.startswith('gi.repository'):
                            parent = parent[len('gi.repository.'):]
                        else:
                            # a module with overrides, such as Gtk, behaves differently
                            parent_module = self.parent_context.obj.__module__
                            if parent_module.startswith('gi.overrides'):
                                parent_module = parent_module[len('gi.overrides.'):]
                                parent = '%s.%s' % (parent_module, parent)

                        if ret_type.startswith(parent):
                            # A pygobject type in the same module
                            ret_type = ret_type[len(parent):]
                        else:
                            # A pygobject type in a different module
                            return_type_parent = ret_type.split('.', 1)[0]
                            ret_type = 'from gi.repository import %s\n%s' % (return_type_parent, ret_type)
                        result = _evaluate_for_statement_string(self.parent_context, ret_type)
                        return set(result)
                if type(self.obj) == FunctionInfo:
                    return actual
                return super().py__call__

        # we need to override CompiledBoundMethod without changing it much,
        # just so it'll not get confused due to our overriden CompiledObject
        class PatchedCompiledBoundMethod(PatchedJediCompiledObject):
            def __init__(self, func):
                super().__init__(func.evaluator, func.obj, func.parent_context, func.tree_node)

        class PatchedJediImporter(Importer):
            "A modified version of Jedi Importer to work with GObject Introspection modules"
            def follow(self):
                module_list = super().follow()
                if not module_list:
                    import_path = '.'.join([str(i) for i in self.import_path])
                    if import_path.startswith('gi.repository'):
                        try:
                            module = gi_importer.load_module(import_path)
                            module_list = [PatchedJediCompiledObject(self._evaluator, module)]
                        except ImportError:
                            pass
                return module_list

        try:
            # Pre 0.12 workaround
            # TODO: What needs to be fixed here for 0.12?
            original_jedi_get_module = jedi.evaluate.compiled.fake.get_module
            def patched_jedi_get_module(obj):
                "Work around a weird bug in jedi"
                try:
                    return original_jedi_get_module(obj)
                except ImportError as e:
                    if e.msg == "No module named 'gi._gobject._gobject'":
                        return original_jedi_get_module('gi._gobject')
            jedi.evaluate.compiled.fake.get_module = patched_jedi_get_module
        except:
            pass

        jedi.evaluate.compiled.CompiledObject = PatchedJediCompiledObject
        try:
            jedi.evaluate.instance.CompiledBoundMethod = PatchedCompiledBoundMethod
        except AttributeError:
            jedi.evaluate.context.instance.CompiledBoundMethod = PatchedCompiledBoundMethod
        jedi.evaluate.imports.Importer = PatchedJediImporter
        return True
    except ImportError as ex:
        print("jedi patching not possible. GI completion may not work.")
        print(ex)
        return False

try:
    import jedi
except ImportError:
    print("jedi not found, python auto-completion not possible.")
    IS_JEDI_PATCHED = False
else:
    IS_JEDI_PATCHED = patch_jedi()
GIR_PATH_LIST = []

def init_gir_path_list():
    global GIR_PATH_LIST
    paths = OrderedDict()

    # Use GI_TYPELIB_PATH and the search path used by gobject-introspection
    # to guess the correct path of gir files. It is likely that gir files and
    # typelib files are installed in the same prefix.
    search_path = GIRepository.Repository.get_default().get_search_path()
    if 'GI_TYPELIB_PATH' in os.environ:
        search_path = os.environ['GI_TYPELIB_PATH'].split(':') + search_path

    for typelib_path in search_path:
        # Check whether the path is end with lib*/girepository-1.0. If
        # not, it is likely to be a custom path used for testing that
        # we should not use.
        typelib_path = os.path.normpath(typelib_path)
        typelib_basename = os.path.basename(typelib_path)
        if typelib_basename != 'girepository-1.0':
            continue

        path_has_lib = False
        gir_path, gir_basename = typelib_path, typelib_basename
        while gir_basename != '':
            gir_path = os.path.normpath(os.path.join(gir_path, os.path.pardir))
            gir_basename = os.path.basename(gir_path)
            if gir_basename.startswith('lib'):
                path_has_lib = True
                break

        if not path_has_lib:
            continue
        # Replace lib component with share.
        gir_path = os.path.normpath(os.path.join(gir_path, os.path.pardir,
            'share', os.path.relpath(typelib_path, gir_path)))
        # Replace girepository-1.0 component with gir-1.0.
        gir_path = os.path.normpath(os.path.join(gir_path, os.path.pardir,
            'gir-1.0'))
        paths[gir_path] = None

    # It is also possible for XDG_DATA_DIRS to contain a list of prefixes.
    if 'XDG_DATA_DIRS' in os.environ:
        for xdg_data_path in os.environ['XDG_DATA_DIRS'].split(':'):
            gir_path = os.path.normpath(os.path.join(
                xdg_data_path, 'gir-1.0'))
            paths[gir_path] = None

    # Ignore non-existent directories to prevent exceptions.
    GIR_PATH_LIST = list(filter(os.path.isdir, paths.keys()))

init_gir_path_list()

class DocumentationDB(object):
    def __init__(self):
        self.db = None
        self.cursor = None

    def close(self):
        "Close the DB if open"
        if self.db is not None:
            self.cursor.close()
            self.db.close()
            self.cursor = None
            self.db = None

    def open(self):
        "Open the DB (if needed)"
        if self.db is None:
            doc_db_path = os.path.join(GLib.get_user_cache_dir(), 'gnome-builder', 'jedi', 'girdoc.db')
            try:
                os.makedirs(os.path.dirname(doc_db_path))
            except:
                pass
            self.db = sqlite3.connect(doc_db_path)
            self.cursor = self.db.cursor()
            # Create the tables if they don't exist to prevent exceptions later on
            self.cursor.execute('CREATE TABLE IF NOT EXISTS doc (symbol text, library_version text, doc text, gir_file text)')
            self.cursor.execute('CREATE TABLE IF NOT EXISTS girfiles (file text, last_modified integer)')

    def query(self, symbol, version):
        "Query the documentation DB"
        self.open()
        self.cursor.execute('SELECT doc FROM doc WHERE symbol=? AND library_version=?', (symbol, version))
        result = self.cursor.fetchone()
        if result is not None:
            return result[0]
        else:
            return None

    def update(self, close_when_done=False):
        "Build the documentation DB and ensure it's up to date"
        if not HAS_LXML:
            return  # Can't process the gir files without lxml
        ns = {'core': 'http://www.gtk.org/introspection/core/1.0',
              'c': 'http://www.gtk.org/introspection/c/1.0'}
        self.open()
        cursor = self.cursor
        processed_gir_files = {}

        # I would use scandir for better performance, but it requires newer Python
        for gir_path in GIR_PATH_LIST:
            for gir_file in os.listdir(gir_path):
                if not gir_file.endswith('.gir'):
                    continue
                if gir_file in processed_gir_files:
                    continue
                processed_gir_files[gir_file] = None
                filename = os.path.join(gir_path, gir_file)
                mtime = os.stat(filename).st_mtime
                cursor.execute('SELECT * from girfiles WHERE file=?', (filename,))
                result = cursor.fetchone()
                if result is None:
                    cursor.execute('INSERT INTO girfiles VALUES (?, ?)', (filename, mtime))
                else:
                    if result[1] >= mtime:
                        continue
                    else:
                        # updated
                        cursor.execute('DELETE FROM doc WHERE gir_file=?', (filename,))
                        cursor.execute('UPDATE girfiles SET last_modified=? WHERE file=?', (mtime, filename))
                parser = lxml.etree.XMLParser(recover=True)
                tree = lxml.etree.parse(filename, parser=parser)
                try:
                    namespace = tree.find('core:namespace', namespaces=ns)
                except:
                    print("Failed to parse", filename)
                    continue
                library_version = namespace.attrib['version']
                for node in namespace.findall('core:class', namespaces=ns):
                    doc = node.find('core:doc', namespaces=ns)
                    if doc is not None:
                        symbol = node.attrib['{http://www.gtk.org/introspection/glib/1.0}type-name']
                        cursor.execute("INSERT INTO doc VALUES (?, ?, ?, ?)",
                                       (symbol, library_version, doc.text, filename))
                for method in namespace.findall('core:method', namespaces=ns) + \
                              namespace.findall('core:constructor', namespaces=ns) + \
                              namespace.findall('core:function', namespaces=ns):
                    doc = method.find('core:doc', namespaces=ns)
                    if doc is not None:
                        symbol = method.attrib['{http://www.gtk.org/introspection/c/1.0}identifier']
                        cursor.execute("INSERT INTO doc VALUES (?, ?, ?, ?)",
                                       (symbol, library_version, doc.text, filename))
        self.db.commit()
        if close_when_done:
            self.close()


def update_doc_db_on_startup():
    db = DocumentationDB()
    threading.Thread(target=db.update, args={'close_when_done': True}).start()

update_doc_db_on_startup()

def get_proxy_cb(app, result, task):
    try:
        worker = app.get_worker_finish(result)
        task.return_object(worker)
    except Exception as ex:
        return_error(task, ex)

def get_proxy_async(cancellable, callback, data):
    global _PROXY_TASK

    task = Ide.Task.new(None, None, callback, data)
    task.set_name('get-jedi-proxy')

    if _PROXY_TASK:
        _PROXY_TASK.chain(task)
        return

    task.set_release_on_propagate(False)
    _PROXY_TASK = task

    app = Gio.Application.get_default()
    app.get_worker_async('jedi_plugin', None, get_proxy_cb, task)

def get_proxy_finish(result):
    return result.propagate_object()

def code_complete_cb(proxy, result, task):
    try:
        variant = proxy.call_finish(result)
        task.variant = variant.get_child_value(0)
        task.return_boolean(True)
    except Exception as ex:
        return_error(task, ex)

def code_complete_async(proxy, filename, line, line_offset, text, cancellable, callback, data=None):
    task = Ide.Task.new(proxy, cancellable, callback, data)
    task.set_name('jedi-code-complete')

    proxy.call('CodeComplete',
               GLib.Variant('(siis)', (filename, line, line_offset, text)),
               0, 10000, cancellable, code_complete_cb, task)

def code_complete_finish(proxy, task):
    task.propagate_boolean()
    return task.variant

class JediCompletionProvider(Ide.Object, Ide.CompletionProvider):
    context = None
    current_word = None
    results = None
    thread = None
    line = -1
    line_offset = -1
    loading_proxy = False

    proxy = None

    def do_get_title(self):
        return 'Jedi'

    def do_get_icon(self):
        return None

    def do_get_priority(self, context):
        return 200

    def do_load(self, context):
        pass

    def do_get_comment(self, proposal):
        return proposal.get_comment()

    def do_activate_proposal(self, context, proposal, key):
        buffer = context.get_buffer()
        view = context.get_view()

        _, begin, end = context.get_bounds()

        snippet = proposal.get_snippet()

        buffer.begin_user_action()
        buffer.delete(begin, end)
        view.push_snippet(snippet, begin)
        buffer.end_user_action()

    def do_display_proposal(self, row, context, typed_text, proposal):
        casefold = typed_text.lower()

        row.set_icon_name(proposal.get_icon_name())
        row.set_left(None)
        row.set_center_markup(proposal.get_markup(casefold))
        row.set_right(None)

    def do_is_trigger(self, iter, ch):
        if ch == '.':
            buffer = iter.get_buffer()
            if buffer.iter_has_context_class(iter, 'string') or \
               buffer.iter_has_context_class(iter, 'comment'):
                return False
            return True
        return False

    def do_populate_async(self, context, cancellable, callback, data):
        task = Ide.Task.new(self, None, callback)
        task.set_name('jedi-completion')

        self.current_word = context.get_word()
        self.current_word_lower = self.current_word.lower()

        _, iter, _ = context.get_bounds()

        buffer = context.get_buffer()

        begin, end = buffer.get_bounds()

        task.filename = buffer.get_file().get_path()
        task.line = iter.get_line()
        task.line_offset = iter.get_line_offset()
        #if task.line_offset > 0:
        #    task.line_offset -= 1
        task.text = buffer.get_text(begin, end, True)

        def code_complete_cb(obj, result, task):
            try:
                variant = code_complete_finish(obj, result)
                task.return_object(JediResults(variant))
            except Exception as ex:
                return_error(task, ex)

        def get_proxy_cb(obj, result, task):
            try:
                proxy = get_proxy_finish(result)
                code_complete_async(proxy, task.filename, task.line, task.line_offset, task.text, task.get_cancellable(), code_complete_cb, task)
            except Exception as ex:
                return_error(task, ex)

        get_proxy_async(None, get_proxy_cb, task)

    def do_populate_finish(self, result):
        return result.propagate_object()

    def do_refilter(self, context, model):
        word = context.get_word().lower()
        model.refilter(word)
        return True

class JediCompletionRequest:
    did_run = False
    cancelled = False

    def __init__(self, invocation, filename, line, column, content):
        assert(type(line) == int)
        assert(type(column) == int)

        self.invocation = invocation
        self.filename = filename
        self.line = line
        self.column = column
        self.content = content

    def run(self):
        try:
            if not self.cancelled:
                self._run()
        except Exception as ex:
            self.invocation.return_error_literal(Gio.dbus_error_quark(), Gio.DBusError.IO_ERROR, repr(ex))

    def _run(self):
        self.did_run = True

        results = []

        # Jedi uses 1-based line indexes, we use 0 throughout Builder.
        script = jedi.Script(self.content, self.line + 1, self.column, self.filename)

        db = DocumentationDB()

        def get_gi_obj(info):
            """ Get a GObject Introspection object from a jedi Completion, or None if the completion is not GObject Introspection related """
            if not IS_JEDI_PATCHED:
                return None
            if (type(info._module) == PatchedJediCompiledObject and
               info._module.obj.__class__ == IntrospectionModule):
                return next(info._name.infer()).obj
            else:
                return None

        for info in script.completions():
            if self.cancelled:
                return

            params = []

            # we have to use custom names here because .type and .params can't
            # be overridden (they are properties)
            obj = get_gi_obj(info)
            if type(obj) == FunctionInfo:
                    info.real_type = 'function'
                    params = [arg_info.get_name() for arg_info in obj.get_arguments()]
            else:
                info.real_type = info.type
                if hasattr(info, 'params'):
                    if len(info.params) > 0 and \
                       info.params[0].name == 'self':
                        del info.params[0]
                    for param in info.params:
                        if hasattr(param, 'description'):
                            params.append(param.description.replace('\n', ''))
                        else:
                            params.append(param.name)

            doc = info.docstring()
            if obj is not None:
                # get documentation for this GObject Introspection object
                symbol = None
                namespace = None

                if type(obj) == GObjectMeta or type(obj) == StructMeta:
                    if hasattr(obj, '__info__'):
                        symbol = obj.__info__.get_type_name()
                        namespace = obj.__info__.get_namespace()
                elif type(obj) == FunctionInfo:
                    symbol = obj.get_symbol()
                    namespace = obj.get_namespace()

                if symbol is not None:
                    result = db.query(symbol, info._module.obj._version)
                    if result is not None:
                        doc = result

            results.append((_TYPES.get(info.real_type, 0), info.name, info.complete, params, doc))

        db.close()

        self.invocation.return_value(GLib.Variant('(a(issass))', (results,)))

    def cancel(self):
        if not self.cancelled and not self.did_run:
            self.cancelled = True
            self.invocation.return_error_literal(Gio.io_error_quark(), Gio.IOErrorEnum.CANCELLED, "Operation was cancelled")

class JediService(Ide.DBusService):
    queue = None
    handler_id = None

    def __init__(self):
        super().__init__()
        self.queue = {}
        self.handler_id = 0

    @Ide.DBusMethod('org.gnome.builder.plugins.jedi', in_signature='siis', out_signature='a(issass)', is_async=True)
    def CodeComplete(self, invocation, filename, line, column, content):
        if filename in self.queue:
            request = self.queue.pop(filename)
            request.cancel()
        self.queue[filename] = JediCompletionRequest(invocation, filename, line, column, content)
        if not self.handler_id:
            self.handler_id = GLib.timeout_add(5, self.process)

    def process(self):
        self.handler_id = 0
        while self.queue:
            filename, request = self.queue.popitem()
            request.run()
        return False

class JediWorker(GObject.Object, Ide.Worker):
    _service = None

    def do_register_service(self, connection):
        self._service = JediService()
        self._service.export(connection, '/')

    def do_create_proxy(self, connection):
        return Gio.DBusProxy.new_sync(connection,
                                      (Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES |
                                       Gio.DBusProxyFlags.DO_NOT_CONNECT_SIGNALS |
                                       Gio.DBusProxyFlags.DO_NOT_AUTO_START_AT_CONSTRUCTION),
                                      None,
                                      None,
                                      '/',
                                      'org.gnome.builder.plugins.jedi',
                                      None)


class JediPreferences(GObject.Object, Ide.PreferencesAddin):
    def do_load(self, prefs):
        self.completion_id = prefs.add_switch(
                'completion', 'providers',
                'org.gnome.builder.extension-type', 'enabled',
                '/org/gnome/builder/extension-types/jedi_plugin/IdeCompletionProvider/',
                None,
                _("Suggest completions from Python"),
                _("Use Jedi to provide completions for the Python language"),
                None, 30)

    def do_unload(self, prefs):
        prefs.remove_id(self.completion_id)

class JediResults(GObject.Object, Gio.ListModel):
    variant = None
    items = None

    def __init__(self, variant, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.variant = variant
        self.items = []
        for i in range(variant.n_children()):
            self.items.append((i,0))

    def do_get_item_type(self):
        return type(Ide.CompletionProposal)

    def do_get_n_items(self):
        return len(self.items)

    def do_get_item(self, position):
        idx,_ = self.items[position]
        return JediResult(self.variant.get_child_value(idx))

    def refilter(self, casefold):
        old_len = len(self.items)
        self.items = []
        for i in range(self.variant.n_children()):
            child = self.variant.get_child_value(i)
            label = child.get_child_value(1).get_string()
            match, priority = Ide.Completion.fuzzy_match(label, casefold)
            if match:
                self.items.append((i, priority))
        self.items.sort(key=lambda x: x[1])
        self.items_changed(0, old_len, len(self.items))

class JediResult(GObject.Object, Ide.CompletionProposal):
    def __init__(self, variant):
        super().__init__()
        self.variant = variant

    def get_kind(self):
        return self.variant.get_child_value(0).get_int32()

    def get_icon_name(self):
        kind = self.get_kind()
        return _ICONS.get(kind, None)

    def get_label(self):
        return self.variant.get_child_value(1).get_string()

    def get_markup(self, casefold):
        markup = Ide.Completion.fuzzy_highlight(self.get_label(), casefold)

        kind = self.get_kind()
        if kind == _TYPE_FUNCTION:
            parts = []
            params = self.get_params()
            for p in params:
                if p.startswith('param '):
                    parts.append(p[6:])
            markup += ''.join(['<span fgalpha="32762">(', ', '.join(parts), ')</span>'])

        return markup

    def get_text(self):
        return self.variant.get_child_value(2).get_string()

    def get_comment(self):
        comment = self.variant.get_child_value(4).get_string()
        if comment and '\n' in comment:
            return comment[:comment.index('\n')]
        return comment

    def get_params(self):
        return self.variant.get_child_value(3).unpack()

    def get_snippet(self):
        snippet = Ide.Snippet.new(None, None)
        snippet.add_chunk(Ide.SnippetChunk(spec=self.get_text()))

        # Add parameter completion for functions.
        kind = self.get_kind()
        if kind == _TYPE_FUNCTION:
            snippet.add_chunk(Ide.SnippetChunk(text='(', text_set=True))
            params = self.get_params()
            if params:
                tab_stop = 0
                for param in params[:-1]:
                    tab_stop += 1
                    if param.startswith('param '):
                        param = param[6:]
                    snippet.add_chunk(Ide.SnippetChunk(text=param, text_set=True, tab_stop=tab_stop))
                    snippet.add_chunk(Ide.SnippetChunk(text=', ', text_set=True))
                tab_stop += 1
                snippet.add_chunk(Ide.SnippetChunk(text=params[-1], text_set=True, tab_stop=tab_stop))
            snippet.add_chunk(Ide.SnippetChunk(text=')', text_set=True))

        return snippet

