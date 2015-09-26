/* ide-vala-symbol-resolver.vala
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
	public class ValaSymbolResolver: Ide.Object, Ide.SymbolResolver
	{
		public async Ide.SymbolTree? get_symbol_tree_async (GLib.File file,
		                                                    GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			var context = this.get_context ();
			var service = (Ide.ValaService)context.get_service_typed (typeof (Ide.ValaService));
			var index = service.index;
			var symbol_tree = yield index.get_symbol_tree (file, cancellable);

			return symbol_tree;
		}

		public async Ide.Symbol? lookup_symbol_async (Ide.SourceLocation location,
		                                              GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			Vala.Symbol? symbol = null;

			var context = this.get_context ();
			var service = (Ide.ValaService)context.get_service_typed (typeof (Ide.ValaService));
			var index = service.index;
			var file = location.get_file ();
			var line = (int)location.get_line () + 1;
			var column = (int)location.get_line_offset () + 1;

			symbol = yield index.find_symbol_at (file.get_file (), line, column);

			if (symbol != null) {
				var kind = Ide.SymbolKind.FUNCTION;
				var flags = Ide.SymbolFlags.NONE;
				var source_reference = symbol.source_reference;

				if (source_reference != null) {
					var loc = new Ide.SourceLocation (file,
					                                  source_reference.begin.line - 1,
					                                  source_reference.begin.column - 1,
					                                  0);
					return new Ide.Symbol (symbol.name, kind, flags, loc, loc, loc);
				}
			}

			return null;
		}
	}
}

