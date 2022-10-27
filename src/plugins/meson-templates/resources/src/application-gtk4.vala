{{include "license.vala"}}

namespace {{PreFix}} {
    public class Application : {{if is_adwaita}}Adw{{else}}Gtk{{end}}.Application {
        public Application () {
            Object (application_id: "{{appid}}", flags: ApplicationFlags.DEFAULT_FLAGS);
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
{{if is_adwaita}}
            string[] developers = { "{{author}}" };
            var about = new Adw.AboutWindow () {
                transient_for = this.active_window,
                application_name = "{{name}}",
                application_icon = "{{appid}}",
                developer_name = "{{author}}",
                version = "{{project_version}}",
                developers = developers,
                copyright = "© {{year}} {{author}}",
            };

            about.present ();
{{else}}
            string[] authors = { "{{author}}" };
            Gtk.show_about_dialog (this.active_window,
                                   "program-name", "{{name}}",
                                   "logo-icon-name", "{{appid}}",
                                   "authors", authors,
                                   "version", "{{project_version}}",
                                   "copyright", "© {{year}} {{author}}");
{{end}}
        }

        private void on_preferences_action () {
            message ("app.preferences action activated");
        }
    }
}
