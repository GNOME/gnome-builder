{{include "license.vala"}}

using Gtk;

namespace {{PreFix}} {
	[GtkTemplate (ui = "{{appid_path}}/{{ui_file}}")]
	public class Window : Gtk.ApplicationWindow {
		[GtkChild]
		Label label;

		public Window (Gtk.Application app) {
			Object(application: app);
		}
	}
}
