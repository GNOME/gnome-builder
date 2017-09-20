{{include "license.vala"}}

int main (string[] args) {
	var app = new Gtk.Application ("{{appid}}", ApplicationFlags.FLAGS_NONE);
	app.activate.connect (() => {
		var win = app.active_window;
		if (win == null) {
			win = new {{PreFix}}.Window (app);
		}
		win.present ();
	});

	return app.run (args);
}
