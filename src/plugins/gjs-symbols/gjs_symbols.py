# -*- coding: utf-8 -*-
#
# gjs_symbols.py
#
# Copyright 2017 Patrick Griffi <tingping@tingping.se>
# Copyright 2017 Giovanni Campagna <gcampagn@cs.stanford.edu>
# Copyright 2018 Christian Hergert <chergert@redhat.com>
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
import json
import threading

from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Ide

SYMBOL_PARAM_FLAGS=flags = GObject.ParamFlags.CONSTRUCT_ONLY | GObject.ParamFlags.READWRITE


class JsSymbolNode(Ide.SymbolNode):
    file = GObject.Property(type=Gio.File, flags=SYMBOL_PARAM_FLAGS)
    line = GObject.Property(type=int, flags=SYMBOL_PARAM_FLAGS)
    col = GObject.Property(type=int, flags=SYMBOL_PARAM_FLAGS)

    def __init__(self, children, **kwargs):
        super().__init__(**kwargs)
        assert self.file is not None
        self.children = children

    def do_get_location_async(self, cancellable, callback, user_data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.return_boolean(True)

    def do_get_location_finish(self, result):
        if result.propagate_boolean():
            return Ide.Location.new(self.file, self.line, self.col)

    def __len__(self):
        return len(self.children)

    def __bool__(self):
        return True

    def __getitem__(self, key):
        return self.children[key]

    def __iter__(self):
        return iter(self.children)

    def __repr__(self):
        return '<JsSymbolNode {} ({})'.format(self.props.name, self.props.kind)


class JsSymbolTree(GObject.Object, Ide.SymbolTree):
    def __init__(self, dict_, file_):
        super().__init__()
        # output = file_.get_path().replace('/', '_') + '.json'
        # with open(output, 'w+') as f:
        #    f.write(json.dumps(dict_, indent=3))
        self.root_node = self._node_from_dict(dict_, file_)

    # For now lets try to extract only a small number of useful symbols:
    # - global properties, functions, classes, and gobject classes
    # - methods to those classes
    # This will be expanded upon as time goes on.
    @staticmethod
    def _node_from_dict(dict_, file_):
        line = max(dict_['loc']['start']['line'] - 1, 0)
        col = dict_['loc']['start']['column']

        # FIXME: Recursion is bad in Python, I know..
        type_ = dict_['type']
        if type_ == 'Program':  # Root node
            children = JsSymbolTree._nodes_from_list(dict_['body'], file_)
            return JsSymbolNode(children, line=line, col=col,
                                kind=Ide.SymbolKind.PACKAGE,
                                name=dict_['loc']['source'],
                                file=file_)
        elif type_ == 'FunctionDeclaration':
            return JsSymbolNode([], line=line, col=col,
                                kind=Ide.SymbolKind.FUNCTION,
                                name=dict_['id']['name'],
                                file=file_)
        elif type_ == 'Property':  # Used for legacy GObject classes
            if dict_['value']['type'] != 'FunctionExpression':
                return None
            name = dict_['key']['name']
            if name == '_init':
                return None
            if dict_.get('kind', None) in ('get', 'set'):
                return None
            return JsSymbolNode([], line=line, col=col,
                                kind=Ide.SymbolKind.METHOD,
                                name=name,
                                file=file_)
        elif type_ == 'VariableDeclaration':
            decs = []
            for dec in dict_['declarations']:
                line = max(dec['id']['loc']['start']['line'] - 1, 0)
                col = dec['id']['loc']['start']['column']
                if dec['id']['type'] != 'Identifier':
                    # destructured assignment, ignore
                    return
                name = dec['id']['name']
                kind = Ide.SymbolKind.VARIABLE
                children = []

                if JsSymbolTree._is_module_import(dec):
                    kind = Ide.SymbolKind.MODULE
                    # For now these just aren't useful
                    continue
                elif JsSymbolTree._is_gobject_class(dec):
                    for arg in dec['init']['arguments']:
                        if arg['type'] == 'ClassExpression':
                            line = max(arg['id']['loc']['start']['line'] - 1, 0)
                            col = arg['id']['loc']['start']['column']
                            kind = Ide.SymbolKind.CLASS
                            children = JsSymbolTree._nodes_from_list(arg['body'], file_)
                            name = arg['id']['name']
                            break
                elif JsSymbolTree._is_legacy_gobject_class(dec):
                    kind = Ide.SymbolKind.CLASS
                    try:
                        arg = dec['init']['arguments'][0]
                    except IndexError:
                        continue

                    if arg['type'] != 'ObjectExpression':
                        continue

                    children = JsSymbolTree._nodes_from_list(arg['properties'], file_)
                # elif dict_.get('kind', None) == 'const':
                #    kind = Ide.SymbolKind.CONSTANT
                decs.append(JsSymbolNode(children, line=line, col=col,
                                         kind=kind, name=name, file=file_))
            return decs
        elif type_ == 'ClassStatement':
            children = JsSymbolTree._nodes_from_list(dict_['body'], file_)
            return JsSymbolNode(children, line=line, col=col,
                                kind=Ide.SymbolKind.CLASS,
                                name=dict_['id']['name'],
                                file=file_)
        elif type_ == 'ClassMethod':
            name = dict_['name']['name']
            if name in ('constructed', '_init'):
                return None
            if dict_.get('kind', None) in ('get', 'set'):
                return None
            return JsSymbolNode([], line=line, col=col,
                                kind=Ide.SymbolKind.METHOD,
                                name=name,
                                file=file_)
        elif type_ == 'ExpressionStatement' and JsSymbolTree._is_module_exports(dict_):
            if dict_['expression']['right']['type'] != 'ClassExpression':
                return None
            class_ = dict_['expression']['right']
            children = JsSymbolTree._nodes_from_list(class_['body'], file_)
            line = max(class_['loc']['start']['line'] - 1, 0)
            col = class_['loc']['start']['column']
            return JsSymbolNode(children, line=line, col=col,
                                kind=Ide.SymbolKind.CLASS,
                                name=class_['id']['name'],
                                file=file_)
        else:
            return None

    @staticmethod
    def _is_module_exports(dict_):
        if dict_['expression']['type'] != 'AssignmentExpression':
            return False
        left = dict_['expression']['left']
        if left['type'] != 'MemberExpression':
            return False
        if left['object']['type'] != 'Identifier' or left['object']['name'] != 'module':
            return False
        if left['property']['type'] != 'Identifier' or left['property']['name'] != 'exports':
            return False
        return True

    @staticmethod
    def _is_module_import(dict_):
        try:
            return dict_['init']['object']['name'] == 'imports'
        except (KeyError, TypeError):
            try:
                return dict_['init']['object']['object']['name'] == 'imports'
            except (KeyError, TypeError):
                try:
                    return dict_['init']['callee']['name'] == 'require'
                except (KeyError, TypeError):
                    return False

    @staticmethod
    def _is_gobject_class(dict_):
        try:
            callee = dict_['init']['callee']
            return callee['object']['name'].lower() == 'gobject' and callee['property']['name'] == 'registerClass'
        except (KeyError, TypeError):
            return False

    @staticmethod
    def _is_legacy_gobject_class(dict_):
        try:
            callee = dict_['init']['callee']
            return callee['object']['name'].lower() in ('gobject', 'lang') and callee['property']['name'] == 'Class'
        except (KeyError, TypeError):
            return False

    @staticmethod
    def _nodes_from_list(list_, file_):
        nodes = []
        for i in list_:
            node = JsSymbolTree._node_from_dict(i, file_)
            if node is not None:
                if isinstance(node, list):
                    nodes += node
                else:
                    nodes.append(node)
        return nodes

    def do_get_n_children(self, node):
        return len(node) if node is not None else len(self.root_node)

    def do_get_nth_child(self, node, nth):
        return node[nth] if node is not None else self.root_node[nth]


JS_SCRIPT = \
"""var data;
try {
    if (ARGV[0] === '--file') {
        const GLib = imports.gi.GLib;
        var ret = GLib.file_get_contents(ARGV[1]);
        data = ret[1];
    } else {
        data = ARGV[0];
    }
    print(JSON.stringify(Reflect.parse(data, {source: '%s'})));
} catch (e) {
    imports.system.exit(1);
}
""".replace('\n', ' ')


class GjsSymbolProvider(Ide.Object, Ide.SymbolResolver):
    def __init__(self):
        super().__init__()

    @staticmethod
    def _get_launcher(context, file_):
        file_path = file_.get_path()
        script = JS_SCRIPT % file_path.replace('\\', '\\\\').replace("'", "\\'")
        unsaved_file = Ide.UnsavedFiles.from_context(context).get_unsaved_file(file_)

        if context.has_project():
            runtime = Ide.ConfigManager.from_context(context).get_current().get_runtime()
            launcher = runtime.create_launcher()
        else:
            launcher = Ide.SubprocessLauncher.new(0)

        launcher.set_flags(Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_SILENCE)
        launcher.push_args(('gjs', '-c', script))

        if unsaved_file is not None:
            launcher.push_argv(unsaved_file.get_content().get_data().decode('utf-8'))
        else:
            launcher.push_args(('--file', file_path))

        return launcher

    def do_lookup_symbol_async(self, location, cancellable, callback, user_data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.return_error(GLib.Error('Not implemented'))

    def do_lookup_symbol_finish(self, result):
        result.propagate_boolean()
        return None

    def do_get_symbol_tree_async(self, file_, buffer_, cancellable, callback, user_data=None):
        task = Gio.Task.new(self, cancellable, callback)
        launcher = self._get_launcher(self.get_context(), file_)

        threading.Thread(target=self._get_tree_thread, args=(task, launcher, file_),
                         name='gjs-symbols-thread').start()

    def _get_tree_thread(self, task, launcher, file_):
        try:
            proc = launcher.spawn()
            success, stdout, stderr = proc.communicate_utf8(None, None)

            if not success:
                task.return_error(GLib.Error('Failed to run gjs'))
                return

            task.symbol_tree = JsSymbolTree(json.loads(stdout), file_)
        except GLib.Error as err:
            task.return_error(err)
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            task.return_error(GLib.Error('Failed to decode gjs json: {}'.format(e)))
        except (IndexError, KeyError) as e:
            task.return_error(GLib.Error('Failed to extract information from ast: {}'.format(e)))
        else:
            task.return_boolean(True)

    def do_get_symbol_tree_finish(self, result):
        if result.propagate_boolean():
            return result.symbol_tree

    def do_load(self):
        pass

    def do_find_references_async(self, location, language_id, cancellable, callback, user_data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.return_error(GLib.Error('Not implemented'))

    def do_find_references_finish(self, result):
        return result.propagate_boolean()

    def do_find_nearest_scope_async(self, location, cancellable, callback, user_data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.return_error(GLib.Error('Not implemented'))

    def do_find_nearest_scope_finish(self, result):
        return result.propagate_boolean()


class JsCodeIndexEntries(GObject.Object, Ide.CodeIndexEntries):
    def __init__(self, file, entries):
        super().__init__()
        self.entries = entries
        self.entry_iter = iter(entries)
        self.file = file

    def do_get_next_entry(self):
        if self.entry_iter is not None:
            try:
                return next(self.entry_iter)
            except StopIteration:
                self.entry_iter = None
        return None

    def do_get_file(self):
        return self.file


class GjsCodeIndexer(Ide.Object, Ide.CodeIndexer):
    active = False
    queue = None

    def __init__(self):
        super().__init__()
        self.queue = []

    @staticmethod
    def _get_node_name(node):
        prefix = {
            Ide.SymbolKind.FUNCTION: 'f',
            Ide.SymbolKind.METHOD: 'f',
            Ide.SymbolKind.VARIABLE: 'v',
            Ide.SymbolKind.CONSTANT: 'v',
            Ide.SymbolKind.CLASS: 'c',
        }.get(node.props.kind, 'x')
        return prefix + '\x1F' + node.props.name

    @staticmethod
    def _flatten_node_list(root_node):
        nodes = [root_node]
        for node in root_node:
            nodes += GjsCodeIndexer._flatten_node_list(node)
        return nodes

    def _index_file_cb(self, subprocess, result, task):
        try:
            _, stdout, stderr = subprocess.communicate_utf8_finish(result)

            try:
                root_node = JsSymbolTree._node_from_dict(json.loads(stdout), task.file)
            except (json.JSONDecodeError, UnicodeDecodeError) as e:
                raise GLib.Error('Failed to decode gjs json: {}'.format(e))
            except (IndexError, KeyError) as e:
                raise GLib.Error('Failed to extract information from ast: {}'.format(e))

            builder = Ide.CodeIndexEntryBuilder()

            entries = []
            # TODO: Avoid recreating the same data
            for node in self._flatten_node_list(root_node):
                builder.set_key(node.props.file.get_path() + '|' + node.props.name) # Some unique value..
                builder.set_name(self._get_node_name(node))
                builder.set_kind(node.props.kind)
                builder.set_flags(node.props.flags)
                # Not sure why offset here doesn't match tree
                builder.set_range(node.props.line + 1, node.props.col + 1, 0, 0)
                entries.append(builder.build())

            task.entries = JsCodeIndexEntries(task.file, entries)
            task.return_boolean(True)

        except Exception as ex:
            print(repr(ex))
            task.return_error(GLib.Error(message=repr(ex)))

        finally:
            try:
                if self.queue:
                    task = self.queue.pop(0)
                    launcher = GjsSymbolProvider._get_launcher(self.get_context(), task.file)
                    proc = launcher.spawn()
                    proc.communicate_utf8_async(None, task.get_cancellable(), self._index_file_cb, task)
                    return
            except Exception as ex:
                print(repr(ex))

            self.active = False

    def do_index_file_async(self, file_, build_flags, cancellable, callback, data):
        task = Gio.Task.new(self, cancellable, callback)
        task.entries = None
        task.file = file_

        if self.active:
            self.queue.append(task)
            return

        self.active = True

        launcher = GjsSymbolProvider._get_launcher(self.get_context(), file_)
        proc = launcher.spawn()
        proc.communicate_utf8_async(None, task.get_cancellable(), self._index_file_cb, task)

    def do_index_file_finish(self, result):
        if result.propagate_boolean():
            return result.entries

    def do_generate_key_async(self, location, flags, cancellable, callback, user_data=None):
        # print('generate key')
        task = Gio.Task.new(self, cancellable, callback)
        task.return_error(GLib.Error('Not implemented'))

    def do_generate_key_finish(self, result):
        if result.propagate_boolean():
            return ''
