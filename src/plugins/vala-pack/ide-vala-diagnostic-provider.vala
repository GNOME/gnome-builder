/* ide-vala-diagnostic-provider.vala
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
	public class ValaDiagnosticProvider: Ide.Object, Ide.DiagnosticProvider
	{
		public async Ide.Diagnostics diagnose_async (GLib.File file,
		                                             GLib.Bytes? contents,
		                                             string? language_id,
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
			return yield client.diagnose_async (file, flags, cancellable);
		}

		public void load () {}
		public void unload () {}
	}
}
