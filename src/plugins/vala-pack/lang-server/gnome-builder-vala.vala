/* ide-vala-indenter.vala
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using Ide;

namespace Ide
{
	public class ValaServer : GLib.Object {
		public bool is_in_flight {
			get {
				return GLib.AtomicInt.@get (ref in_flight) != 0;
			}
		}

		private int in_flight;
		private Ide.ValaIndex? index = null;

		construct {
			//
		}

		public void initialize (Jsonrpc.Client client, GLib.Variant id, GLib.Variant @params) {
			unowned string uri = null;
			if (Jsonrpc.Message.parse (params, "rootUri", Jsonrpc.Message.GetString.create (ref uri))) {
				index = new Ide.ValaIndex (GLib.File.new_for_uri (uri));
			}

			reply_to_client.begin (client, id);
		}

		public void get_index_key (Jsonrpc.Client client, GLib.Variant id, GLib.Variant @params) {
			unowned string uri = null;
			string[] flags = {};
			int64 line = 0;
			int64 column = 0;

			if (Jsonrpc.Message.parse (params,
				"path", Jsonrpc.Message.GetString.create (ref uri),
				"flags", Jsonrpc.Message.GetStrv.create (ref flags),
				"line", Jsonrpc.Message.GetInt64.create (ref line),
				"column", Jsonrpc.Message.GetInt64.create (ref column)))
			{
				//workdir = GLib.File.new_for_uri (uri);
				reply_to_client.begin (client, id);
			} else {
				reply_error_to_client.begin (client, id, Jsonrpc.ClientError.INVALID_PARAMS, "Invalid parameters for method call");
			}
		}

		public void diagnose (Jsonrpc.Client client, GLib.Variant id, GLib.Variant @params) {
			unowned string path = null;
			if (!Jsonrpc.Message.parse (params, "path", Jsonrpc.Message.GetString.create (ref path))) {
				reply_error_to_client.begin (client, id, Jsonrpc.ClientError.INVALID_PARAMS, "Invalid parameters for method call");
				return;
			}

			string[] flags = {};
			Jsonrpc.Message.parse (params, "flags", Jsonrpc.Message.GetStrv.create (ref flags));

			var diagnostics = index.get_file_diagnostics (path, flags);
			var builder = new VariantBuilder (new VariantType ("aa{sv}") );
			var size = diagnostics.get_size ();
			for (int i = 0; i < size; i++) {
				builder.add_value (diagnostics.index (i).to_variant ());
			}

			reply_to_client.begin (client, id, builder.end ());
		}

		public void set_buffer (Jsonrpc.Client client, GLib.Variant id, GLib.Variant @params) {
			unowned string path = null;
			if (!Jsonrpc.Message.parse (params, "path", Jsonrpc.Message.GetString.create (ref path))) {
				reply_error_to_client.begin (client, id, Jsonrpc.ClientError.INVALID_PARAMS, "Invalid parameters for method call");
				return;
			}

			unowned string content = null;
			params.lookup ("contents", "^&ay", ref content);
			GLib.Bytes? bytes = null;
			if (content != null)
				bytes = new GLib.Bytes.take (content.data);

			index.set_unsaved_file (path, bytes);
			reply_to_client.begin (client, id, new GLib.Variant.boolean (true));
		}

		private async void reply_to_client (Jsonrpc.Client client, GLib.Variant id, GLib.Variant? reply = null) {
			GLib.AtomicInt.inc (ref in_flight);
			try {
				yield client.reply_async (id, reply, null);
			} catch (Error e) {
				warning ("Reply failed: %s", e.message);
			}

			if (GLib.AtomicInt.dec_and_test (ref in_flight))
				notify_property ("is-in-flight");
		}

		private async void reply_error_to_client (Jsonrpc.Client client, GLib.Variant id, int code, string? message) {
			GLib.AtomicInt.inc (ref in_flight);
			try {
				yield client.reply_error_async (id, code, message, null);
			} catch (Error e) {
				warning ("Reply failed: %s", e.message);
			}

			if (GLib.AtomicInt.dec_and_test (ref in_flight))
				notify_property ("is-in-flight");
		}
	}

	int main (string[] args) {
		var input = new GLib.UnixInputStream (Posix.STDIN_FILENO, false);
		var output = new GLib.UnixOutputStream (Posix.STDOUT_FILENO, false);
		var stream = new GLib.SimpleIOStream (input, output);

		/* Only write to stderr so that we don't interrupt IPC */
		GLib.Log.set_handler (null, GLib.LogLevelFlags.LEVEL_MASK, (log_domain, log_levels, message) => {
			GLib.printerr ("%s: %s\n", log_domain, message);
		});

		var vala = new Ide.ValaServer ();
		try {
			GLib.Unix.set_fd_nonblocking (Posix.STDIN_FILENO, true);
			GLib.Unix.set_fd_nonblocking (Posix.STDOUT_FILENO, true);
		} catch (Error e) {
			GLib.printerr ("Failed to set FD non-blocking: %s\n", e.message);
			return Posix.EXIT_FAILURE;
		}

		var main_loop = new GLib.MainLoop (null, false);
		var server = new Jsonrpc.Server ();
		bool closing = false;
		server.client_closed.connect (() => {
			closing = true;
			if (!vala.is_in_flight)
				main_loop.quit ();
		});

		vala.notify["is-in-flight"].connect (() => {
			if (closing && !vala.is_in_flight) {
				main_loop.quit ();
			}
		});

		server.add_handler ("initialize", (self, client, method, id, @params) => vala.initialize (client, id, params));
		server.add_handler ("vala/getIndexKey", (self, client, method, id, @params) => vala.get_index_key (client, id, params));
		server.add_handler ("vala/diagnose", (self, client, method, id, @params) => vala.diagnose (client, id, params));
		server.add_handler ("vala/setBuffer", (self, client, method, id, @params) => vala.set_buffer (client, id, params));
		  // ADD_HANDLER ("clang/complete", handle_complete);
		  // ADD_HANDLER ("clang/findNearestScope", handle_find_nearest_scope);
		  // ADD_HANDLER ("clang/getIndexKey", handle_get_index_key);
		  // ADD_HANDLER ("clang/getSymbolTree", handle_get_symbol_tree);
		  // ADD_HANDLER ("clang/indexFile", handle_index_file);
		  // ADD_HANDLER ("clang/locateSymbol", handle_locate_symbol);
		  // ADD_HANDLER ("clang/getHighlightIndex", handle_get_highlight_index);
		  // ADD_HANDLER ("$/cancelRequest", handle_cancel_request);

		server.accept_io_stream (stream);

		main_loop.run ();
		return Posix.EXIT_SUCCESS;
	}
}
