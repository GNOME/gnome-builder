/* ide-vala-preferences-addin.vala
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

namespace Ide {
	public class ValaPreferencesAddin: GLib.Object, Ide.PreferencesAddin {
		uint enabled_switch;

		public void load (Dazzle.Preferences preferences) {
			this.enabled_switch = preferences.add_switch ("code-insight",
			                                              "diagnostics",
			                                              "org.gnome.builder.extension-type",
			                                              "enabled",
			                                              "/org/gnome/builder/extension-types/vala-pack-plugin/IdeDiagnosticProvider/",
			                                              null,
			                                              _("Vala"),
			                                              _("Show errors and warnings provided by Vala"),
			                                              /* translators: keywords used when searching for preferences */
			                                              _("vala diagnostics warnings errors"),
			                                              100);
		}

		public void unload (Dazzle.Preferences preferences) {
			/* TODO: preferences.remove (self.enabled_switch); */
		}
	}
}
