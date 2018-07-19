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

namespace Ide
{
	public class ValaSymbolTree : Ide.Object, Ide.SymbolTree
	{
		public GLib.File file { get; construct; }
		public GLib.Variant tree { get; construct; }

		public ValaSymbolTree (Ide.Context context, GLib.File file, GLib.Variant tree)
		{
			GLib.Object (context: context, file: file, tree: tree);
		}

		public uint get_n_children (Ide.SymbolNode? node)
		{
			if (node != null)
				return (node as ValaSymbolNode).n_children;

			return (uint)tree.n_children ();
		}

		public Ide.SymbolNode? get_nth_child (Ide.SymbolNode? node, uint nth)
		{
			if (node != null)
				return (node as ValaSymbolNode).get_nth_child (nth);

			var child_val = tree.get_child_value (nth);
			return new ValaSymbolNode (context, child_val);
		}
	}

	public class ValaSymbolNode : Ide.SymbolNode
	{
		public GLib.Variant children { get; construct; }
		public Ide.Symbol symbol { get; construct; }

		public uint n_children
		{
			get {
				return (uint)children.n_children ();
			}
		}

		public ValaSymbolNode (Ide.Context context, GLib.Variant node)
		{
			var _symbol = new Ide.Symbol.from_variant (node);

			var tmp_children = node.lookup_value ("children", null);
			if (tmp_children.is_of_type (GLib.VariantType.VARIANT)) {
				tmp_children = tmp_children.get_variant ();
			} else if (!tmp_children.is_of_type (new GLib.VariantType ("aa{sv}")) &&
			           !tmp_children.is_of_type (new GLib.VariantType ("aa{sv}"))) {
				tmp_children = null;
			}

			GLib.Object (context: context,
			             children: tmp_children,
			             symbol: _symbol,
			             kind: _symbol.get_kind (),
			             flags: _symbol.get_flags (),
			             name: _symbol.get_name ());
		}

		construct {
			
		}

		public ValaSymbolNode get_nth_child (uint nth)
		{
			var child_val = children.get_child_value (nth);
			return new ValaSymbolNode (context, child_val);
		}
	}
}
