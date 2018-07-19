/* ide-vala-index.vala
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
 * Copyright 2018 Collabora Ltd.
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
 *
 * Authors: Christian Hergert <christian@hergert.me>
 *          Corentin Noël <corentin.noel@collabora.com>
 */

namespace Ide
{
	public class ValaIndex: GLib.Object
	{
		Vala.CodeContext code_context;
		Vala.Parser parser;
		Ide.ValaDiagnostics report;
		GLib.File workdir;

		GLib.HashTable<string, GLib.Bytes> unsaved_files;

		public ValaIndex (GLib.File workdir)
		{
			this.workdir = workdir;
			code_context = new Vala.CodeContext ();
			unsaved_files = new GLib.HashTable<string, GLib.Bytes> (str_hash, str_equal);

			Vala.CodeContext.push (code_context);

			/*
			 * TODO: Some of the following options could be extracted by parsing
			 *       the contents of *_VALAFLAGS or AM_VALAFLAGS in automake.
			 *       We need to do this in a somewhat build system agnostic fashion
			 *       since there seems to a cargo cult of cmake/vala.
			 */

			code_context.assert = true;
			code_context.checking = false;
			code_context.deprecated = false;
			code_context.hide_internal = false;
			code_context.experimental = false;
			code_context.experimental_non_null = false;
			code_context.gobject_tracing = false;
			code_context.nostdpkg = false;
			code_context.ccode_only = true;
			code_context.compile_only = true;
			code_context.use_header = false;
			code_context.includedir = null;
			code_context.basedir = workdir.get_path ();
			code_context.directory = GLib.Environment.get_current_dir ();
			code_context.debug = false;
			code_context.mem_profiler = false;
			code_context.save_temps = false;

			code_context.profile = Vala.Profile.GOBJECT;
			code_context.add_define ("GOBJECT");

			code_context.entry_point_name = null;

			code_context.run_output = false;

			int minor = 36;
			var tokens = Config.VALA_VERSION.split(".", 2);
			if (tokens[1] != null) {
				minor = int.parse(tokens[1]);
			}

			for (var i = 2; i <= minor; i += 2) {
				code_context.add_define ("VALA_0_%d".printf (i));
			}

			for (var i = 16; i < GLib.Version.minor; i+= 2) {
				code_context.add_define ("GLIB_2_%d".printf (i));
			}

			code_context.vapi_directories = {};

			/* $prefix/share/vala-0.32/vapi */
			string versioned_vapidir = get_versioned_vapidir ();
			if (versioned_vapidir != null) {
				add_vapidir_locked (versioned_vapidir);
			}

			/* $prefix/share/vala/vapi */
			string unversioned_vapidir = get_unversioned_vapidir ();
			if (unversioned_vapidir != null) {
				add_vapidir_locked (unversioned_vapidir);
			}

			code_context.add_external_package ("glib-2.0");
			code_context.add_external_package ("gobject-2.0");

			report = new Ide.ValaDiagnostics ();
			code_context.report = report;

			parser = new Vala.Parser ();
			parser.parse (code_context);

			code_context.check ();

			load_directory (workdir);
			Vala.CodeContext.pop ();
		}

		public void set_unsaved_file (string path,
		                              GLib.Bytes? bytes)
		{
			if (bytes == null) {
				unsaved_files.remove (path);
			} else {
				unsaved_files[path] = bytes;
			}
		}

		public Ide.Diagnostics get_file_diagnostics (string path,
		                                             string[] flags)
		{
			lock (this.code_context) {
				Vala.CodeContext.push (this.code_context);
				load_build_flags (flags);
				add_file (GLib.File.new_for_path (path));
				reparse ();
				if (report.get_errors () == 0) {
					code_context.check ();
				}

				Vala.CodeContext.pop ();
			}

			return report.get_diagnostic_from_file (path);
		}

		public GLib.Variant get_symbol_tree (string path,
		                                     string[] flags)
		{
			GLib.Variant symbol_tree;
			lock (this.code_context) {
				Vala.CodeContext.push (this.code_context);
				load_build_flags (flags);

				if (add_file (GLib.File.new_for_path (path)))
					reparse ();

				var tree_builder = new Ide.ValaSymbolTreeVisitor ();
				foreach (var source_file in code_context.get_source_files ()) {
					if (source_file.filename == path) {
						source_file.accept_children (tree_builder);
						break;
					}
				}

				symbol_tree = tree_builder.build_tree ();

				Vala.CodeContext.pop ();
			}

			return symbol_tree;
		}

		public GLib.Variant get_index_entries (string path,
		                                       string[] flags)
		{
			GLib.Variant index_entries;
			lock (this.code_context) {
				Vala.CodeContext.push (this.code_context);
				load_build_flags (flags);

				if (add_file (GLib.File.new_for_path (path)))
					reparse ();

				var tree_builder = new Ide.ValaSymbolTreeVisitor ();
				foreach (var source_file in code_context.get_source_files ()) {
					if (source_file.filename == path) {
						source_file.accept_children (tree_builder);
						break;
					}
				}

				index_entries = tree_builder.build_index_entries ();

				Vala.CodeContext.pop ();
			}

			return index_entries;
		}

		public Ide.Symbol? locate_symbol (string path,
		                                  string[] flags,
		                                  uint line,
		                                  uint column)
		{
			Ide.Symbol? symbol = null;
			lock (this.code_context) {
				Vala.CodeContext.push (this.code_context);
				load_build_flags (flags);

				if (add_file (GLib.File.new_for_path (path)))
					reparse ();

				foreach (var source_file in code_context.get_source_files ()) {
					if (source_file.filename == path) {
						var locator = new Ide.ValaLocator ();
						var vala_node = locator.locate (source_file, line, column);
						if (vala_node != null && vala_node is Vala.Symbol) {
							symbol = Ide.vala_to_ide_symbol (vala_node as Vala.Symbol);
						}

						break;
					}
				}

				Vala.CodeContext.pop ();
			}

			return symbol;
		}

		public Ide.Symbol? find_nearest_scope (string path,
		                                       string[] flags,
		                                       uint line,
		                                       uint column)
		{
			Ide.Symbol? ide_symbol = null;
			lock (this.code_context) {
				Vala.CodeContext.push (this.code_context);
				load_build_flags (flags);
				var symbol = find_nearest_symbol (path, line, column);
				if (symbol != null) {
					ide_symbol = Ide.vala_to_ide_symbol (symbol);
				}

				Vala.CodeContext.pop ();
			}

			return ide_symbol;
		}

		public GLib.Variant code_complete (string path,
		                                   uint line,
		                                   uint column,
		                                   string? line_text)
		{
			GLib.Variant array;
			lock (this.code_context) {
				Vala.CodeContext.push (this.code_context);
				var block = find_nearest_symbol (path, line, column) as Vala.Block;
				Vala.SourceLocation cursor = Vala.SourceLocation (null, (int)line, (int)column);
				var completion = new Ide.ValaCompletion (code_context, cursor, line_text, block);
				Vala.ArrayList<Vala.Symbol>? symbols = completion.run (ref cursor);
				var variant_builder = new GLib.VariantBuilder (new GLib.VariantType ("aa{sv}"));
				foreach (var symbol in symbols) {
					var dict = new GLib.VariantDict ();
					dict.insert ("name", "s", symbol.name);
					if (symbol is Vala.LocalVariable || symbol is Vala.Variable) {
						dict.insert ("type", "s", "variable");
						var variable = symbol as Vala.Variable;
						dict.insert ("returns", "s", variable.variable_type.to_qualified_string (symbol.owner));
					} else if (symbol is Vala.Field)
						dict.insert ("type", "s", "field");
					else if (symbol is Vala.Subroutine)
						dict.insert ("type", "s", "function");
					else if (symbol is Vala.Namespace)
						dict.insert ("type", "s", "namespace");
					else if (symbol is Vala.MemberAccess)
						dict.insert ("type", "s", "member");
					else if (symbol is Vala.Property) {
						dict.insert ("type", "s", "property");
						var prop = symbol as Vala.Property;
						dict.insert ("returns", "s", prop.property_type.to_qualified_string (symbol.owner));
					} else if (symbol is Vala.Struct) {
						var str = symbol as Vala.Struct;
						if (str.is_simple_type ())
							dict.insert ("type", "s", "simpletype");
						else
							dict.insert ("type", "s", "struct");
					} else if (symbol is Vala.Class) {
						dict.insert ("type", "s", "class");
						var cls = symbol as Vala.Class;
						if (cls.is_abstract) {
							dict.insert ("misc", "s", "abstract");
						} else if (cls.is_compact) {
							dict.insert ("misc", "s", "compact");
						} else if (cls.is_immutable) {
							dict.insert ("misc", "s", "immutable");
						}
					} else if (symbol is Vala.Enum)
						dict.insert ("type", "s", "enum");
					else if (symbol is Vala.EnumValue)
						dict.insert ("type", "s", "enum-value");
					else if (symbol is Vala.Delegate)
						dict.insert ("type", "s", "delegate");
					else if (symbol is Vala.Method) {
						dict.insert ("type", "s", "method");
						var method = symbol as Vala.Method;
						var type_params = method.get_type_parameters ();
						if (type_params.size > 0) {
							string[] params_name = {};
							foreach (var type_param in type_params) {
								params_name += type_param.name;
							}

							dict.insert ("type-parameters", "as", params_name);
						}

						var params_dict = new GLib.VariantDict ();
						var parameters = method.get_parameters ();
						foreach (var param in parameters) {
							if (param.ellipsis) {
								params_dict.insert ("dir", "s", "ellipsis");
								break;
							}

							if (param.direction == Vala.ParameterDirection.OUT)
								params_dict.insert ("dir", "s", "out");
							else if (param.direction == Vala.ParameterDirection.REF)
								params_dict.insert ("dir", "s", "ref");

							params_dict.insert ("type", "s", param.variable_type.to_qualified_string (method.owner));
						}

						dict.insert_value ("params", params_dict.end ());
					}

					variant_builder.add_value (dict.end ());
				}

				array = variant_builder.end ();
				Vala.CodeContext.pop ();
			}

			return array;
		}

		private void reparse ()
		{
			report.clear ();

			foreach (var source_file in code_context.get_source_files ()) {
				if (source_file.get_nodes ().size == 0) {
					parser.visit_source_file (source_file);
				}
			}
		}

		private Vala.Symbol? find_nearest_symbol (string path,
		                                          uint line,
		                                          uint column)
		{
			Vala.Symbol? symbol = null;
			if (add_file (GLib.File.new_for_path (path)))
				reparse ();

			apply_unsaved_files ();
			foreach (var source_file in code_context.get_source_files ()) {
				if (source_file.filename == path) {
					var locator = new Ide.ValaLocator ();
					var vala_node = locator.locate (source_file, line, column) as Vala.Symbol;
					while (vala_node != null) {
						if (vala_node is Vala.Class ||
							vala_node is Vala.Subroutine ||
							vala_node is Vala.Namespace ||
							vala_node is Vala.Struct)
							break;

						if (vala_node.owner != null)
							vala_node = vala_node.owner.owner;
						else
							vala_node = vala_node.parent_symbol;
					}

					symbol = vala_node;
					break;
				}
			}

			return symbol;
		}

		private bool add_file (GLib.File file)
		{
			var path = file.get_path ();
			if (path == null)
				return false;

			foreach (var source_file in code_context.get_source_files ()) {
				if (source_file.filename == path)
					return false;
			}

			var type = Vala.SourceFileType.SOURCE;
			if (path.has_suffix ("vapi"))
				type = Vala.SourceFileType.PACKAGE;

			var source_file = new Ide.ValaSourceFile (code_context, type, path, null, false);
			code_context.add_source_file (source_file);
			return true;
		}

		private void apply_unsaved_files () {
			foreach (var source_file in code_context.get_source_files ()) {
				if (source_file is Ide.ValaSourceFile) {
					GLib.Bytes? content = unsaved_files[source_file.filename];
					(source_file as Ide.ValaSourceFile).sync (content);
					unsaved_files.remove (source_file.filename);
				}
			}
		}

		private void load_directory (GLib.File directory,
		                             GLib.Cancellable? cancellable = null)
		{
			try {
				var enumerator = directory.enumerate_children (FileAttribute.STANDARD_NAME+","+FileAttribute.STANDARD_TYPE, 0, cancellable);

				FileInfo file_info;
				while ((file_info = enumerator.next_file ()) != null) {
					var name = file_info.get_name ();

					if (name == ".flatpak-builder" || name == ".git")
						continue;

					if (file_info.get_file_type () == GLib.FileType.DIRECTORY) {
						var child = directory.get_child (file_info.get_name ());
						load_directory (child, cancellable);
					} else if (name.has_suffix (".vala") || name.has_suffix (".vapi")) {
						add_file (directory.get_child (file_info.get_name ()));
					}
				}

				enumerator.close ();
			} catch (GLib.Error err) {
				warning (err.message);
			}
		}

		private void load_build_flags (string[] flags)
		{
			lock (code_context) {
				Vala.CodeContext.push (code_context);

				var packages = new Vala.ArrayList<string> ();

				var len = flags.length;
				for (var i = 0; i < len; i++) {
					string next_param = null;
					string param = flags[i];

					if (param.contains ("=")) {
						var offset = param.index_of("=") + 1;
						next_param = param.offset(offset);
					} else if (i + 1 < len) {
						next_param = flags[i + 1];
					}

					if (next_param != null) {
						if (param.has_prefix("--pkg")) {
							packages.add (next_param);
						} else if (param.has_prefix ("--vapidir")) {
							add_vapidir_locked (next_param);
						} else if (param.has_prefix ("--vapi")) {
							packages.add (next_param);
						} else if (param.has_prefix ("--girdir")) {
							add_girdir_locked (next_param);
						} else if (param.has_prefix ("--metadatadir")) {
							add_metadatadir_locked (next_param);
						} else if (param.has_prefix ("--target-glib")) {
							/* TODO: Parse glib version ~= 2.44 */
						}

						continue;
					}
					else if (param.has_suffix (".vapi")) {
						if (!GLib.Path.is_absolute (param)) {
							var child = workdir.get_child (param);
							add_file (child);
						} else {
							add_file (GLib.File.new_for_path (param));
						}
					}
				}

				/* Now add external packages after vapidir/girdir have been added */
				foreach (var package in packages) {
					code_context.add_external_package (package);
				}

				Vala.CodeContext.pop ();
			}
		}

		/* Caller is expected to hold code_context lock */
		private void add_vapidir_locked (string vapidir)
		{
			var dirs = code_context.vapi_directories;
			if (vapidir in dirs)
				return;

			debug ("Adding vapidir %s", vapidir);
			dirs += vapidir;
			code_context.vapi_directories = dirs;
		}

		/* Caller is expected to hold code_context lock */
		private void add_girdir_locked (string girdir)
		{
			var dirs = code_context.gir_directories;
			if (girdir in dirs)
				return;

			dirs += girdir;
			code_context.gir_directories = dirs;
		}

		/* Caller is expected to hold code_context lock */
		private void add_metadatadir_locked (string metadata_dir)
		{
			var dirs = code_context.metadata_directories;
			if (metadata_dir in dirs)
				return;

			dirs += metadata_dir;
			code_context.metadata_directories = dirs;
		}

		static string? get_versioned_vapidir ()
		{
			try {
				var pkgname = "libvala-%s".printf (Config.VALA_VERSION);
				string outstr = null;
				var subprocess = new GLib.Subprocess (GLib.SubprocessFlags.STDOUT_PIPE,
					                                  "pkg-config",
					                                  "--variable=vapidir",
					                                  pkgname,
					                                  null);
				subprocess.communicate_utf8 (null, null, out outstr, null);
				outstr = outstr.strip ();
				return outstr;
			} catch (GLib.Error er) {
				warning ("%s", er.message);
				return null;
			}
		}

		static string? get_unversioned_vapidir ()
		{
			string versioned_vapidir = get_versioned_vapidir ();

			if (versioned_vapidir != null) {
				return GLib.Path.build_filename (versioned_vapidir,
				                                 "..", "..", "vala", "vapi", null);
			}

			return null;
		}
	}
}
