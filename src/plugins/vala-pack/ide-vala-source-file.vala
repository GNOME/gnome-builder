/* ide-vala-source-file.vala
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

/*
 * Bits of the following file were inspired from Anjuta. It's original
 * copyright is in tact below.
 *
 * Copyright © 2008-2010 Abderrahim Kitouni
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
using Ide;
using Vala;

namespace Ide
{
	public class ValaSourceFile: Vala.SourceFile
	{
		ArrayList<Ide.Diagnostic> diagnostics;
		internal Ide.File file;

		public ValaSourceFile (Vala.CodeContext context,
		                       Vala.SourceFileType type,
		                       string filename,
		                       string? content,
		                       bool cmdline)
		{
			base (context, type, filename, content, cmdline);

			this.file = new Ide.File (null, GLib.File.new_for_path (filename));
			this.diagnostics = new ArrayList<Ide.Diagnostic> ();

			this.add_default_namespace ();
			this.dirty = true;
		}

		public bool dirty { get; set; }

		public GLib.File get_file ()
		{
			return this.file.file;
		}

		public void reset ()
		{
			this.diagnostics.clear ();

			/* Copy the node list since we will be mutating while iterating */
			var copy = new ArrayList<Vala.CodeNode> ();
			foreach (var node in this.get_nodes ()) {
				copy.add (node);
			}

			var entry_point = this.context.entry_point;

			foreach (var node in copy) {
				this.remove_node (node);

				if (node is Vala.Symbol) {
					var symbol = (Vala.Symbol)node;
					if (symbol.owner != null) {
						symbol.owner.remove (symbol.name);
					}
					if (symbol == entry_point) {
						this.context.entry_point = null;
					}
				}
			}

			this.add_default_namespace ();
			this.dirty = true;
		}

		public void sync (GenericArray<unowned Ide.UnsavedFile> unsaved_files)
		{
			var gfile = this.file.file;
			unsaved_files.foreach((unsaved_file) => {
				if (unsaved_file.get_file ().equal (gfile)) {
					var bytes = unsaved_file.get_content ();

					if (bytes.get_data () != (uint8[]) this.content) {
						this.content = (string)bytes.get_data ();
						this.reset ();
						return;
					}
				}
			});
		}

		public void report (Vala.SourceReference source_reference,
		                    string message,
		                    Ide.DiagnosticSeverity severity)
		{
			var begin = new Ide.SourceLocation (this.file,
			                                    source_reference.begin.line - 1,
			                                    source_reference.begin.column - 1,
			                                    0);
			var end = new Ide.SourceLocation (this.file,
			                                  source_reference.end.line - 1,
			                                  source_reference.end.column - 1,
			                                  0);

			var diag = new Ide.Diagnostic (severity, message, begin);
			diag.take_range (new Ide.SourceRange (begin, end));
			this.diagnostics.add (diag);
		}

		public Ide.Diagnostics? diagnose ()
		{
			var ar = new GLib.GenericArray<Ide.Diagnostic> ();
			foreach (var diag in this.diagnostics) {
				ar.add (diag);
			}
			return new Ide.Diagnostics (ar);
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

