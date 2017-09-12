import gi
import os
import json
import threading

gi.require_versions({
    'Ide': '1.0',
})

from gi.repository import (
    GLib,
    GObject,
    Gio,
    Ide,
)


SYMBOL_PARAM_FLAGS=flags = GObject.ParamFlags.CONSTRUCT_ONLY | GObject.ParamFlags.READWRITE


class JsSymbolNode(Ide.SymbolNode):
    file = GObject.Property(type=Ide.File, flags=SYMBOL_PARAM_FLAGS)
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
            return Ide.SourceLocation.new(self.file, self.line, self.col, 0)

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
        else:
            return None

    @staticmethod
    def _is_module_import(dict_):
        try:
            return dict_['init']['object']['name'] == 'imports'
        except (KeyError, TypeError):
            try:
                return dict_['init']['object']['object']['name'] == 'imports'
            except (KeyError, TypeError):
                return False

    @staticmethod
    def _is_gobject_class(dict_):
        try:
            callee = dict_['init']['callee']
            return callee['object']['name'].lower() == 'gobject' and callee['property']['name'] == 'registerClass'
        except KeyError:
            return False

    @staticmethod
    def _is_legacy_gobject_class(dict_):
        try:
            callee = dict_['init']['callee']
            return callee['object']['name'].lower() in ('gobject', 'lang') and callee['property']['name'] == 'Class'
        except KeyError:
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
if (ARGV[0] === '--file') {
  const GLib = imports.gi.GLib;
  var ret = GLib.file_get_contents(ARGV[1]);
  data = ret[1];
} else {
  data = ARGV[0];
}
print(JSON.stringify(Reflect.parse(data, {source: '%s'})));""".replace('\n', ' ')


class GjsSymbolProvider(Ide.Object, Ide.SymbolResolver):
    def __init__(self):
        super().__init__()

    @staticmethod
    def _get_launcher(context, file_):
        file_path = file_.get_path()
        script = JS_SCRIPT %file_path
        unsaved_file = context.get_unsaved_files().get_unsaved_file(file_)

        pipeline = context.get_build_manager().get_pipeline()
        runtime = pipeline.get_configuration().get_runtime()

        launcher = runtime.create_launcher()
        launcher.set_flags(Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_SILENCE)
        launcher.push_args(('gjs', '-c', script))
        if unsaved_file is not None:
            launcher.push_argv(unsaved_file.get_content().get_data().decode('utf-8'))
        else:
            launcher.push_args(('--file', file_path))
        return launcher

    def do_lookup_symbol_async(self, location, cancellable, callback, user_data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.return_boolean(False)  # Not implemented

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
                task.return_boolean(False)
                return

            ide_file = Ide.File(file=file_, context=self.get_context())
            task.symbol_tree = JsSymbolTree(json.loads(stdout), ide_file)
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

    def do_find_references_async(self, location, cancellable, callback, user_data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.return_boolean(False)  # Not implemented

    def do_find_references_finish(self, result):
        result.propagate_boolean()
        return None

    def do_find_nearest_scope_async(self, location, cancellable, callback, user_data=None):
        task = Gio.Task.new(self, cancellable, callback)
        task.return_boolean(False)  # Not implemented

    def do_find_nearest_scope_finish(self, result):
        result.propagate_boolean()
        return None


class JsCodeIndexEntries(GObject.Object, Ide.CodeIndexEntries):
    def __init__(self, entries):
        super().__init__()
        self.entries = entries
        self.entry_iter = None

    def do_get_next_entry(self):
        if self.entry_iter is None:
            self.entry_iter = iter(self.entries)
        try:
            return next(self.entry_iter)
        except StopIteration:
            self.entry_iter = None
            return None


class GjsCodeIndexer(Ide.Object, Ide.CodeIndexer):
    def __init__(self):
        super().__init__()

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

    def do_index_file(self, file_, build_flags, cancellable):
        if 'node_modules' in file_.get_path().split(os.sep):
            return None  # Avoid indexing these

        launcher = GjsSymbolProvider._get_launcher(self.get_context(), file_)
        proc = launcher.spawn()
        success, stdout, stderr = proc.communicate_utf8(None, None)

        if not success:
            return None

        ide_file = Ide.File(file=file_, context=self.get_context())
        try:
            root_node = JsSymbolTree._node_from_dict(json.loads(stdout), ide_file)
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            raise GLib.Error('Failed to decode gjs json: {}'.format(e))
        except (IndexError, KeyError) as e:
            raise GLib.Error('Failed to extract information from ast: {}'.format(e))

        entries = []
        # TODO: Avoid recreating the same data
        for node in self._flatten_node_list(root_node):
            entry = Ide.CodeIndexEntry(
                key=node.props.file.get_path() + '|' + node.props.name, # Some unique value..
                name=self._get_node_name(node),
                kind=node.props.kind,
                flags=node.props.flags,
                begin_line=node.props.line,
                begin_line_offset=node.props.col,
            )
            entries.append(entry)
        return JsCodeIndexEntries(entries)

    def do_generate_key_async(self, location, cancellable, callback, user_data=None):
        # print('generate key')
        task = Gio.Task.new(self, cancellable, callback)
        task.return_boolean(False)

    def do_generate_key_finish(self, result):
        if result.propagate_boolean():
            return ''
