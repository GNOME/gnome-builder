/* ide-vala-indenter.vala
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

using Gtk;
using Ide;

namespace Ide
{
	public class ValaIndenter: Ide.Object, Ide.Indenter
	{
		public bool is_trigger (Gdk.EventKey evkey)
		{
			switch (evkey.keyval) {

			/* newline indents */
			case Gdk.Key.Return:
			case Gdk.Key.KP_Enter:
				return true;

			/* close multiline comment */
			case Gdk.Key.slash:
				return true;

			default:
				return false;
			}
		}

		public string? format (Gtk.TextView text_view,
		                       Gtk.TextIter begin,
		                       Gtk.TextIter end,
		                       out int      cursor_offset,
		                       Gdk.EventKey evkey)
		{
			Gtk.SourceView source_view = (text_view as Gtk.SourceView);
			bool was_newline = is_newline_keyval (evkey.keyval);
			Gtk.TextIter copy = end;

			cursor_offset = 0;

			/* Move us back to the just inserted character */
			copy.backward_char ();

			/* If we are in a comment, continue the indentation. */
			if (in_comment (text_view, copy)) {
				/* maybe close a multiline comment */
				if (copy.get_char () == '/') {
					Gtk.TextIter close = copy;
					if (close.backward_char () && close.get_char () == ' ' &&
					    close.backward_char () && close.get_char () == '*') {
						begin.backward_char ();
						begin.backward_char ();
						return "/";
					}
				}

				if (was_newline)
					return indent_comment (text_view, copy);
			}

			if (is_newline_in_braces (copy)) {
				string prefix = copy_indent (text_view, copy);
				string indent;

				if (source_view.insert_spaces_instead_of_tabs)
					indent = "    ";
				else
					indent = "\t";

				cursor_offset = -prefix.length - 1;
				return (prefix + indent + "\n" + prefix);
			}

			if (was_newline)
				return copy_indent (text_view, copy);

			return null;
		}

		string? copy_indent (Gtk.TextView text_view,
		                     Gtk.TextIter iter)
		{
			Gtk.TextIter begin = iter;
			Gtk.TextIter end;

			begin.set_line_offset (0);
			end = begin;

			while (!end.ends_line () && end.get_char ().isspace () && end.forward_char ()) {
				/* Do nothing */
			}

			return begin.get_slice (end);
		}

		string get_line_text (Gtk.TextIter iter)
		{
			Gtk.TextIter begin = iter;
			Gtk.TextIter end = iter;

			begin.set_line_offset (0);
			if (!end.ends_line ())
				end.forward_to_line_end ();

			return begin.get_slice (end);
		}

		string? indent_comment (Gtk.TextView text_view,
		                        Gtk.TextIter iter)
		{
			string line = get_line_text (iter).strip ();

			/* continue with another single line comment */
			if (line.has_prefix ("//"))
				return copy_indent (text_view, iter) + "// ";

			/* comment is closed, copy indent, possibly trimming extra space */
			if (line.has_suffix ("*/")) {
				if (line.has_prefix ("*")) {
					var str = new GLib.StringBuilder (copy_indent (text_view, iter));
					if (str.str.has_suffix (" "))
						str.truncate (str.len - 1);
					return str.str;
				}
			}

			if (line.has_prefix ("/*") && !line.has_suffix ("*/"))
				return copy_indent (text_view, iter) + " * ";
			else if (line.has_prefix ("*"))
				return copy_indent (text_view, iter) + "* ";

			return copy_indent (text_view, iter);
		}

		bool in_comment (Gtk.TextView text_view,
		                 Gtk.TextIter iter)
		{
			Gtk.SourceBuffer buffer = text_view.buffer as Gtk.SourceBuffer;
			Gtk.TextIter copy = iter;

			copy.backward_char ();

			return buffer.iter_has_context_class (copy, "comment");
		}

		bool is_newline_keyval (uint keyval)
		{
			switch (keyval) {
			case Gdk.Key.Return:
			case Gdk.Key.KP_Enter:
				return true;

			default:
				return false;
			}
		}

		bool is_newline_in_braces (Gtk.TextIter iter)
		{
			Gtk.TextIter prev = iter;
			Gtk.TextIter next = iter;

			prev.backward_char ();
			next.forward_char ();

			return (prev.get_char () == '{') && (iter.get_char () == '\n') && (next.get_char () == '}');
		}
	}
}
