{{include "license.vala"}}

using Gtk;

int main (string[] args) {
	var app = new Gtk.Application ("{{appid}}", ApplicationFlags.FLAGS_NONE);
	app.activate.connect (() => {
		if (app.active_window == null) {
			new {{PreFix}}.Window (app);
		}
		app.active_window.present ();
	});
	int ret = app.run (args);

	return ret;
}
