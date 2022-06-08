{{include "license.vala"}}

namespace {{PreFix}} {
    [GtkTemplate (ui = "{{appid_path}}/{{ui_file}}")]
    public class Window : {{if is_adwaita}}Adw{{else}}Gtk{{end}}.ApplicationWindow {
        [GtkChild]
        private unowned Gtk.Label label;

        public Window (Gtk.Application app) {
            Object (application: app);
        }
    }
}
