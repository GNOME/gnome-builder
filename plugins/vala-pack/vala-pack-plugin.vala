/* vala-pack-plugin.vala
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
using Peas;

[ModuleInit]
public void peas_register_types (GLib.TypeModule module)
{
	Peas.ObjectModule peas = (Peas.ObjectModule)module;

	peas.register_extension_type (typeof (Ide.BuildPipelineAddin), typeof (Ide.ValaPipelineAddin));
	peas.register_extension_type (typeof (Ide.CompletionProvider), typeof (Ide.ValaCompletionProvider));
	peas.register_extension_type (typeof (Ide.DiagnosticProvider), typeof (Ide.ValaDiagnosticProvider));
	peas.register_extension_type (typeof (Ide.Indenter), typeof (Ide.ValaIndenter));
	peas.register_extension_type (typeof (Ide.PreferencesAddin), typeof (Ide.ValaPreferencesAddin));
	peas.register_extension_type (typeof (Ide.Service), typeof (Ide.ValaService));
	peas.register_extension_type (typeof (Ide.SymbolResolver), typeof (Ide.ValaSymbolResolver));
}
