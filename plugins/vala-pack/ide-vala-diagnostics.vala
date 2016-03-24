/* ide-vala-diagnostics.vala
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
using Vala;

namespace Ide
{
	public class ValaDiagnostics: Vala.Report
	{
		public void clear ()
		{
			this.errors = 0;
			this.warnings = 0;
		}

		void add (Vala.SourceReference?  source_reference,
		          string                 message,
		          Ide.DiagnosticSeverity severity)
		{
			if (source_reference == null)
				return;

			if (source_reference.file is Ide.ValaSourceFile) {
				var file = (Ide.ValaSourceFile)source_reference.file;
				file.report (source_reference, message, severity);
			}
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
