/* ide-vala-diagnostic-provider.vala
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
	public class ValaDiagnosticProvider: GLib.Object, Ide.DiagnosticProvider
	{
		public async Ide.Diagnostics? diagnose_async (Ide.File file,
		                                              Ide.Buffer buffer,
		                                              GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			var service = (Ide.ValaService)_context.get_service_typed (typeof (Ide.ValaService));
			yield service.index.parse_file (file.file, _context.unsaved_files, cancellable);
			var results = yield service.index.get_diagnostics (file.file, cancellable);
			return results;
		}

		public void load () {}

		// This code shouldn't have to exist.
		// If we can fixup libide+vala to not have such weird interaction that
		// would be great.
		Ide.Context? _context;
		public Ide.Context context { construct { _context = value; } }
		public void set_context (Ide.Context context) { _context = context; }
	}
}
