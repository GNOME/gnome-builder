/* ide-vala-completion-provider.vala
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
	/*
	 * TODO:
	 *
	 *   There is some fun optimization work we can do here to work around the
	 *   list interface of GtkSourceCompletionContext. However, I'm not yet
	 *   sure how to do this in Vala. In C, of course, it's trivial.
	 *
	 *   The goal here is to reduce the overhead of sorting the GList of items
	 *   that needs to be sent to GtkSourceCompletionContext. We may want to
	 *   reorder them based on the input text or another better sorting
	 *   heuristic. But doing that on a malloc'd list is expensive and
	 *   annoying.
	 *
	 *   We could solve this by having an array of GList,pointer fields and
	 *   update the GList items after sorting. We could also just embed the
	 *   GList in the Ide.ValaCompletionitem (but I don't know how to do this
	 *   in Vala. I guess we could make a subclass for this too.
	 *
	 *   This way, when looking at our previous items, we sort using a
	 *   cacheline efficient array of items, and just update the pointers
	 *   at the same time or at the end.
	 *
	 *   For now, we'll do the dumb stupid slow thing.
	 */
	public class ValaCompletionProvider: Ide.Object,
	                                     Gtk.SourceCompletionProvider,
	                                     Ide.CompletionProvider
	{
		ArrayList<Ide.ValaCompletionItem>? last_results;
		string? last_line;
		string? last_prefix;
		int line = -1;
		int column = -1;

		public void populate (Gtk.SourceCompletionContext context)
		{
			Gtk.TextIter iter;
			Gtk.TextIter begin;

			if (!context.get_iter (out iter)) {
				context.add_proposals (this, null, true);
				return;
			}

			begin = iter;
			begin.set_line_offset (0);
			var line = begin.get_slice (iter);

			/*
			 * We might be able to reuse the results from our previous query if
			 * the buffer is sufficiently similar. If so, possibly just
			 * rearrange some things and redisplay those results.
			 */
			if (can_replay (line)) {
				HashTable<void*,bool> dedup = new HashTable<void*,bool> (GLib.direct_hash, GLib.direct_equal);
				Gtk.TextIter stop = iter;

				/* Move to the just inserted character. */
				if (!stop.starts_line ())
					stop.backward_char ();

				/*
				 * Walk backwards to locate the first character after a stop
				 * character (anything non-alphanumeric or _).
				 */
				while (!stop.starts_line () &&
					   (stop.get_char ().isalnum () || stop.get_char () == '_') &&
					   stop.backward_char ()) {
					/* Do nothing */
				}
				var ch = stop.get_char ();
				if (!ch.isalnum () && (ch != ')') && (stop.compare (iter) < 0))
					stop.forward_char ();

				var prefix = stop.get_slice (iter).strip ();
				var downcase = prefix.down ();
				var results = new GLib.List<Gtk.SourceCompletionProposal> ();

				/* See the comment above about optimizing this. */
				foreach (var result in this.last_results) {
					if (result.matches (downcase)) {
						var hash = (void*)result.hash ();
						if (dedup.contains ((void*)hash))
							continue;
						results.prepend (result);
						dedup.insert (hash, true);
					}
				}

				this.last_prefix = prefix;

				results.reverse ();

				context.add_proposals (this, results, true);

				return;
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

			var service = (this.get_context ()
			                   .get_service_typed (typeof (Ide.ValaService)) as Ide.ValaService);
			var index = service.index;
			var unsaved_files = this.get_context ().get_unsaved_files ();

			var cancellable = new GLib.Cancellable ();
			context.cancelled.connect(() => {
				cancellable.cancel ();
			});

			index.code_complete.begin (file.file,
			                           iter.get_line () + 1,
			                           iter.get_line_offset () + 1,
			                           line,
			                           unsaved_files,
			                           null,
			                           (obj,res) => {
				int res_line = -1;
				int res_column = -1;

				var results = index.code_complete.end (res, out res_line , out res_column);

				if (res_line > 0 && res_column > 0) {
					this.line = res_line - 1;
					this.column = res_column - 1;
				}

				if (!cancellable.is_cancelled ()) {
					/* TODO: fix more brain dead slow list conversion stuff */
					var list = new GLib.List<Ide.ValaCompletionItem> ();
					foreach (var item in results) {
						list.prepend (item);
						item.set_markup_func (this.markup_func);
					}
					context.add_proposals (this, list, true);
				}

				this.last_results = results;
				this.last_line = line;
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

			return true;
		}

		/*
		 * Check to see if this line can be replayed using the results from
		 * the previous query. We can do that if the characters that have
		 * changed are simply alphanumeric or _ (function or symbol name
		 * characters). We just need to massage the results appropriately.
		 */
		bool can_replay (string? line)
		{
			if (line == null || this.last_line == null)
				return false;

			if (!line.has_prefix (this.last_line))
				return false;

			var suffix = line.offset (this.last_line.length);

			while (suffix[0] != '\0') {
				var ch = suffix.get_char ();
				if (!ch.isalnum() && ch != '_')
					return false;
				suffix = suffix.next_char ();
			}

			return true;
		}

		string markup_func (string name)
		{
			return highlight_full (name, this.last_prefix, true, 1);
		}
	}

	[CCode (cheader_filename = "gb-string.h", cname = "gb_str_highlight_full")]
	extern unowned string? highlight_full (string haystack, string needle, bool insensitive, int type);
}
