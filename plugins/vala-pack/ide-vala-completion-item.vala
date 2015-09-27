/* ide-vala-completion-item.vala
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
using Gtk;
using Vala;

namespace Ide
{
	public delegate string ValaCompletionMarkupFunc (string name);

	public class ValaCompletionItem: GLib.Object, Gtk.SourceCompletionProposal
	{
		static uint hash_seed;

		Vala.Symbol symbol;
		ValaCompletionMarkupFunc? markup_func;
		string label;

		static construct {
			hash_seed = "IdeValaCompletionItem".hash ();
		}

		public ValaCompletionItem (Vala.Symbol symbol)
		{
			this.symbol = symbol;

			/* Unfortunate we have to do this here, because it would
			 * be better to do this lazy. But that would require access to
			 * the code context while it could be getting mutated.
			 */
			this.build_label ();
		}

		public void set_markup_func (owned ValaCompletionMarkupFunc? func)
		{
			this.markup_func = func;
		}

		public unowned string? get_icon_name ()
		{
			if (symbol is Vala.LocalVariable)
				return "lang-variable-symbolic";
			else if (symbol is Vala.Field)
				return "struct-field-symbolic";
			else if (symbol is Vala.Subroutine)
				return "lang-function-symbolic";
			else if (symbol is Vala.Namespace)
				return "lang-include-symbolic";
			else if (symbol is Vala.MemberAccess)
				return "struct-field-symbolic";
			else if (symbol is Vala.Property)
				return "struct-field-symbolic";
			else if (symbol is Vala.Struct)
				return "lang-struct-symbolic";
			else if (symbol is Vala.Class)
				return "lang-class-symbolic";
			else if (symbol is Vala.Enum)
				return "lang-enum-symbolic";
			else if (symbol is Vala.EnumValue)
				return "lang-enum-value-symbolic";
			else if (symbol is Vala.Delegate)
				return "lang-typedef-symbolic";

			return null;
		}

		public bool matches (string? prefix_lower)
		{
			if (prefix_lower == null || prefix_lower [0] == '\0')
				return true;

			return gb_str_simple_match (this.symbol.name, prefix_lower);
		}

		public string get_label () {
			return this.symbol.name;
		}

		public void build_label ()
		{
			GLib.StringBuilder str = new GLib.StringBuilder ();

			if (this.symbol is Vala.Method) {
				var method = symbol as Vala.Method;
				str.append (method.return_type.to_qualified_string (symbol.owner));
				str.append_printf (" %s", method.name);
				var type_params = method.get_type_parameters ();
				if (type_params.size > 0) {
					str.append ("&lt;");
					foreach (var type_param in type_params) {
						str.append (type_param.name);
						str.append_c (',');
					}
					str.truncate (str.len - 1);
					str.append ("&gt;");
				}
				str.append (" (");
				var parameters = method.get_parameters ();
				foreach (var param in parameters) {
					if (param.ellipsis) {
						str.append ("..., ");
						break;
					}

					if (param.direction == ParameterDirection.OUT)
						str.append ("out ");
					else if (param.direction == ParameterDirection.REF)
						str.append ("ref ");

					str.append_printf ("%s, ", param.variable_type.to_qualified_string (method.owner));
				}
				if (parameters.size > 0) {
					str.truncate (str.len - 2);
				}
				str.append_c (')');
			} else {
				str.append (this.symbol.name);
			}

			this.label = str.str;
		}

		public string get_markup () {
			if (this.markup_func != null)
				return this.markup_func (this.label);
			return this.label;
		}

		public string get_text () {
			return this.symbol.name;
		}

		public uint hash ()
		{
			return this.symbol.name.hash () ^ hash_seed;
		}

		public unowned GLib.Icon? get_gicon () { return null; }
		public unowned Gdk.Pixbuf? get_icon () { return null; }
		public string? get_info () { return null; }
	}

	[CCode (cheader_filename = "gb-string.h", cname = "gb_str_simple_match")]
	extern bool gb_str_simple_match (string? text, string? prefix_lower);
}

