/* ide-vala-completion-provider.vala
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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
using Vala;

namespace Ide
{
	public class ValaCompletionProvider: GLib.Object,
	                                     Gtk.SourceCompletionProvider,
	                                     Ide.CompletionProvider
	{
		internal string? query;
		int line = -1;
		int column = -1;
		Ide.CompletionResults? results;
		Ide.Context? _context;

		public void populate (Gtk.SourceCompletionContext context)
		{
			Gtk.TextIter iter;
			Gtk.TextIter begin;

			if (!context.get_iter (out iter)) {
				context.add_proposals (this, null, true);
				return;
			}

			this.query = CompletionProvider.context_current_word (context);

			begin = iter;
			begin.set_line_offset (0);
			var line = begin.get_slice (iter);

			if (this.results != null) {
				// If we are right after a . then we cannot reuse our
				// previous results since they will be for a different
				// object type, also, ensure that the word has the same
				// prefix as our initial query or we must regenerate
				// our results.
				if (!line.has_suffix (".") &&
				    this.line == iter.get_line () &&
				    this.results.replay (this.query)) {
					this.results.present (this, context);
					return;
				}
				this.results = null;
			}

			this.line = -1;
			this.column = -1;

			var buffer = iter.get_buffer () as Ide.Buffer;
			var file = buffer.file;

			if (file == null || file.is_temporary) {
				context.add_proposals (this, null, true);
				return;
			}

			buffer.sync_to_unsaved_files ();

			var service = (this._context.get_service_typed (typeof (Ide.ValaService)) as Ide.ValaService);
			var index = service.index;
			var unsaved_files = this._context.get_unsaved_files ();

			/* make a copy for threaded access */
			var unsaved_files_copy = unsaved_files.to_array ();

			var cancellable = new GLib.Cancellable ();
			context.cancelled.connect(() => {
				cancellable.cancel ();
			});

			Ide.ThreadPool.push (Ide.ThreadPoolKind.INDEXER, () => {
				int res_line = -1;
				int res_column = -1;
				this.results = index.code_complete (file.file,
				                                    iter.get_line () + 1,
				                                    iter.get_line_offset () + 1,
				                                    line,
				                                    unsaved_files_copy,
				                                    this,
				                                    cancellable,
				                                    out res_line,
				                                    out res_column);
				if (res_line > 0 && res_column > 0) {
					this.line = res_line - 1;
					this.column = res_column - 1;
				}

				Idle.add (() => {
					if (!cancellable.is_cancelled ())
						this.results.present (this, context);
					return false;
				});
			});
		}

		public bool match (Gtk.SourceCompletionContext context)
		{
			Gtk.TextIter iter;

			if (!context.get_iter (out iter))
				return false;

			var buffer = iter.get_buffer () as Ide.Buffer;

			if (buffer.file == null || buffer.file.file == null) {
				return false;
			}

			/*
			 * Only match if we were user requested or the character is not after
			 * whitespace.
			 */
			if (context.activation != Gtk.SourceCompletionActivation.USER_REQUESTED) {
				if (iter.starts_line () || !iter.backward_char () || iter.get_char ().isspace ())
					return false;
			}

			if (Ide.CompletionProvider.context_in_comment_or_string (context))
				return false;

			return true;
		}

		public string get_name () {
			return "Vala";
		}

		public int get_priority ()
		{
			return 200;
		}

		public void load (Ide.Context context) {
			this._context = context;
		}
	}
}
