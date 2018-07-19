/* ide-vala-source-file.vala
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

/*
 * Bits of the following file were inspired from Anjuta. It's original
 * copyright is in tact below.
 *
 * Copyright 2008-2010 Abderrahim Kitouni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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
using Vala;

namespace Ide
{
	public class ValaSourceFile: Vala.SourceFile
	{
		public ValaSourceFile (Vala.CodeContext context,
		                       Vala.SourceFileType type,
		                       string filename,
		                       string? content,
		                       bool cmdline)
		{
			base (context, type, filename, content, cmdline);

			add_default_namespace ();
			dirty = true;
		}

		public bool dirty { get; set; }

		public void reset ()
		{
			/* Copy the node list since we will be mutating while iterating */
			var copy = new ArrayList<Vala.CodeNode> ();
			foreach (var node in this.get_nodes ()) {
				copy.add (node);
			}

			var entry_point = context.entry_point;

			foreach (var node in copy) {
				remove_node (node);

				if (node is Vala.Symbol) {
					var symbol = (Vala.Symbol)node;
					if (symbol.owner != null) {
						symbol.owner.remove (symbol.name);
					}
					if (symbol == entry_point) {
						context.entry_point = null;
					}
				}
			}

			add_default_namespace ();
			dirty = true;
		}

		public void sync (GLib.Bytes? bytes)
		{
			if (bytes == null) {
				content = null;
				reset ();
			}

			unowned uint8[] data = bytes.get_data ();
			if (data != (uint8[]) content) {
				content = (string)data;
				reset ();
			}
		}

		void add_default_namespace ()
		{
			this.current_using_directives = new ArrayList<Vala.UsingDirective> ();

			var unres = new Vala.UnresolvedSymbol (null, "GLib");
			var udir = new Vala.UsingDirective (unres);

			this.add_using_directive (udir);
			this.context.root.add_using_directive (udir);
		}
	}
}

