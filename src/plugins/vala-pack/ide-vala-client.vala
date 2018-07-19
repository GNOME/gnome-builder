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
	public class ValaClient : Ide.Object, Ide.Service {
		private enum State {
			INITIAL,
			SPAWNING,
			RUNNING,
			SHUTDOWN
		}

		GLib.Queue<Ide.Task> get_client;
		Ide.SubprocessSupervisor supervisor;
		Jsonrpc.Client rpc_client;
		GLib.File root_uri;
		//GLib.HashTable<GLib.File, int64?> seq_by_file = null;
		State state = State.INITIAL;

		construct {
			get_client = new GLib.Queue<Ide.Task> ();
			root_uri = context.vcs.working_directory;

			var launcher = new Ide.SubprocessLauncher (GLib.SubprocessFlags.STDOUT_PIPE | GLib.SubprocessFlags.STDIN_PIPE);
			if (root_uri.is_native ())
				launcher.cwd = root_uri.get_path ();

			launcher.clean_env = false;
			launcher.push_argv (Config.PACKAGE_LIBEXECDIR + "/gnome-builder-vala");
			supervisor = new Ide.SubprocessSupervisor ();
			supervisor.set_launcher (launcher);
			supervisor.spawned.connect (subprocess_spawned);
			supervisor.exited.connect (subprocess_exited);
			context.buffer_manager.buffer_saved.connect (buffer_saved);
		}

		~ValaClient () {
			state = State.SHUTDOWN;
			if (supervisor != null) {
				var _supervisor = supervisor;
				supervisor = null;
				_supervisor.stop ();
			}
		}

		public unowned string get_name () {
			return "vala client";
		}

		public void start () {

		}

		public void stop () {

		}

		public void subprocess_exited (Ide.Subprocess object) {
			if (state == State.RUNNING)
				state = State.SPAWNING;

			rpc_client = null;
			/*lock (seq_by_file) {
				seq_by_file = null;
			}*/
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

			Ide.Task task = null;
			while ((task = get_client.pop_head ()) != null) {
				task.return_object (rpc_client);
			}

			var @params = Jsonrpc.Message.@new (
				"rootUri", Jsonrpc.Message.PutString.create (root_uri.get_uri ()),
				"rootPath", Jsonrpc.Message.PutString.create (root_uri.get_path ()),
				"processId", Jsonrpc.Message.PutInt64.create (Posix.getpid ()),
				"capabilities", "{", "}"
			);

			rpc_client.call_async.begin ("initialize", params, null);
		}

		public void buffer_saved (Ide.Buffer buffer) {
			/*
			 * We need to clear the cached buffer on the peer (and potentially
			 * pop the translation unit cache) now that the buffer has been
			 * saved to disk and we no longer need the draft.
			 */

			unowned GLib.File gfile = buffer.file.file;
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
		                                            [CCode (array_length = false, array_null_terminated = true)] string[]? flags = null,
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
		                                         [CCode (array_length = false, array_null_terminated = true)] string[]? flags = null,
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

		private async Jsonrpc.Client get_client_async (GLib.Cancellable? cancellable) throws GLib.Error {
			switch (state) {
				default:
					state = State.SPAWNING;
					Idle.add (() => {
						supervisor.start ();
						var task = new Ide.Task (this, cancellable, (GLib.TaskReadyCallback)get_client_async.callback);
						get_client.push_tail (task);
						return false;
					});
					yield;
					return rpc_client;
				case State.SPAWNING:
					Idle.add (() => {
						var task = new Ide.Task (this, cancellable, (GLib.TaskReadyCallback)get_client_async.callback);
						get_client.push_tail (task);
						return false;
					});
					yield;
					return rpc_client;
				case State.RUNNING:
					return rpc_client;
				case State.SHUTDOWN:
					throw new GLib.IOError.CLOSED ("The client has been closed");
			}
		}

		private void sync_buffers () {
			unowned Ide.UnsavedFiles unsaved_files_object = context.unsaved_files;
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
					//int64? prev = seq_by_file[file];
					int64 seq = unsaved_file.get_sequence ();
					/*if (prev != null && seq <= prev)
						return;*/

					string? name = file.get_basename ();
					if (name  == null || !(name.has_suffix (".vala") ||
						                   name.has_suffix (".vapi")))
						return;

					//seq_by_file.insert (file, seq);
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
		                                             [CCode (array_length = false, array_null_terminated = true)] string[]? flags = null,
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
				var ret = new Ide.Diagnostics (null);
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
