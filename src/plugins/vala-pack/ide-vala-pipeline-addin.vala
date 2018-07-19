/* ide-vala-pipeline-addin.vala
 *
 * Copyright 2017 Christian Hergert <christian@hergert.me>
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
using Gtk;
using Ide;

namespace Ide
{
	public class ValaPipelineAddin: Ide.Object, Ide.BuildPipelineAddin
	{
		// main.vala:24.30-24.30: error: initializer list used for `Gtk.WindowType', which is neither array nor struct
		const string ERROR_FORMAT_REGEX =
			"(?<filename>[a-zA-Z0-9\\-\\.\\/_]+.vala):" +
			"(?<line>\\d+).(?<column>\\d+)-(?<line2>\\d+).(?<column2>\\d+): " +
			"(?<level>[\\w\\s]+): " +
			"(?<message>.*)";

		uint error_format = 0;

		public void load (Ide.BuildPipeline pipeline)
		{
			this.error_format = pipeline.add_error_format (ERROR_FORMAT_REGEX,
			                                               GLib.RegexCompileFlags.OPTIMIZE | GLib.RegexCompileFlags.CASELESS);
		}

		public void unload (Ide.BuildPipeline pipeline)
		{
			pipeline.remove_error_format (this.error_format);
		}
	}
}
