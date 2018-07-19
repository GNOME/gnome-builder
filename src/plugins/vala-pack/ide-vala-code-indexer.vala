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
	bool is_null_or_empty (string? s)
	{
		return s == null || s[0] == '\0';
	}

	public class ValaCodeIndexer : Ide.Object, Ide.CodeIndexer
	{
		public async Ide.CodeIndexEntries index_file_async (GLib.File file,
		                                                    [CCode (array_length = false, array_null_terminated = true)] string[]? build_flags,
		                                                    GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			if (!file.is_native ())
				throw new GLib.IOError.NOT_SUPPORTED ("Only native files are supported");

			unowned Ide.ValaClient? client = (Ide.ValaClient)context.get_service_typed (typeof (Ide.ValaClient));
			try {
				var entries = yield client.index_file_async (file, build_flags, cancellable);
				return new ValaCodeIndexEntries (file, entries);
			} catch (Error e) {
				throw e;
			}
		}

		public async string generate_key_async (Ide.SourceLocation location,
		                                        [CCode (array_length = false, array_null_terminated = true)] string[]? build_flags,
		                                        GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			unowned Ide.ValaClient? client = (Ide.ValaClient)context.get_service_typed (typeof (Ide.ValaClient));
			unowned Ide.File ifile = location.get_file ();
			var line = location.get_line () + 1;
			var column = location.get_line_offset () + 1;
			try {
				return yield client.get_index_key_async (ifile.file, build_flags, line, column, cancellable);
			} catch (Error e) {
				throw e;
			}
		}
	}

	public class ValaCodeIndexEntries : GLib.Object, Ide.CodeIndexEntries
	{
		GLib.GenericArray<Ide.CodeIndexEntry> entries;
		GLib.File file;
		uint pos;

		public GLib.File get_file ()
		{
			return this.file;
		}

		public ValaCodeIndexEntries (GLib.File file, GLib.Variant ventries)
		{
			entries = new GLib.GenericArray<Ide.CodeIndexEntry> ();
			this.file = file;
			var iter = ventries.iterator ();
			Ide.SymbolFlags flags;
			Ide.SymbolKind kind;
			string key;
			string name;
			uint begin_line;
			uint begin_line_offset;
			uint end_line;
			uint end_line_offset;
			while (iter.next ("(usisuuuu)",
			                  out flags,
			                  out key,
			                  out kind,
			                  out name,
			                  out begin_line,
			                  out begin_line_offset,
			                  out end_line,
			                  out end_line_offset)) {
				var entry_builder = new Ide.CodeIndexEntryBuilder ();
				entry_builder.set_flags (flags);
				entry_builder.set_key (key);
				entry_builder.set_kind (kind);
				entry_builder.set_name (name);
				entry_builder.set_range (begin_line, begin_line_offset, end_line, end_line_offset);
				entries.add (entry_builder.build ());
			}
		}

		public Ide.CodeIndexEntry? get_next_entry ()
		{
			if (this.pos < entries.length)
				return this.entries [this.pos++];
			return null;
		}

        public async GLib.GenericArray<Ide.CodeIndexEntry> next_entries_async (GLib.Cancellable? cancellable)
			throws GLib.Error
        {
			var ret = new GLib.GenericArray<Ide.CodeIndexEntry> ();

			for (;;)
			{
				Ide.CodeIndexEntry entry = get_next_entry ();

				if (entry == null)
					break;
				ret.add (entry);
			}

			return ret;
		}
	}
}
