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
	public class ValaCompletionProvider: Ide.Object, Ide.CompletionProvider
	{
		Ide.ValaService? service;

		public void load (Ide.Context context)
		{
			this.service = context.get_service_typed (typeof (Ide.ValaService)) as Ide.ValaService;
		}

		public bool is_trigger (Gtk.TextIter iter, unichar ch)
		{
			return ch == '.';
		}

		public bool refilter (Ide.CompletionContext context,
		                      GLib.ListModel proposals)
		{
			(proposals as ValaCompletionResults).refilter (context.get_word ());
			return true;
		}

		public async GLib.ListModel populate_async (Ide.CompletionContext context,
		                                            GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			Ide.ValaCompletionResults results = null;
			var buffer = context.get_buffer () as Ide.Buffer;
			var file = buffer.get_file ();
			Gtk.TextIter iter;

			if (file.is_temporary) {
				throw new GLib.IOError.NOT_SUPPORTED ("Cannot complete on temporary files");
			}

			buffer.sync_to_unsaved_files ();

			context.get_bounds (out iter, null);
			var line = iter.get_line ();
			var line_offset = iter.get_line_offset ();

			var index = this.service.index;
			var unsaved_files = this.get_context ().get_unsaved_files ();

			/* make a copy for threaded access */
			var unsaved_files_copy = unsaved_files.to_array ();

			Ide.ThreadPool.push (Ide.ThreadPoolKind.COMPILER, () => {
				int res_line = -1;
				int res_column = -1;
				results = index.code_complete (file.file,
				                               line + 1,
				                               line_offset + 1,
				                               unsaved_files_copy,
				                               cancellable,
				                               out res_line,
				                               out res_column);
				GLib.Idle.add (this.populate_async.callback);
			});

			yield;

			if (cancellable.is_cancelled () || results == null)
				throw new GLib.IOError.CANCELLED ("operation was cancelled");

			return results;
		}

		public string? get_title () {
			return "Vala";
		}

		public int get_priority ()
		{
			return 200;
		}

		public GLib.Icon? get_icon ()
		{
			return null;
		}

		public string? get_comment (Ide.CompletionProposal proposal)
		{
			var comment = (proposal as ValaCompletionItem).symbol.comment;

			if (comment != null)
				return comment.content;

			return null;
		}

		public bool key_activates (Ide.CompletionProposal proposal,
		                           Gdk.EventKey key)
		{
			var item = proposal as ValaCompletionItem;

			if (key.keyval == Gdk.Key.period) {
				return item.symbol is Vala.Variable ||
				       item.symbol is Vala.Method ||
				       item.symbol is Vala.Class ||
				       item.symbol is Vala.Property;
			}

			if (key.keyval == Gdk.Key.semicolon) {
				return true;
			}

			return false;
		}

		public void display_proposal (Ide.CompletionListBoxRow row,
		                              Ide.CompletionContext context,
		                              string? typed_text,
		                              Ide.CompletionProposal proposal)
		{
			Ide.ValaCompletionItem item = (proposal as Ide.ValaCompletionItem);
			var markup = item.get_markup (typed_text);
			var left = item.get_return_type ();
			var right = item.get_misc ();

			// never show "null"
			if (left == "null")
				left = null;

			row.set_icon_name (item.get_icon_name ());
			row.set_left_markup (left);
			row.set_center_markup (markup);
			row.set_right (right);
		}

		public void activate_proposal (Ide.CompletionContext context,
		                               Ide.CompletionProposal proposal,
		                               Gdk.EventKey key)
		{
			Gtk.TextIter begin, end;

			var buffer = context.get_buffer ();
			var view = context.get_view () as Ide.SourceView;
			var item = proposal as ValaCompletionItem;
			var snippet = item.get_snippet ();

			if (key.keyval == Gdk.Key.period) {
				var chunk = new Ide.SnippetChunk ();
				chunk.set_spec (".");
				snippet.add_chunk (chunk);
			}

			if (key.keyval == Gdk.Key.semicolon) {
				var chunk = new Ide.SnippetChunk ();
				chunk.set_spec (";");
				snippet.add_chunk (chunk);
			}

			buffer.begin_user_action ();
			if (context.get_bounds (out begin, out end))
				buffer.delete (ref begin, ref end);
			view.push_snippet (snippet, begin);
			buffer.end_user_action ();
		}
	}

	public class ValaCompletionResults : GLib.Object, GLib.ListModel
	{
		GLib.GenericArray<ValaCompletionItem> items;
		GLib.GenericArray<ValaCompletionItem> filtered;
		string? filter;

		construct {
			this.items = new GLib.GenericArray<ValaCompletionItem> ();
			this.filtered = new GLib.GenericArray<ValaCompletionItem> ();
			this.filter = null;
		}

		public GLib.Type get_item_type ()
		{
			return typeof (ValaCompletionItem);
		}

		public uint get_n_items ()
		{
			return this.filtered.length;
		}

		public GLib.Object? get_item (uint position)
		{
			return this.filtered[position];
		}

		public void add (Vala.Symbol symbol)
		{
			var item = new ValaCompletionItem (symbol);

			this.items.add (item);

			if (matches (item, this.filter))
				this.filtered.add (item);
		}

		public void refilter (string? word)
		{
			uint old_len = this.filtered.length;

			this.filter = word.casefold ();

			if (old_len > 0)
				this.filtered.remove_range (0, old_len);

			for (var i = 0; i < this.items.length; i++) {
				var item = this.items[i];

				if (matches (item, word))
					this.filtered.add (item);
			}

			this.items_changed (0, old_len, this.filtered.length);
		}

		bool matches (Ide.ValaCompletionItem item, string? typed_text)
		{
			uint priority = 0;

			if (typed_text == null || typed_text[0] == '\0')
			{
				item.set_priority (0);
				return true;
			}

			if (Ide.CompletionItem.fuzzy_match (item.get_name (), this.filter, out priority))
			{
				item.set_priority (priority);
				return true;
			}

			item.set_priority (0);
			return false;
		}
	}
}
