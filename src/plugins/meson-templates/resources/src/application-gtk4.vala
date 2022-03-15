{{include "license.vala"}}

namespace {{PreFix}} {
    public class Application : {{if is_adwaita}}Adw{{else}}Gtk{{end}}.Application {
        public Application () {
            Object (application_id: "{{appid}}", flags: ApplicationFlags.FLAGS_NONE);
        }

        construct {
            ActionEntry[] action_entries = {
                { "about", this.on_about_action },
                { "preferences", this.on_preferences_action },
                { "quit", this.quit }
            };
            this.add_action_entries (action_entries, this);
            this.set_accels_for_action ("app.quit", {"<primary>q"});
        }

        public override void activate () {
            base.activate ();
            var win = this.active_window;
            if (win == null) {
                win = new {{PreFix}}.Window (this);
            }
            win.present ();
        }

        private void on_about_action () {
            string[] authors = { "{{author}}" };
            Gtk.show_about_dialog (this.active_window,
                                   "program-name", "{{name}}",
                                   "authors", authors,
                                   "version", "{{project_version}}");
        }

        private void on_preferences_action () {
            message ("app.preferences action activated");
        }
    }
}
