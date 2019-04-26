/* ide-vala-code-context.vala
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
 * Copyright 2019 Collabora Ltd.
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
	public class ValaCodeContext: Vala.CodeContext
	{
		private const string DEFAULT_COLORS = "error=01;31:warning=01;35:note=01;36:caret=01;32:locus=01:quote=01";

		static string _basedir;
		static string _directory;
		[CCode (array_length = false, array_null_terminated = true)]
		static string[] _vapi_directories;
		[CCode (array_length = false, array_null_terminated = true)]
		static string[] _gir_directories;
		[CCode (array_length = false, array_null_terminated = true)]
		static string[] _metadata_directories;
		static string vapi_filename;
		static string library;
		static string shared_library;
		static string gir;
		[CCode (array_length = false, array_null_terminated = true)]
		static string[] packages;
		static string target_glib;
		[CCode (array_length = false, array_null_terminated = true)]
		static string[] _gresources;
		[CCode (array_length = false, array_null_terminated = true)]
		static string[] _gresources_directories;

		static bool _ccode_only;
		static bool _abi_stability;
		static string _header_filename;
		static bool _use_header;
		static string _internal_header_filename;
		static string internal_vapi_filename;
		static string fast_vapi_filename;
		static bool _vapi_comments;
		static string _symbols_filename;
		static string _includedir;
		static bool _compile_only;
		static string _output;
		static bool _debug;
		static bool _mem_profiler;
		static bool disable_assert;
		static bool enable_checking;
		static bool _deprecated;
		static bool _hide_internal;
		static bool _experimental;
		static bool _experimental_non_null;
		static bool _gobject_tracing;
		static bool disable_since_check;
		static bool disable_warnings;
		static string cc_command;
		[CCode (array_length = false, array_null_terminated = true)]
		static string[] cc_options;
		static string _pkg_config_command;
		static string dump_tree;
		static bool _save_temps;
		[CCode (array_length = false, array_null_terminated = true)]
		static string[] defines;
		static bool quiet_mode;
		static bool _verbose_mode;
		static string _profile;
		static bool _nostdpkg;
		static bool enable_version_header;
		static bool disable_version_header;
		static bool fatal_warnings;
		static string dependencies;

		static string _entry_point;

		static bool _run_output;
		static string run_args;

		const OptionEntry[] options = {
			{ "vapidir", 0, 0, OptionArg.FILENAME_ARRAY, ref _vapi_directories, "Look for package bindings in DIRECTORY", "DIRECTORY..." },
			{ "girdir", 0, 0, OptionArg.FILENAME_ARRAY, ref _gir_directories, "Look for .gir files in DIRECTORY", "DIRECTORY..." },
			{ "metadatadir", 0, 0, OptionArg.FILENAME_ARRAY, ref _metadata_directories, "Look for GIR .metadata files in DIRECTORY", "DIRECTORY..." },
			{ "pkg", 0, 0, OptionArg.STRING_ARRAY, ref packages, "Include binding for PACKAGE", "PACKAGE..." },
			{ "vapi", 0, 0, OptionArg.FILENAME, ref vapi_filename, "Output VAPI file name", "FILE" },
			{ "library", 0, 0, OptionArg.STRING, ref library, "Library name", "NAME" },
			{ "shared-library", 0, 0, OptionArg.STRING, ref shared_library, "Shared library name used in generated gir", "NAME" },
			{ "gir", 0, 0, OptionArg.STRING, ref gir, "GObject-Introspection repository file name", "NAME-VERSION.gir" },
			{ "basedir", 'b', 0, OptionArg.FILENAME, ref _basedir, "Base source directory", "DIRECTORY" },
			{ "directory", 'd', 0, OptionArg.FILENAME, ref _directory, "Change output directory from current working directory", "DIRECTORY" },
			{ "ccode", 'C', 0, OptionArg.NONE, ref _ccode_only, "Output C code", null },
			{ "header", 'H', 0, OptionArg.FILENAME, ref _header_filename, "Output C header file", "FILE" },
			{ "use-header", 0, 0, OptionArg.NONE, ref _use_header, "Use C header file", null },
			{ "includedir", 0, 0, OptionArg.FILENAME, ref _includedir, "Directory used to include the C header file", "DIRECTORY" },
			{ "internal-header", 'h', 0, OptionArg.FILENAME, ref _internal_header_filename, "Output internal C header file", "FILE" },
			{ "internal-vapi", 0, 0, OptionArg.FILENAME, ref internal_vapi_filename, "Output vapi with internal api", "FILE" },
			{ "vapi-comments", 0, 0, OptionArg.NONE, ref _vapi_comments, "Include comments in generated vapi", null },
			{ "deps", 0, 0, OptionArg.STRING, ref dependencies, "Write make-style dependency information to this file", null },
			{ "symbols", 0, 0, OptionArg.FILENAME, ref _symbols_filename, "Output symbols file", "FILE" },
			{ "compile", 'c', 0, OptionArg.NONE, ref _compile_only, "Compile but do not link", null },
			{ "output", 'o', 0, OptionArg.FILENAME, ref _output, "Place output in file FILE", "FILE" },
			{ "debug", 'g', 0, OptionArg.NONE, ref _debug, "Produce debug information", null },
			{ "enable-mem-profiler", 0, 0, OptionArg.NONE, ref _mem_profiler, "Enable GLib memory profiler", null },
			{ "define", 'D', 0, OptionArg.STRING_ARRAY, ref defines, "Define SYMBOL", "SYMBOL..." },
			{ "main", 0, 0, OptionArg.STRING, ref _entry_point, "Use SYMBOL as entry point", "SYMBOL..." },
			{ "nostdpkg", 0, 0, OptionArg.NONE, ref _nostdpkg, "Do not include standard packages", null },
			{ "disable-assert", 0, 0, OptionArg.NONE, ref disable_assert, "Disable assertions", null },
			{ "enable-checking", 0, 0, OptionArg.NONE, ref enable_checking, "Enable additional run-time checks", null },
			{ "enable-deprecated", 0, 0, OptionArg.NONE, ref _deprecated, "Enable deprecated features", null },
			{ "hide-internal", 0, 0, OptionArg.NONE, ref _hide_internal, "Hide symbols marked as internal", null },
			{ "enable-experimental", 0, 0, OptionArg.NONE, ref _experimental, "Enable experimental features", null },
			{ "disable-warnings", 0, 0, OptionArg.NONE, ref disable_warnings, "Disable warnings", null },
			{ "fatal-warnings", 0, 0, OptionArg.NONE, ref fatal_warnings, "Treat warnings as fatal", null },
			{ "disable-since-check", 0, 0, OptionArg.NONE, ref disable_since_check, "Do not check whether used symbols exist in local packages", null },
			{ "enable-experimental-non-null", 0, 0, OptionArg.NONE, ref _experimental_non_null, "Enable experimental enhancements for non-null types", null },
			{ "enable-gobject-tracing", 0, 0, OptionArg.NONE, ref _gobject_tracing, "Enable GObject creation tracing", null },
			{ "cc", 0, 0, OptionArg.STRING, ref cc_command, "Use COMMAND as C compiler command", "COMMAND" },
			{ "Xcc", 'X', 0, OptionArg.STRING_ARRAY, ref cc_options, "Pass OPTION to the C compiler", "OPTION..." },
			{ "pkg-config", 0, 0, OptionArg.STRING, ref _pkg_config_command, "Use COMMAND as pkg-config command", "COMMAND" },
			{ "dump-tree", 0, 0, OptionArg.FILENAME, ref dump_tree, "Write code tree to FILE", "FILE" },
			{ "save-temps", 0, 0, OptionArg.NONE, ref _save_temps, "Keep temporary files", null },
			{ "profile", 0, 0, OptionArg.STRING, ref _profile, "Use the given profile instead of the default", "PROFILE" },
			{ "quiet", 'q', 0, OptionArg.NONE, ref quiet_mode, "Do not print messages to the console", null },
			{ "verbose", 'v', 0, OptionArg.NONE, ref _verbose_mode, "Print additional messages to the console", null },
			{ "target-glib", 0, 0, OptionArg.STRING, ref target_glib, "Target version of glib for code generation", "MAJOR.MINOR" },
			{ "gresources", 0, 0, OptionArg.FILENAME_ARRAY, ref _gresources, "XML of gresources", "FILE..." },
			{ "gresourcesdir", 0, 0, OptionArg.FILENAME_ARRAY, ref _gresources_directories, "Look for resources in DIRECTORY", "DIRECTORY..." },
			{ "enable-version-header", 0, 0, OptionArg.NONE, ref enable_version_header, "Write vala build version in generated files", null },
			{ "disable-version-header", 0, 0, OptionArg.NONE, ref disable_version_header, "Do not write vala build version in generated files", null },
			{ "run-args", 0, 0, OptionArg.STRING, ref run_args, "Arguments passed to directly compiled executeable", null },
			{ "abi-stability", 0, 0, OptionArg.NONE, ref _abi_stability, "Enable support for ABI stability", null },
			{ null }
		};

		public void parse_arguments (string[] args) {
			if (args != null) {
				stderr.printf ("gnome-builder-vala: %s\n", string.joinv (" ", args));
				try {
					var opt_context = new OptionContext ();
					opt_context.add_main_entries (options, null);
					opt_context.set_ignore_unknown_options (true);
					opt_context.parse (ref args);
				} catch (OptionError e) {
					GLib.debug ("%s", e.message);
				}
			}

			this.assert = !disable_assert;
			this.checking = enable_checking;
			this.deprecated = _deprecated;
			this.since_check = !disable_since_check;
			this.hide_internal = _hide_internal;
			this.experimental = _experimental;
			this.experimental_non_null = _experimental_non_null;
			this.gobject_tracing = _gobject_tracing;
			this.report.enable_warnings = !disable_warnings;
			this.report.set_verbose_errors (!quiet_mode);
			this.verbose_mode = _verbose_mode;
			this.version_header = !disable_version_header;

			this.ccode_only = _ccode_only;
			if (_ccode_only && cc_options != null) {
				warning ("-X has no effect when -C or --ccode is set");
			}
			this.abi_stability = _abi_stability;
			this.compile_only = _compile_only;
			this.header_filename = _header_filename;
			if (_header_filename == null && _use_header) {
				critical ("--use-header may only be used in combination with --header");
			}

			this.use_header = _use_header;
			this.internal_header_filename = _internal_header_filename;
			this.symbols_filename = _symbols_filename;
			this.includedir = _includedir;
			this.output = _output;
			if (_output != null && _ccode_only) {
				warning ("--output and -o have no effect when -C or --ccode is set");
			}

			if (_basedir == null) {
				this.basedir = Vala.CodeContext.realpath (".");
			} else {
				this.basedir = Vala.CodeContext.realpath (_basedir);
			}

			if (_directory != null) {
				this.directory = Vala.CodeContext.realpath (_directory);
			} else {
				this.directory = this.basedir;
			}

			this.vapi_directories = _vapi_directories;
			this.vapi_comments = _vapi_comments;
			this.gir_directories = _gir_directories;
			this.metadata_directories = _metadata_directories;
			this.debug = _debug;
			this.mem_profiler = _mem_profiler;
			this.save_temps = _save_temps;
			if (_ccode_only && _save_temps) {
				warning ("--save-temps has no effect when -C or --ccode is set");
			}

			if (_profile == "posix") {
				this.profile = Vala.Profile.POSIX;
				this.add_define ("POSIX");
			} else if (_profile == "gobject-2.0" || _profile == "gobject" || _profile == null) {
				// default profile
				this.profile = Vala.Profile.GOBJECT;
				this.add_define ("GOBJECT");
			} else {
				critical ("Unknown profile %s".printf (_profile));
			}

			_nostdpkg |= fast_vapi_filename != null;
			this.nostdpkg = _nostdpkg;

			this.entry_point_name = _entry_point;

			this.run_output = _run_output;

			if (_pkg_config_command == null) {
				_pkg_config_command = GLib.Environment.get_variable ("PKG_CONFIG") ?? "pkg-config";
			}
			this.pkg_config_command = _pkg_config_command;

			if (defines != null) {
				foreach (unowned string define in defines) {
					this.add_define (define);
				}
			}

			if (this.profile == Vala.Profile.POSIX) {
				if (!_nostdpkg) {
					/* default package */
					this.add_external_package ("posix");
				}
			} else if (this.profile == Vala.Profile.GOBJECT) {
				if (target_glib != null) {
					this.set_target_glib_version (target_glib);
				}

				if (!_nostdpkg) {
					/* default packages */
					this.add_external_package ("glib-2.0");
					this.add_external_package ("gobject-2.0");
				}
			}

			if (packages != null) {
				foreach (unowned string package in packages) {
					this.add_external_package (package);
				}
				packages = null;
			}

			this.gresources = _gresources;
			this.gresources_directories = _gresources_directories;

			if (dependencies != null) {
				this.write_dependencies (dependencies);
			}
		}

		public void add_source (string path) {
			Vala.SourceFile? source_file = get_source (path);
			if (source_file == null) {
				add_source_filename (path);
			}
		}

		public Vala.SourceFile? get_source (string path) {
			foreach (var source_file in get_source_files ()) {
				if (source_file.filename == path) {
					return source_file;
				}
			}

			return null;
		}
	}
}
