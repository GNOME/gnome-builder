/* ide-vala-service.vala
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
	public class ValaService: Ide.Object, Ide.Service
	{
		Ide.ValaIndex _index;

		public unowned string get_name () {
			return typeof (Ide.ValaService).name ();
		}

		public ValaIndex index {
			get {
				if (this._index == null) {
					this._index = new Ide.ValaIndex (this.get_context ());

					Ide.ThreadPool.push (Ide.ThreadPoolKind.INDEXER, () => {
						Ide.Vcs vcs = this.get_context ().get_vcs ();
						var files = new ArrayList<GLib.File> ();

						load_directory (vcs.get_working_directory (), null, files);

						if (files.size > 0) {
							this._index.add_files.begin (files, null, () => {
								debug ("Vala files registered");
							});
						}
					});
				}

				return this._index;
			}
		}

		public void start () {
		}

		public void stop () {
		}

		public void load_directory (GLib.File directory,
		                            GLib.Cancellable? cancellable,
		                            ArrayList<GLib.File> files)
		{
			try {
				var enumerator = directory.enumerate_children (FileAttribute.STANDARD_NAME+","+FileAttribute.STANDARD_TYPE, 0, cancellable);
				var directories = new ArrayList<GLib.File> ();

				FileInfo file_info;
				while ((file_info = enumerator.next_file ()) != null) {
					var name = file_info.get_name ();

					if (name == ".flatpak-builder" || name == ".git")
						continue;

					if (file_info.get_file_type () == GLib.FileType.DIRECTORY) {
						directories.add (directory.get_child (file_info.get_name ()));
					} else if (name.has_suffix (".vala") || name.has_suffix (".vapi")) {
						files.add (directory.get_child (file_info.get_name ()));
					}
				}

				enumerator.close ();

				foreach (var child in directories) {
					load_directory (child, cancellable, files);
				}
			} catch (GLib.Error err) {
				warning ("%s", err.message);
			}
		}
	}
}
