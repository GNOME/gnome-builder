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
		Ide.ValaCodeContext code_context;
		Vala.Parser parser;
		Ide.ValaDiagnostics report;
		GLib.File workdir;

		public ValaIndex (GLib.File workdir)
		{
			this.workdir = workdir;
			code_context = new Ide.ValaCodeContext ();

			Vala.CodeContext.push (code_context);

			report = new Ide.ValaDiagnostics ();
			code_context.report = report;

			parser = new Vala.Parser ();
			parser.parse (code_context);

			load_directory (workdir);
			Vala.CodeContext.pop ();
		}

		public void set_unsaved_file (string path,
		                              string? content)
		{
			Vala.CodeContext.push (this.code_context);
			Vala.SourceFile? source_file = code_context.get_source (path);
			if (source_file != null) {
				if (source_file.content != content) {
					source_file.content = content;

					unowned Vala.Method? entry_point = code_context.entry_point;
					if (entry_point != null && entry_point.source_reference != null && entry_point.source_reference.file == source_file) {
						code_context.entry_point = null;
					}

					// Copy the node list since we will be mutating while iterating
					var copy = new Vala.ArrayList<Vala.CodeNode> ();
					copy.add_all (source_file.get_nodes ());
					foreach (var node in copy) {
						source_file.remove_node (node);
					}
				}
			} else if (content != null) {
				source_file = new Vala.SourceFile (code_context, Vala.SourceFileType.SOURCE, path, content);
				code_context.add_source_file (source_file);
			}

			Vala.CodeContext.pop ();
		}

		public Ide.Diagnostics get_file_diagnostics (string path,
		                                             string[] flags)
		{
			lock (this.code_context) {
				Vala.CodeContext.push (this.code_context);
				code_context.parse_arguments (flags);
				code_context.add_source (path);

				critical ("Diagnostics %s", path);
				report.clear ();
				code_context.check ();
				

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
				code_context.parse_arguments (flags);
				code_context.add_source (path);
				reparse ();

				var tree_builder = new Ide.ValaSymbolTreeVisitor ();
				Vala.SourceFile? source_file = code_context.get_source (path);
				if (source_file != null) {
					source_file.accept_children (tree_builder);
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
				code_context.parse_arguments (flags);
				code_context.add_source (path);
				reparse ();

				var tree_builder = new Ide.ValaSymbolTreeVisitor ();
				Vala.SourceFile? source_file = code_context.get_source (path);
				if (source_file != null) {
					source_file.accept_children (tree_builder);
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
				code_context.parse_arguments (flags);
				code_context.add_source (path);
				reparse ();

				Vala.SourceFile? source_file = code_context.get_source (path);
				if (source_file != null) {
					var locator = new Ide.ValaLocator ();
					var vala_node = locator.locate (source_file, line, column);
					if (vala_node != null) {
						symbol = Ide.vala_to_ide_symbol (vala_node);
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
				code_context.parse_arguments (flags);
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

		private Vala.CodeNode? find_nearest_symbol (string path,
		                                            uint line,
		                                            uint column)
		{
			Vala.CodeNode? symbol = null;
			code_context.add_source (path);
			reparse ();

			Vala.SourceFile? source_file = code_context.get_source (path);
			if (source_file != null) {
				var locator = new Ide.ValaLocator ();
				symbol = locator.locate (source_file, line, column);
			}

			return symbol;
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
					} else if (name.has_suffix (".vala") || name.has_suffix (".vapi") || name.has_suffix (".gs") || name.has_suffix (".c")) {
						var child = directory.get_child (file_info.get_name ());
						code_context.add_source (child.get_path ());
					}
				}

				enumerator.close ();
			} catch (GLib.Error err) {
				warning (err.message);
			}
		}
	}
}
