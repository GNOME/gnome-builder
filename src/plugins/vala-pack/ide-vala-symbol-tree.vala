/* ide-vala-symbol-tree.vala
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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
using Vala;

namespace Ide
{
	public class ValaSymbolTreeVisitor: Vala.CodeVisitor
	{
		HashMap<Vala.CodeNode?,ArrayList<Vala.CodeNode>> table;
		GLib.Queue<ArrayList<Vala.CodeNode>> queue;

		public ValaSymbolTreeVisitor ()
		{
			this.table = new HashMap<Vala.CodeNode?,ArrayList<Vala.CodeNode>> ();
			this.queue = new GLib.Queue<ArrayList<Vala.CodeNode>> ();

			var root = new ArrayList<Vala.CodeNode> ();
			this.table [null] = root;
			this.queue.push_head (root);
		}

		public Ide.SymbolTree? build_tree ()
		{
			return new Ide.ValaSymbolTree (this.table);
		}

		void visit_generic (Vala.CodeNode node)
		{
			var current = this.queue.peek_head ();
			current.add (node);

			var list = new ArrayList<Vala.CodeNode> ();
			this.queue.push_head (list);

			this.table [node] = list;

			node.accept_children (this);

			this.queue.pop_head ();
		}

		public override void visit_class (Vala.Class node) { this.visit_generic (node); }
		public override void visit_method (Vala.Method node) { this.visit_generic (node); }
		public override void visit_local_variable (Vala.LocalVariable node) { this.visit_generic (node); }
		public override void visit_interface (Vala.Interface node) { this.visit_generic (node); }
		public override void visit_struct (Vala.Struct node) { this.visit_generic (node); }
		public override void visit_creation_method (Vala.CreationMethod node) { this.visit_generic (node); }
		public override void visit_property (Vala.Property node) { this.visit_generic (node); }
		public override void visit_property_accessor (Vala.PropertyAccessor node) { this.visit_generic (node); }
		public override void visit_constructor (Vala.Constructor node) { this.visit_generic (node); }
		public override void visit_destructor (Vala.Destructor node) { this.visit_generic (node); }

		public override void visit_block (Vala.Block node) { node.accept_children (this); }
		public override void visit_source_file (Vala.SourceFile source_file) { source_file.accept_children (this); }
	}

	public class ValaSymbolTree : GLib.Object, Ide.SymbolTree
	{
		HashMap<Vala.CodeNode?,ArrayList<Vala.CodeNode>> table;

		public ValaSymbolTree (HashMap<Vala.CodeNode?,ArrayList<Vala.CodeNode>> table)
		{
			this.table = table;

			debug ("Tree created with %u rows", table.size);
		}

		ArrayList<Vala.CodeNode>? find (Ide.SymbolNode? node)
		{
			Ide.ValaSymbolNode? symbol_node = (Ide.ValaSymbolNode)node;
			Vala.CodeNode? key = null;

			if (symbol_node != null) {
				if  (!this.table.contains (symbol_node.node))
					return null;
				key = symbol_node.node;
			}

			return this.table [key];
		}

		public uint get_n_children (Ide.SymbolNode? node)
		{
			var list = find (node);

			if (list == null)
				debug ("Failed to find child! %p", node);
			else
				debug ("node has %u children.", list.size);

			return list != null ? list.size : 0;
		}

		public Ide.SymbolNode? get_nth_child (Ide.SymbolNode? node, uint nth)
		{
			var list = find (node);

			if (list != null && list.size > nth)
				return new Ide.ValaSymbolNode (list [(int)nth]);

			return null;
		}
	}

	public class ValaSymbolNode : Ide.SymbolNode
	{
		public Vala.CodeNode? node;

		public ValaSymbolNode (Vala.CodeNode node)
		{
			this.node = node;

			this.name = (node as Vala.Symbol).name;
			this.kind = Ide.SymbolKind.NONE;
			this.flags = Ide.SymbolFlags.NONE;

			if (node is Vala.Method)
				this.kind = Ide.SymbolKind.FUNCTION;
			else if (node is Vala.Class)
				this.kind = Ide.SymbolKind.CLASS;
			else if (node is Vala.Struct)
				this.kind = Ide.SymbolKind.STRUCT;
			else if (node is Vala.Property)
				this.kind = Ide.SymbolKind.FIELD;
		}

		public override async Ide.SourceLocation? get_location_async (GLib.Cancellable? cancellable)
		{
			var source_reference = this.node.source_reference;
			var file = (source_reference.file as Ide.ValaSourceFile).file;

			return new Ide.SourceLocation (file,
			                               source_reference.begin.line - 1,
			                               source_reference.begin.column - 1,
			                               0);
		}
	}
}
