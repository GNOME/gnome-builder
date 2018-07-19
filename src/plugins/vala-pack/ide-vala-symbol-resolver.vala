/* ide-vala-symbol-resolver.vala
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
	public class ValaSymbolResolver: Ide.Object, Ide.SymbolResolver
	{
		public async Ide.SymbolTree? get_symbol_tree_async (GLib.File file,
		                                                    GLib.Bytes? contents,
		                                                    GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			unowned Ide.Context context = this.get_context ();
			unowned Ide.BuildSystem? build_system = Ide.BuildSystem.from_context (context);

			string[] flags = {};
			try {
				flags = yield build_system.get_build_flags_async (file, cancellable);
			} catch (GLib.Error err) {
				warning (err.message);
			}

			unowned Ide.ValaClient client = Ide.ValaClient.from_context (context);
			return yield client.get_symbol_tree_async (file, flags, cancellable);
		}

		public async Ide.Symbol? lookup_symbol_async (Ide.Location location,
		                                              GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			unowned Ide.Context context = this.get_context ();
			unowned Ide.BuildSystem? build_system = Ide.BuildSystem.from_context (context);
			var line = (int)location.line + 1;
			var column = (int)location.line_offset + 1;

			string[] flags = {};
			try {
				flags = yield build_system.get_build_flags_async (location.file, cancellable);
			} catch (GLib.Error err) {
				warning (err.message);
			}

			unowned Ide.ValaClient client = Ide.ValaClient.from_context (context);
			return yield client.locate_symbol_async (location.file, flags, line, column, cancellable);
		}

		public void load () {}
		public void unload () {}

		public async GLib.GenericArray<Ide.Range> find_references_async (Ide.Location location,
		                                                                 string? language_id,
		                                                                 GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			return new GLib.GenericArray<Ide.Range> ();
		}

		public async Ide.Symbol? find_nearest_scope_async (Ide.Location location,
		                                                   GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			unowned Ide.Context context = this.get_context ();
			unowned Ide.BuildSystem? build_system = Ide.BuildSystem.from_context (context);
			var line = (int)location.line + 1;
			var column = (int)location.line_offset + 1;

			string[] flags = {};
			try {
				flags = yield build_system.get_build_flags_async (location.file, cancellable);
			} catch (GLib.Error err) {
				warning (err.message);
			}

			unowned Ide.ValaClient client = Ide.ValaClient.from_context (context);
			var symbol = yield client.locate_symbol_async (location.file, flags, line, column, cancellable);
			if (symbol != null)
				return symbol;

			throw new GLib.IOError.NOT_FOUND ("Failed to locate nearest scope");
		}
	}
}

