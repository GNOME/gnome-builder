/* ide-vala-diagnostics.vala
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

using Vala;

namespace Ide
{
	public class ValaDiagnostics: Vala.Report
	{
		private GLib.HashTable<string, Vala.ArrayList<Ide.Diagnostic>> diagnosed_files;

		public void clear ()
		{
			this.errors = 0;
			this.warnings = 0;
			if (diagnosed_files != null)
				diagnosed_files.remove_all ();

			diagnosed_files = new GLib.HashTable<string, Vala.ArrayList<Ide.Diagnostic>> (str_hash, str_equal);
		}

		private void add (Vala.SourceReference?  source_reference,
		                  string                 message,
		                  Ide.DiagnosticSeverity severity)
		{
			if (source_reference == null)
				return;

			unowned string filename = source_reference.file.filename;
			Ide.File file;
			unowned Vala.ArrayList<Ide.Diagnostic> diagnosed_file = diagnosed_files[filename];
			if (diagnosed_file != null)
				file = diagnosed_file[0].get_location ().get_file ();
			else {
				file = new Ide.File (null, GLib.File.new_for_path (filename));
				var list = new Vala.ArrayList<Ide.Diagnostic> ();
				diagnosed_files[filename] = list;
				diagnosed_file = list;
			}

			var begin = new Ide.SourceLocation (file,
			                                    source_reference.begin.line - 1,
			                                    source_reference.begin.column - 1,
			                                    0);
			var end = new Ide.SourceLocation (file,
			                                  source_reference.end.line - 1,
			                                  source_reference.end.column - 1,
			                                  0);

			var diag = new Ide.Diagnostic (severity, message, begin);
			diag.take_range (new Ide.SourceRange (begin, end));
			diagnosed_file.add (diag);
		}

		public Ide.Diagnostics get_diagnostic_from_file (string filename)
		{
			var ar = new GLib.GenericArray<Ide.Diagnostic> ();
			unowned Vala.ArrayList<Ide.Diagnostic> diagnosed_file = diagnosed_files[filename];
			if (diagnosed_file != null) {
				foreach (var diag in diagnosed_file) {
					ar.add (diag);
				}
			}

			return new Ide.Diagnostics (ar);
		}

		public override void note (Vala.SourceReference? source_reference, string message) {
			add (source_reference, message, Ide.DiagnosticSeverity.NOTE);
		}

		public override void depr (Vala.SourceReference? source_reference, string message) {
			add (source_reference, message, Ide.DiagnosticSeverity.DEPRECATED);
			++warnings;
		}

		public override void warn (Vala.SourceReference? source_reference, string message) {
			add (source_reference, message, Ide.DiagnosticSeverity.WARNING);
			++warnings;
		}

		public override void err (Vala.SourceReference? source_reference, string message) {
			add (source_reference, message, Ide.DiagnosticSeverity.ERROR);
			++errors;
		}
	}
}
