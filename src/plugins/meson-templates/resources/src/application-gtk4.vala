{{include "license.vala"}}

namespace {{PreFix}} {
    public class Application : {{if is_adwaita}}Adw{{else}}Gtk{{end}}.Application {
        private ActionEntry[] APP_ACTIONS = {
            { "about", this.on_about_action },
            { "preferences", this.on_preferences_action },
            { "quit", this.quit }
        };


        public Application () {
            Object (application_id: "{{appid}}", flags: ApplicationFlags.FLAGS_NONE);

            this.add_action_entries (this.APP_ACTIONS, this);
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
