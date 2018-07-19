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
		Vala.HashMap<Vala.CodeNode?,Vala.ArrayList<Vala.CodeNode>> table;
		GLib.Queue<Vala.ArrayList<Vala.CodeNode>> queue;

		public ValaSymbolTreeVisitor ()
		{
			this.table = new Vala.HashMap<Vala.CodeNode?,Vala.ArrayList<Vala.CodeNode>> ();
			this.queue = new GLib.Queue<Vala.ArrayList<Vala.CodeNode>> ();

			var root = new Vala.ArrayList<Vala.CodeNode> ();
			this.table [null] = root;
			this.queue.push_head (root);
		}

		/*
		 * Creates the Code Index, a list of all the symbols
		 */
		public GLib.Variant build_index_entries ()
		{
			var variant_builder = new GLib.VariantBuilder (new GLib.VariantType ("a(usisuuuu)"));
			Vala.ArrayList<Vala.CodeNode>? list = table[null];
			if (list != null) {
				foreach (var element in list) {
					create_index (element, variant_builder);
				}
			}

			return variant_builder.end ();
		}

		private void create_index (Vala.CodeNode node, GLib.VariantBuilder variant_builder) {
			var symbol = vala_symbol_from_code_node (node);
			if (symbol == null)
				return;

			Ide.SymbolKind kind = vala_symbol_kind_from_code_node (node);
			Ide.SymbolFlags flags = vala_symbol_flags_from_code_node (node) | Ide.SymbolFlags.IS_DEFINITION;
			string name = vala_symbol_name (symbol);
			string search_name;
			switch (kind) {
				case Ide.SymbolKind.FUNCTION:
				case Ide.SymbolKind.METHOD:
					search_name = "f\x1F%s".printf (name);
					break;

				case Ide.SymbolKind.VARIABLE:
				case Ide.SymbolKind.FIELD:
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

			unowned Vala.SourceReference? loc = symbol.source_reference;
			variant_builder.add ("(usisuuuu)", flags,
			                                     symbol.get_full_name (),
			                                     kind,
			                                     search_name,
			                                     loc.begin.line,
			                                     loc.begin.column,
			                                     loc.end.line,
			                                     loc.end.column);

			Vala.ArrayList<Vala.CodeNode>? list = table[node];
			if (list != null) {
				foreach (var element in list) {
					create_index (element, variant_builder);
				}
			}
		}

		/*
		 * Creates the Symbols Tree
		 */
		public GLib.Variant build_tree ()
		{
			var variant_builder = new GLib.VariantBuilder (new GLib.VariantType ("aa{sv}"));
			Vala.ArrayList<Vala.CodeNode>? list = table[null];
			if (list != null) {
				foreach (var element in list) {
					var node = create_node (element);
					if (node != null)
						variant_builder.add_value (node);
				}
			}

			return variant_builder.end ();
		}

		private GLib.Variant? create_node (Vala.CodeNode node) {
			GLib.Variant? root_variant = null;
			var symbol = node as Vala.Symbol;
			if (symbol != null) {
				Ide.Symbol? ide_symbol = Ide.vala_to_ide_symbol (symbol);
				if (ide_symbol == null)
					return null;

				root_variant = ide_symbol.to_variant ();
			}

			var variantdict = new GLib.VariantDict (root_variant);
			var variant_builder = new GLib.VariantBuilder (new GLib.VariantType ("aa{sv}"));
			Vala.ArrayList<Vala.CodeNode>? list = table[node];
			if (list != null) {
				foreach (var element in list) {
					variant_builder.add_value (create_node (element));
				}
			}

			variantdict.insert_value ("children", variant_builder.end ());
			return variantdict.end ();
		}

		void visit_generic (Vala.CodeNode node)
		{
			var current = this.queue.peek_head ();
			current.add (node);

			var list = new ArrayList<Vala.CodeNode> ();
			this.queue.push_head (list);

			this.table [node] = list;

			// Automatic properties have a nested field that we don't want.
			if (!(node is Vala.Property)) {
				node.accept_children (this);
			}

			this.queue.pop_head ();
		}

		public override void visit_class (Vala.Class node) { this.visit_generic (node); }
		public override void visit_method (Vala.Method node) { this.visit_generic (node); }
		public override void visit_local_variable (Vala.LocalVariable node) { this.visit_generic (node); }
		public override void visit_interface (Vala.Interface node) { this.visit_generic (node); }
		public override void visit_struct (Vala.Struct node) { this.visit_generic (node); }
		public override void visit_creation_method (Vala.CreationMethod node) { this.visit_generic (node); }
		public override void visit_property (Vala.Property node) { this.visit_generic (node); }
		public override void visit_field (Vala.Field node) { this.visit_generic (node); }
		public override void visit_constant (Vala.Constant node) { this.visit_generic (node); }
		public override void visit_constructor (Vala.Constructor node) { this.visit_generic (node); }
		public override void visit_destructor (Vala.Destructor node) { this.visit_generic (node); }
		public override void visit_signal (Vala.Signal node) { this.visit_generic (node); }
		public override void visit_delegate (Vala.Delegate node) { this.visit_generic (node); }

		public override void visit_block (Vala.Block node) { node.accept_children (this); }
		public override void visit_source_file (Vala.SourceFile source_file) { source_file.accept_children (this); }
	}
}
