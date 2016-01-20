[DBus (name="org.mpris.MediaPlayer2.Player")]
public interface Mpris : Object {
	// public abstract string playback_status { get; }

	public abstract void previous () throws IOError;
	public abstract void play_pause () throws IOError;
	public abstract void next () throws IOError;
}

public class Ide.MediaControls : GLib.Object, Ide.WorkbenchAddin {
	public bool can_open (Ide.Uri uri, string? content_type, out int priority) {
		return false;
	}

	public string get_id () {
		return typeof (Ide.MediaControls).name ();
	}

	public void load (Ide.Workbench workbench) {
		var _titlebar = workbench.get_perspective_by_name ("editor").get_titlebar ();
		if (_titlebar is Gtk.HeaderBar) {
			var titlebar = (Gtk.HeaderBar) _titlebar;
			var box = new Gtk.Box (Gtk.Orientation.HORIZONTAL, 0);
			box.get_style_context ().add_class ("linked");

			var back = new Gtk.Button.from_icon_name ("media-seek-backward-symbolic");
			var play = new Gtk.Button.from_icon_name ("media-playback-start-symbolic");
			var forward = new Gtk.Button.from_icon_name ("media-seek-forward-symbolic");

			Mpris? mpris = null;
			Bus.get_proxy.begin<Mpris> (BusType.SESSION,
										"org.mpris.MediaPlayer2.spotify",
										"/org/mpris/MediaPlayer2",
										0, null,
										(obj, res) => {
				mpris = Bus.get_proxy.end (res);
				back.clicked.connect (button => mpris.previous ());
				play.clicked.connect (button => mpris.play_pause ());
				forward.clicked.connect (button => mpris.next ());
			});

			box.add (back);
			box.add (play);
			box.add (forward);

			box.show_all ();
			titlebar.pack_end (box);
		}
	}

	public async bool open_async (Ide.Uri uri, string content_type, Cancellable? cancel) throws Error {
		assert_not_reached ();
		return false;
	}

	public void unload (Ide.Workbench workbench) {
		// TODO
	}
}

[ModuleInit]
public void peas_register_types (TypeModule module)
{
	Peas.ObjectModule peas = (Peas.ObjectModule)module;

	peas.register_extension_type (typeof (Ide.WorkbenchAddin), typeof (Ide.MediaControls));
}
