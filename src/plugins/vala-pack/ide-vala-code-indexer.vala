/* ide-vala-indenter.vala
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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
	public class ValaCodeIndexer : Ide.Object, Ide.CodeIndexer
	{
		public Ide.CodeIndexEntries index_file (GLib.File file,
		                                        string[]? build_flags,
		                                        GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			throw new GLib.IOError.NOT_SUPPORTED ("indexing files is not yet supported");
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
}
