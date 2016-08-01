{{include "license.vala"}}

using Gtk;

int main (string[] args) {
    Gtk.init (ref args);

    var window = new Window ();
    window.title = "{{name}}";
    window.set_default_size (200, 200);
    window.destroy.connect (Gtk.main_quit);

    /* You can add GTK+ widgets to your window here.
     * See https://developer.gnome.org/ for help.
     */

    window.show_all ();

    Gtk.main ();
    return 0;
}
