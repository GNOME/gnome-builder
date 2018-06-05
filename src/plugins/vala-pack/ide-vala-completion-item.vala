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
	public class ValaCompletionItem : GLib.Object, Ide.CompletionProposal
	{
		internal Vala.Symbol symbol;
		internal uint priority;

		public ValaCompletionItem (Vala.Symbol symbol)
		{
			this.symbol = symbol;
		}

		public void set_priority (uint priority)
		{
			this.priority = priority;
		}

		public unowned string get_name ()
		{
			return this.symbol.name;
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
			{
				var str = symbol as Vala.Struct;
				if (str.is_boolean_type () || str.is_integer_type () || str.is_floating_type ())
					return "lang-typedef-symbolic";
				return "lang-struct-symbolic";
			}
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

		string? esc_angle_brackets (string? in) {
			if (in == null)
				return null;
		    return in.replace ("<", "&lt;").replace (">", "&gt;");
		}

		public string get_markup (string? typed_text)
		{
			GLib.StringBuilder str = new GLib.StringBuilder ();

			var highlight = Ide.CompletionItem.fuzzy_highlight (this.symbol.name, typed_text != null ? typed_text : "");

			if (highlight != null)
				str.append(highlight);

			if (this.symbol is Vala.Method) {
				var method = symbol as Vala.Method;
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
				str.append (" <span fgalpha='32767'>(");
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

					var escaped = esc_angle_brackets (param.variable_type.to_qualified_string (method.owner));
					if (escaped != null)
						str.append_printf ("%s, ", escaped);
				}
				if (parameters.size > 0) {
					str.truncate (str.len - 2);
				}
				str.append (")</span>");
			}

			return str.str;
		}

		public string? get_return_type () {
			if (this.symbol is Vala.Method) {
				var method = this.symbol as Vala.Method;
				return esc_angle_brackets (method.return_type.to_qualified_string (this.symbol.owner));
			}
			if (this.symbol is Vala.Property) {
				var prop = this.symbol as Vala.Property;
				return esc_angle_brackets (prop.property_type.to_qualified_string (this.symbol.owner));
			}
			if (this.symbol is Vala.Variable) {
				var variable = this.symbol as Vala.Variable;
				return esc_angle_brackets (variable.variable_type.to_qualified_string (this.symbol.owner));
			}
			return null;
		}

		public string? get_misc () {
			if (this.symbol is Vala.Class) {
				var klass = this.symbol as Vala.Class;
				if (klass.is_abstract)
					return _("Abstract");
				if (klass.is_compact)
					return _("Compact");
				if (klass.is_immutable)
					return _("Immutable");
			}
			return null;
		}

		public Ide.Snippet get_snippet ()
		{
			var snippet = new Ide.Snippet (null, null);
			var chunk = new Ide.SnippetChunk ();

			chunk.set_spec (this.symbol.name);
			snippet.add_chunk (chunk);

			return snippet;
		}
	}
}
