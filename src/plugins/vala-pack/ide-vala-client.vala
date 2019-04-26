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

using GLib;
using Ide;


namespace Ide
{
	public class ValaClient : Ide.Object {

		public static unowned Ide.ValaClient from_context (Ide.Context context) {
			context.lock ();
			var client = (Ide.ValaClient)context.ensure_child_typed (typeof (ValaClient));

			if (client == null) {
				client = new Ide.ValaClient ();
				context.append (client);
			}
			context.unlock ();

			unowned Ide.ValaClient self = client;
			return self;
		}

		public unowned string get_name () {
			return typeof (Ide.ValaClient).name ();
		}

		private enum State {
			INITIAL,
			SPAWNING,
			RUNNING,
			SHUTDOWN
		}

		GLib.Queue<GetContextCB> get_client;
		Ide.SubprocessSupervisor supervisor;
		Jsonrpc.Client rpc_client;
		GLib.File root_uri;
		//GLib.HashTable<GLib.File, int64?> seq_by_file = null;
		State state = State.INITIAL;

		~ValaClient () {
			state = State.SHUTDOWN;
			if (supervisor != null) {
				var _supervisor = supervisor;
				supervisor = null;
				_supervisor.stop ();
			}
		}

		[Compact]
		public class GetContextCB {
			public SourceFunc source_func;
		}

		public override void parent_set (Ide.Object? parent) {
			if (parent == null)
				return;

			get_client = new GLib.Queue<GetContextCB> ();
			unowned Ide.Context? context = get_context ();
			root_uri = context.ref_workdir ();

			var launcher = new Ide.SubprocessLauncher (GLib.SubprocessFlags.STDOUT_PIPE | GLib.SubprocessFlags.STDIN_PIPE);
			if (root_uri.is_native ())
				launcher.cwd = root_uri.get_path ();

			launcher.clean_env = false;
			launcher.push_argv (ValaConfig.PACKAGE_LIBEXECDIR + "/gnome-builder-vala");
			supervisor = new Ide.SubprocessSupervisor ();
			supervisor.set_launcher (launcher);
			supervisor.spawned.connect (subprocess_spawned);
			supervisor.exited.connect (subprocess_exited);

			unowned Ide.BufferManager buffer_manager = Ide.BufferManager.from_context (context);
			buffer_manager.buffer_saved.connect (buffer_saved);
		}

		public void subprocess_exited (Ide.Subprocess object) {
			if (state == State.RUNNING)
				state = State.SPAWNING;

			rpc_client = null;
		}

		public void subprocess_spawned (Ide.Subprocess subprocess) {
			if (state == State.SPAWNING)
				state = State.RUNNING;

			unowned GLib.UnixInputStream input = subprocess.get_stdout_pipe () as GLib.UnixInputStream;
			unowned GLib.UnixOutputStream output = subprocess.get_stdin_pipe () as GLib.UnixOutputStream;
			var stream = new GLib.SimpleIOStream (input, output);
			try {
				GLib.Unix.set_fd_nonblocking (input.get_fd (), true);
			} catch (Error e) {}

			try {
				GLib.Unix.set_fd_nonblocking (output.get_fd (), true);
			} catch (Error e) {}

			rpc_client = new Jsonrpc.Client (stream);
			rpc_client.set_use_gvariant (true);

			var @params = Jsonrpc.Message.@new (
				"rootUri", Jsonrpc.Message.PutString.create (root_uri.get_uri ()),
				"rootPath", Jsonrpc.Message.PutString.create (root_uri.get_path ()),
				"processId", Jsonrpc.Message.PutInt64.create (Posix.getpid ()),
				"capabilities", "{", "}"
			);

			rpc_client.call_async.begin ("initialize", params, null);

			GetContextCB cb = null;
			lock (get_client) {
				while ((cb = get_client.pop_head ()) != null) {
					cb.source_func ();
				}
			}
		}

		public void buffer_saved (Ide.Buffer buffer) {
			/*
			 * We need to clear the cached buffer on the peer (and potentially
			 * pop the translation unit cache) now that the buffer has been
			 * saved to disk and we no longer need the draft.
			 */

			var gfile = buffer.file;
			/*lock (seq_by_file) {
				if (seq_by_file != null) {
					seq_by_file.remove (gfile);
				}
			}*/

			/* skip if thereis no peer */
			if (rpc_client == null)
				return;

			if (gfile != null)
				set_buffer_async.begin (gfile);
		}

		public async GLib.Variant? call_async (string method,
		                                       GLib.Variant @params,
		                                       GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			try {
				var client = yield get_client_async (cancellable);
				GLib.Variant reply;
				yield client.call_async (method, params, cancellable, out reply);
				return reply;
			} catch (Error e) {
				throw e;
			}
		}

		public async GLib.Variant index_file_async (GLib.File file,
		                                            string[]? flags = null,
		                                            GLib.Cancellable? cancellable = null)
			throws GLib.Error
		{
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("Only native files can be indexed");

			sync_buffers ();
			var params = Jsonrpc.Message.@new (
				"path", Jsonrpc.Message.PutString.create (file.get_path ()),
				"flags", Jsonrpc.Message.PutStrv.create (flags)
			);

			try {
				return yield call_async ("vala/indexFile",
				                         params,
				                         cancellable);
			} catch (Error e) {
				throw e;
			}
		}

		public async string get_index_key_async (GLib.File file,
		                                         string[]? flags = null,
		                                         uint line,
		                                         uint column,
		                                         GLib.Cancellable? cancellable = null)
			throws GLib.Error
		{
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("Only native files are supported");

			sync_buffers ();
			var params = Jsonrpc.Message.@new (
				"path", Jsonrpc.Message.PutString.create (file.get_path ()),
				"flags", Jsonrpc.Message.PutStrv.create (flags),
				"line", Jsonrpc.Message.PutInt64.create (line),
				"column", Jsonrpc.Message.PutInt64.create (column)
			);

			try {
				var reply = yield call_async ("vala/getIndexKey",
				                              params,
				                              cancellable);
				if (!reply.is_of_type (GLib.VariantType.STRING)) {
					throw new GLib.IOError.INVALID_DATA ("Got a result back that was not a string");
				}

				return reply.dup_string ();
			} catch (Error e) {
				throw e;
			}
		}

		public async Ide.SymbolTree get_symbol_tree_async (GLib.File file,
		                                                   string[]? flags = null,
		                                                   GLib.Cancellable? cancellable = null)
			throws GLib.Error
		{
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("Only native files are supported");

			sync_buffers ();
			var params = Jsonrpc.Message.@new (
				"path", Jsonrpc.Message.PutString.create (file.get_path ()),
				"flags", Jsonrpc.Message.PutStrv.create (flags)
			);

			try {
				var reply = yield call_async ("vala/getSymbolTree",
				                              params,
				                              cancellable);
				return new ValaSymbolTree (file, reply);
			} catch (Error e) {
				throw e;
			}
		}

		public async Ide.Symbol? locate_symbol_async (GLib.File file,
		                                              string[]? flags = null,
		                                              uint line,
		                                              uint column,
		                                              GLib.Cancellable? cancellable = null)
			throws GLib.Error
		{
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("Only native files are supported");

			sync_buffers ();
			var params = Jsonrpc.Message.@new (
				"path", Jsonrpc.Message.PutString.create (file.get_path ()),
				"flags", Jsonrpc.Message.PutStrv.create (flags),
				"line", Jsonrpc.Message.PutInt64.create (line),
				"column", Jsonrpc.Message.PutInt64.create (column)
			);

			try {
				var reply = yield call_async ("vala/locateSymbol",
				                              params,
				                              cancellable);
				return new Ide.Symbol.from_variant (reply);
			} catch (Error e) {
				throw e;
			}
		}

		public async Ide.Symbol? find_nearest_scope_async (GLib.File file,
		                                                   string[]? flags = null,
		                                                   uint line,
		                                                   uint column,
		                                                   GLib.Cancellable? cancellable = null)
			throws GLib.Error
		{
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("Only native files are supported");

			sync_buffers ();
			var params = Jsonrpc.Message.@new (
				"path", Jsonrpc.Message.PutString.create (file.get_path ()),
				"flags", Jsonrpc.Message.PutStrv.create (flags),
				"line", Jsonrpc.Message.PutInt64.create (line),
				"column", Jsonrpc.Message.PutInt64.create (column)
			);

			try {
				var reply = yield call_async ("vala/findNearestScope",
				                              params,
				                              cancellable);
				return new Ide.Symbol.from_variant (reply);
			} catch (Error e) {
				throw e;
			}
		}

		public async Ide.Symbol? proposals_populate_async (GLib.File file,
		                                                   uint line,
		                                                   uint column,
		                                                   string? line_text,
		                                                   GLib.Cancellable? cancellable = null)
			throws GLib.Error
		{
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("Only native files are supported");

			sync_buffers ();
			var params = Jsonrpc.Message.@new (
				"path", Jsonrpc.Message.PutString.create (file.get_path ()),
				"line", Jsonrpc.Message.PutInt64.create (line),
				"column", Jsonrpc.Message.PutInt64.create (column),
				"line_text", Jsonrpc.Message.PutString.create (line_text)
			);

			try {
				var reply = yield call_async ("vala/complete",
				                              params,
				                              cancellable);
				if (reply != null) {
					return new Ide.Symbol.from_variant (reply);
				}
				return null;
			} catch (Error e) {
				throw e;
			}
		}

		private async Jsonrpc.Client get_client_async (GLib.Cancellable? cancellable) throws GLib.Error {
			switch (state) {
				default:
					state = State.SPAWNING;
					supervisor.start ();
					var cb = new GetContextCB ();
					cb.source_func = get_client_async.callback;
					lock (get_client) {
						get_client.push_tail ((owned) cb);
					}

					yield;
					return rpc_client;
				case State.SPAWNING:
					var cb = new GetContextCB ();
					cb.source_func = get_client_async.callback;
					lock (get_client) {
						get_client.push_tail ((owned) cb);
					}

					yield;
					return rpc_client;
				case State.RUNNING:
					return rpc_client;
				case State.SHUTDOWN:
					throw new GLib.IOError.CLOSED ("The client has been closed");
			}
		}

		private void sync_buffers () {
			unowned Ide.Context context = get_context ();
			unowned Ide.UnsavedFiles unsaved_files_object = Ide.UnsavedFiles.from_context (context);
			var unsaved_files = unsaved_files_object.to_array ();
			//lock (seq_by_file) {
				/*if (seq_by_file == null)
					seq_by_file = new GLib.HashTable<GLib.File, int64?> (GLib.File.hash, GLib.File.equal);*/

				/*
				 * We need to sync buffers to the subprocess, but only those that are of any
				 * consequence to us. So that means Vala files.
				 *
				 * Further more, to avoid the chatter, we only want to send updated buffers
				 * for unsaved files which we have not already sent or we'll be way too
				 * chatty and cancel any cached translation units the subprocess has.
				 *
				 * Since the subprocess processes commands in order, we can simply call the
				 * function to set the buffer on the peer and ignore the result (and it will
				 * be used on subsequence commands).
				 */
				unsaved_files.foreach ((unsaved_file) => {
					unowned GLib.File file = unsaved_file.get_file ();

					string? name = file.get_basename ();
					if (name  == null || !(name.has_suffix (".vala") ||
						                   name.has_suffix (".vapi")))
						return;

					set_buffer_async.begin (file, unsaved_file.get_content ());
				});
			//}
		}

		public async bool set_buffer_async (GLib.File file, GLib.Bytes? bytes = null, GLib.Cancellable? cancellable = null) throws GLib.Error {
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("File must be a local file");

			var dict = new VariantDict ();
			dict.insert ("path", "s", file.get_path ());

			/* data doesn't need to be utf-8, but it does have to be
			 * a valid byte string (no embedded \0 bytes).
			 */
			if (bytes != null && bytes.get_data () != null) {
				dict.insert ("contents", "^ay", bytes.get_data ());
			}

			try {
				yield call_async ("vala/setBuffer", dict.end (), cancellable);
			} catch (Error e) {
				throw e;
			}

			return true;
		}

		public async Ide.Diagnostics diagnose_async (GLib.File file,
		                                             string[]? flags = null,
		                                             GLib.Cancellable? cancellable = null)
			throws GLib.Error
		{
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("Only native files are supported");

			sync_buffers ();
			var params = Jsonrpc.Message.@new (
				"path", Jsonrpc.Message.PutString.create (file.get_path ()),
				"flags", Jsonrpc.Message.PutStrv.create (flags)
			);

			try {
				var reply = yield call_async ("vala/diagnose",
				                              params,
				                              cancellable);
				var iter = reply.iterator ();
				GLib.Variant variant;
				var ret = new Ide.Diagnostics ();
				while ((variant = iter.next_value ()) != null) {
					var diag = new Ide.Diagnostic.from_variant (variant);
					if (diag != null) {
						ret.take (diag);
					}
				}

				return ret;
			} catch (Error e) {
				throw e;
			}
		}
	}
}
