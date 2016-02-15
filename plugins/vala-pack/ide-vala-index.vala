/* ide-vala-index.vala
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

/*
 * The point of Ide.ValaIndex is somewhat analogous to Clang's CXIndex.
 * It is the top-level container for everything you can do with vala
 * files for a particular context. Typically, you would have one index
 * per project. Therefore, we use the singleton-per-project nature of
 * Ide.Service (via Ide.ValaService) to keep an index-per-project.
 */

using GLib;
using Gtk;
using Ide;
using Vala;

namespace Ide
{
	public class ValaIndex: GLib.Object
	{
		Vala.CodeContext code_context;
		Vala.Parser parser;
		HashMap<GLib.File,Ide.ValaSourceFile> source_files;
		Ide.ValaDiagnostics report;

		public ValaIndex ()
		{
			this.source_files = new HashMap<GLib.File,Ide.ValaSourceFile> (GLib.File.hash, (GLib.EqualFunc)GLib.File.equal);

			this.code_context = new Vala.CodeContext ();

			Vala.CodeContext.push (this.code_context);

			/*
			 * TODO: Some of the following options could be extracted by parsing
			 *       the contents of *_VALAFLAGS or AM_VALAFLAGS in automake.
			 *       We need to do this in a somewhat build system agnostic fashion
			 *       since there seems to a cargo cult of cmake/vala.
			 */

			this.code_context.assert = true;
			this.code_context.checking = false;
			this.code_context.deprecated = false;
			this.code_context.hide_internal = false;
			this.code_context.experimental = false;
			this.code_context.experimental_non_null = false;
			this.code_context.gobject_tracing = false;
			this.code_context.nostdpkg = false;
			this.code_context.ccode_only = true;
			this.code_context.compile_only = true;
			this.code_context.use_header = false;
			this.code_context.includedir = null;
			this.code_context.basedir = GLib.Environment.get_current_dir ();
			this.code_context.directory = GLib.Environment.get_current_dir ();
			this.code_context.debug = false;
			this.code_context.thread = true;
			this.code_context.mem_profiler = false;
			this.code_context.save_temps = false;

			this.code_context.profile = Vala.Profile.GOBJECT;
			this.code_context.add_define ("GOBJECT");

			this.code_context.entry_point_name = null;

			this.code_context.run_output = false;

			for (var i = 2; i <= 32; i += 2) {
				this.code_context.add_define ("VALA_0_%d".printf (i));
			}

			for (var i = 16; i < GLib.Version.minor; i+= 2) {
				this.code_context.add_define ("GLIB_2_%d".printf (i));
			}

			this.code_context.add_external_package ("glib-2.0");
			this.code_context.add_external_package ("gobject-2.0");

			/* TODO: find packages from build system */
			this.code_context.add_external_package ("gio-2.0");
			this.code_context.add_external_package ("gtk+-3.0");
			this.code_context.add_external_package ("gtksourceview-3.0");
			this.code_context.add_external_package ("libide-1.0");
			this.code_context.add_external_package ("libpeas-1.0");
			this.code_context.add_external_package ("libvala-0.32");

			this.report = new Ide.ValaDiagnostics ();
			this.code_context.report = this.report;

			this.parser = new Vala.Parser ();
			this.parser.parse (this.code_context);

			this.code_context.check ();

			Vala.CodeContext.pop ();
		}

		void add_file (GLib.File file)
		{
			var path = file.get_path ();
			if (path == null)
				return;

			var source_file = new Ide.ValaSourceFile (this.code_context, Vala.SourceFileType.SOURCE, path, null, false);
			this.code_context.add_source_file (source_file);

			this.source_files [file] = source_file;
		}

		public async void add_files (ArrayList<GLib.File> files,
		                             GLib.Cancellable? cancellable)
		{
			Ide.ThreadPool.push (Ide.ThreadPoolKind.COMPILER, () => {
				lock (this.code_context) {
					Vala.CodeContext.push (this.code_context);
					foreach (var file in files)
						this.add_file (file);
					Vala.CodeContext.pop ();
					GLib.Idle.add(add_files.callback);
				}
			});

			yield;
		}

		public async bool parse_file (GLib.File file,
		                              Ide.UnsavedFiles? unsaved_files,
		                              GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			GLib.GenericArray<UnsavedFile>? unsaved_files_copy = null;

			if (unsaved_files != null) {
				unsaved_files_copy = unsaved_files.to_array ();
			}

			Ide.ThreadPool.push (Ide.ThreadPoolKind.COMPILER, () => {
				if ((cancellable == null) || !cancellable.is_cancelled ()) {
					lock (this.code_context) {
						Vala.CodeContext.push (this.code_context);

						if (!this.source_files.contains (file))
							this.add_file (file);

						this.apply_unsaved_files (unsaved_files_copy);
						this.reparse ();
						this.code_context.check ();

						GLib.Idle.add(this.parse_file.callback);

						Vala.CodeContext.pop ();
					}
				}
			});

			yield;

			return true;
		}

		public Ide.CompletionResults code_complete (GLib.File file,
		                                            int line,
		                                            int column,
		                                            string? line_text,
		                                            Ide.UnsavedFiles? unsaved_files,
		                                            Ide.ValaCompletionProvider provider,
		                                            GLib.Cancellable? cancellable,
		                                            out int result_line,
		                                            out int result_column)
		{
			var unsaved_files_copy = unsaved_files.to_array ();
			var result = new Ide.CompletionResults (provider.query);

			if ((cancellable == null) || !cancellable.is_cancelled ()) {
				lock (this.code_context) {
					Vala.CodeContext.push (this.code_context);

					this.apply_unsaved_files (unsaved_files_copy);
					this.reparse ();
					this.code_context.check ();

					if (this.source_files.contains (file)) {
						var source_file = this.source_files [file];
						string? text = (line_text == null) ? source_file.get_source_line (line) : line_text;
						var locator = new Ide.ValaLocator ();
						var nearest = locator.locate (source_file, line, column);

						this.add_completions (source_file, ref line, ref column, text, nearest, result, provider);
					}

					Vala.CodeContext.pop ();
				}
			}

			result_line = line;
			result_column = column;

			return result;
		}

		public async Ide.Diagnostics? get_diagnostics (GLib.File file,
		                                               GLib.Cancellable? cancellable = null)
		{
			Ide.Diagnostics? diagnostics = null;

			Ide.ThreadPool.push (Ide.ThreadPoolKind.COMPILER, () => {
				if ((cancellable == null) || !cancellable.is_cancelled ()) {
					lock (this.code_context) {
						Vala.CodeContext.push (this.code_context);
						if (this.source_files.contains (file)) {
							diagnostics = this.source_files[file].diagnose ();
						}
						Vala.CodeContext.pop ();
					}
				}
				GLib.Idle.add(this.get_diagnostics.callback);
			});

			yield;

			return diagnostics;
		}

		void apply_unsaved_files (GLib.GenericArray<Ide.UnsavedFile> unsaved_files)
		{
			foreach (var source_file in this.code_context.get_source_files ()) {
				if ((source_file.file_type == Vala.SourceFileType.SOURCE) &&
				    (source_file is Ide.ValaSourceFile)) {
					(source_file as Ide.ValaSourceFile).sync (unsaved_files);
				}
			}
		}

		void reparse ()
		{
			this.report.clear ();

			foreach (var source_file in this.code_context.get_source_files ()) {
				if (source_file.get_nodes ().size == 0) {
					if (source_file is Ide.ValaSourceFile) {
						this.parser.visit_source_file (source_file);
						(source_file as Ide.ValaSourceFile).dirty = false;
					} else {
						this.parser.visit_source_file (source_file);
					}
				}
			}
		}

		void add_completions (Ide.ValaSourceFile source_file,
		                      ref int line,
		                      ref int column,
		                      string line_text,
		                      Vala.Symbol? nearest,
		                      Ide.CompletionResults results,
		                      Ide.ValaCompletionProvider provider)
		{
			var block = nearest as Vala.Block;
			Vala.SourceLocation cursor = Vala.SourceLocation (null, line, column);

			// TODO: our list building could use a lot of low-hanging optimizations.
			//       the list/array/list in particular.
			//       it would be nice to stay as an array as long as possible.

			var completion = new Ide.ValaCompletion (this.code_context, cursor, line_text, block);
			var list = completion.run (ref cursor);

			foreach (var symbol in list) {
				if (symbol.name != null && symbol.name[0] != '\0')
					results.take_proposal (new Ide.ValaCompletionItem (symbol, provider));
			}

			line = cursor.line;
			column = cursor.column;
		}

		public async Vala.Symbol? find_symbol_at (GLib.File file, int line, int column)
		{
			Vala.Symbol? symbol = null;

			/*
			 * TODO: This isn't quite right because the locator is finding
			 *       us the nearest block instead of the symbol. We need to
			 *       adjust the locator to find the exact expression.
			 */

			Ide.ThreadPool.push (Ide.ThreadPoolKind.COMPILER, () => {
				lock (this.code_context) {
					Vala.CodeContext.push (this.code_context);

					if (!this.source_files.contains (file)) {
						this.add_file (file);
						this.reparse ();
					}

					var source_file = this.source_files [file];
					var locator = new Ide.ValaLocator ();

					symbol = locator.locate (source_file, line, column);

					Vala.CodeContext.pop ();
				}

				this.find_symbol_at.callback ();
			});

			yield;

			return symbol;
		}

		public async Ide.SymbolTree? get_symbol_tree (GLib.File file,
		                                              GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			Ide.SymbolTree? ret = null;

			Ide.ThreadPool.push (Ide.ThreadPoolKind.COMPILER, () => {
				lock (this.code_context) {
					Vala.CodeContext.push (this.code_context);

					if (!this.source_files.contains (file)) {
						this.add_file (file);
						this.reparse ();
					}

					var source_file = this.source_files [file];
					if (source_file.dirty) {
						this.reparse ();
					}

					var tree_builder = new Ide.ValaSymbolTreeVisitor ();
					source_file.accept_children (tree_builder);
					ret = tree_builder.build_tree ();

					Vala.CodeContext.pop ();

					GLib.Idle.add (this.get_symbol_tree.callback);
				}
			});

			yield;

			return ret;
		}
	}
}

