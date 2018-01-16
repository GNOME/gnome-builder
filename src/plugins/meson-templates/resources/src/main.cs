using Gtk;

namespace Example
{
	public class App
	{
		static void Main (string[] args)
		{
			Application.Init ();

			var window = new Gtk.Window ("Example");

			var label = new Gtk.Label ("Hello, World!");
			window.Add (label);
			label.Show ();

			window.DeleteEvent += (win, ev) => {
				Application.Quit ();
			};

			window.Present ();
			Application.Run ();
		}
	}
}