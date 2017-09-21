{{include "license.vala"}}

namespace {{PreFix}} {
	[GtkTemplate (ui = "{{appid_path}}/{{ui_file}}")]
	public class Window : Gtk.ApplicationWindow {
		[GtkChild]
		Gtk.Label label;

		public Window (Gtk.Application app) {
			Object (application: app);
		}
	}
}
