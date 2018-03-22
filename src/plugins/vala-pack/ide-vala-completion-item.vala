/* ide-vala-completion-item.vala
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
using Gtk;
using Vala;

namespace Ide
{
	public class ValaCompletionItem: Ide.CompletionItem, Gtk.SourceCompletionProposal
	{
		static uint hash_seed;

		internal Vala.Symbol symbol;
		weak Ide.ValaCompletionProvider provider;
		string label;

		static construct {
			hash_seed = "IdeValaCompletionItem".hash ();
		}

		public ValaCompletionItem (Vala.Symbol symbol, Ide.ValaCompletionProvider provider)
		{
			this.symbol = symbol;
			this.provider = provider;

			/* Unfortunate we have to do this here, because it would
			 * be better to do this lazy. But that would require access to
			 * the code context while it could be getting mutated.
			 */
			this.build_label ();
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

		public override bool match (string query, string casefold)
		{
			uint priority = 0;
			bool result = Ide.CompletionItem.fuzzy_match (this.symbol.name, casefold, out priority);
			this.set_priority (priority);
			return result;
		}

		private string esc_angle_brackets (string in) {
		    return in.replace ("<", "&lt;").replace (">", "&gt;");
		}

		public void build_label ()
		{
			GLib.StringBuilder str = new GLib.StringBuilder ();

			if (this.symbol is Vala.Method) {
				var method = symbol as Vala.Method;
				str.append (esc_angle_brackets (method.return_type.to_qualified_string (symbol.owner)));
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

					str.append_printf ("%s, ", esc_angle_brackets (param.variable_type.to_qualified_string (method.owner)));
				}
				if (parameters.size > 0) {
					str.truncate (str.len - 2);
				}
				str.append_c (')');
			} else {
				str.append (this.symbol.name);
			}

			/* Steal the string instead of strdup */
			this.label = (owned)str.str;
		}

		public string get_markup () {
			if (this.provider.query != null)
				return Ide.CompletionItem.fuzzy_highlight (this.label, this.provider.query);
			return this.label;
		}

		public string get_label () {
			return this.label;
		}

		public string get_text ()
		{
			return this.symbol.name;
		}

		public string? get_info () {
			return null;
		}

		public unowned GLib.Icon? get_gicon () {
			return null;
		}

		public uint hash ()
		{
			return this.symbol.name.hash () ^ hash_seed;
		}
	}
}
