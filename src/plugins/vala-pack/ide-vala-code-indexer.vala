/* ide-vala-indenter.vala
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using GLib;
using Ide;

namespace Ide
{
	bool is_null_or_empty (string? s)
	{
		return s == null || s[0] == '\0';
	}

	public class ValaCodeIndexer : Ide.Object, Ide.CodeIndexer
	{
		public async Ide.CodeIndexEntries index_file_async (GLib.File file,
		                                                    [CCode (array_length = false, array_null_terminated = true)] string[]? build_flags,
		                                                    GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			var context = this.get_context ();
			var service = (Ide.ValaService)context.get_service_typed (typeof (Ide.ValaService));
			var index = service.index;
			var tree = index.get_symbol_tree_sync (file, cancellable);

			Ide.CodeIndexEntries? ret = null;

			Ide.ThreadPool.push (Ide.ThreadPoolKind.INDEXER, () => {
				index.do_locked (_ => {
					ret = new Ide.ValaCodeIndexEntries (file, tree as Ide.ValaSymbolTree);
				});
				GLib.Idle.add(index_file_async.callback);
			});

			yield;

			if (ret == null)
				throw new GLib.IOError.FAILED ("failed to build entries");

			return ret;
		}

		public async string generate_key_async (Ide.SourceLocation location,
		                                        GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			var context = this.get_context ();
			var service = (Ide.ValaService)context.get_service_typed (typeof (Ide.ValaService));
			var index = service.index;
			var file = location.get_file ();
			var line = location.get_line () + 1;
			var column = location.get_line_offset () + 1;
			Vala.Symbol? symbol = yield index.find_symbol_at (file.get_file (), (int)line, (int)column);

			if (symbol == null)
				throw new GLib.IOError.FAILED ("failed to locate symbol");

			return symbol.get_full_name ();
		}
	}

	public class ValaCodeIndexEntries : GLib.Object, Ide.CodeIndexEntries
	{
		GLib.GenericArray<Ide.CodeIndexEntry> entries;
		GLib.File file;
		uint pos;

		public GLib.File get_file ()
		{
			return this.file;
		}

		public ValaCodeIndexEntries (GLib.File file, Ide.ValaSymbolTree tree)
		{
			this.entries = new GLib.GenericArray<Ide.CodeIndexEntry> ();
			this.file = file;
			this.add_children (tree, null, "");
		}

		public Ide.CodeIndexEntry? get_next_entry ()
		{
			if (this.pos < entries.length)
				return this.entries [this.pos++];
			return null;
		}

        public async GLib.GenericArray<weak Ide.CodeIndexEntry> next_entries_async (GLib.Cancellable? cancellable)
			throws GLib.Error
        {
			var ret = new GLib.GenericArray<Ide.CodeIndexEntry> ();

			for (;;)
			{
				Ide.CodeIndexEntry entry = get_next_entry ();

				if (entry == null)
					break;
				ret.add (entry);
			}

			return ret;
		}

		void add_children (Ide.ValaSymbolTree tree,
		                   Ide.SymbolNode? parent,
		                   string prefix)
		{
			var n_children = tree.get_n_children (parent);
			var builder = new Ide.CodeIndexEntryBuilder ();

			for (var i = 0; i < n_children; i++) {
				var child = tree.get_nth_child (parent, i) as Ide.ValaSymbolNode;
				string name = null;

				if (is_null_or_empty (prefix))
					name = child.name;
				else if (child.name != null && child.name[0] == '.')
					name = "%s%s".printf (prefix, child.name);
				else if (child.name != null)
					name = "%s.%s".printf (prefix, child.name);
				else
					continue;

				if (child.node is Vala.Symbol) {
					var node = child.node as Vala.Symbol;
					var loc = node.source_reference;
					var search_name = name;

					// NOTE: I don't like that we do the prefix stuff here,
					//       but we don't have a good place to do it yet.
					switch (child.kind) {
					case Ide.SymbolKind.FUNCTION:
					case Ide.SymbolKind.METHOD:
						search_name = "f\x1F%s".printf (name);
						break;

					case Ide.SymbolKind.VARIABLE:
					case Ide.SymbolKind.CONSTANT:
						search_name = "v\x1F%s".printf (name);
						break;

					case Ide.SymbolKind.CLASS:
						search_name = "c\x1F%s".printf (name);
						break;

					default:
						search_name = "x\x1F%s".printf (name);
						break;
					}

					builder.set_flags (child.flags | Ide.SymbolFlags.IS_DEFINITION);
					builder.set_name (search_name);
					builder.set_key (node.get_full_name ());
					builder.set_kind (child.kind);
					builder.set_range (loc.begin.line, loc.begin.column, loc.end.line, loc.end.column);

					var entry = builder.build ();

					this.entries.add (entry);
				}

				this.add_children (tree, child, name);
			}
		}
	}
}
