/*
 * Some of the code below is taken from Anjuta and is licensed under the
 * terms of the GPL v2. The original copyright is preserved.
 *
 * Copyright (C) 2008-2010 Abderrahim Kitouni
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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
	public class ValaCompletion: GLib.Object
	{
		static Regex member_access;
		static Regex member_access_split;
		static Regex function_call;

		Vala.CodeContext context;
		Vala.SourceLocation location;
		string current_text;
		Vala.Block? nearest;

		static construct {
			try {
				member_access = new Regex("""((?:\w+(?:\s*\([^()]*\))?\.)*)(\w*)$""");
				member_access_split = new Regex ("""(\s*\([^()]*\))?\.""");
				function_call = new Regex("""(new )?((?:\w+(?:\s*\([^()]*\))?\.)*)(\w+)\s*\(([^(,)]+,)*([^(,)]*)$""");
			} catch (RegexError err) {
				critical("Regular expressions failed to compile : %s", err.message);
			}
		}

		public ValaCompletion (Vala.CodeContext context,
		                       Vala.SourceLocation location,
		                       string current_text,
		                       Vala.Block? nearest)
		{
			this.context = context;
			this.location = location;
			this.current_text = current_text;
			this.nearest = nearest;
		}

		public GLib.List<Vala.Symbol>? run (ref Vala.SourceLocation start_pos)
		{
			MatchInfo match_info;

			if (!member_access.match (current_text, 0, out match_info))
				return null;
			else if (match_info.fetch(0).length < 2)
				return null;

			start_pos.line = this.location.line;
			start_pos.column = this.location.column - (int)match_info.fetch (2).length;

			var names = member_access_split.split (match_info.fetch (1));

			var syms = lookup_symbol (construct_member_access (names),
			                          match_info.fetch (2),
			                          true,
			                          nearest);

			return syms;
		}

		GLib.List<Vala.Symbol> lookup_symbol (Vala.Expression? inner, string name, bool prefix_match, Vala.Block? block)
		{
			var matching_symbols = new GLib.List<Vala.Symbol> ();

			if (block == null)
				return matching_symbols;

			if (inner == null) {
				for (var sym = (Vala.Symbol) block; sym != null; sym = sym.parent_symbol) {
					matching_symbols.concat (symbol_lookup_inherited (sym, name, prefix_match));
				}

				foreach (var ns in block.source_reference.file.current_using_directives) {
					matching_symbols.concat (symbol_lookup_inherited (ns.namespace_symbol, name, prefix_match));
				}
			} else if (inner.symbol_reference != null) {
					matching_symbols.concat (symbol_lookup_inherited (inner.symbol_reference, name, prefix_match));
			} else if (inner is Vala.MemberAccess) {
				var inner_ma = (Vala.MemberAccess) inner;
				var matching = lookup_symbol (inner_ma.inner, inner_ma.member_name, false, block);
				if (matching != null)
					matching_symbols.concat (symbol_lookup_inherited (matching.data, name, prefix_match));
			} else if (inner is Vala.MethodCall) {
				var inner_inv = (Vala.MethodCall) inner;
				var inner_ma = inner_inv.call as Vala.MemberAccess;
				if (inner_ma != null) {
					var matching = lookup_symbol (inner_ma.inner, inner_ma.member_name, false, block);
					if (matching != null)
						matching_symbols.concat (symbol_lookup_inherited (matching.data, name, prefix_match, true));
				}
			}

			return matching_symbols;
		}

		GLib.List<Vala.Symbol> symbol_lookup_inherited (Vala.Symbol? sym,
		                                                string name,
		                                                bool prefix_match,
		                                                bool invocation = false)
		{
			GLib.List<Vala.Symbol> result = null;

			// This may happen if we cannot find all the needed packages
			if (sym == null)
				return result;

			var symbol_table = sym.scope.get_symbol_table ();

			if (symbol_table != null) {
				foreach (string key in symbol_table.get_keys()) {
					if (((prefix_match && key.has_prefix (name)) || key == name)) {
						result.append (symbol_table[key]);
					}
				}
			}

			if (invocation && sym is Vala.Method) {
				var func = (Vala.Method) sym;
				result.concat (symbol_lookup_inherited (func.return_type.data_type, name, prefix_match));
			} else if (sym is Vala.Class) {
				var cl = (Vala.Class) sym;
				foreach (var base_type in cl.get_base_types ()) {
					result.concat (symbol_lookup_inherited (base_type.data_type, name, prefix_match));
				}
			} else if (sym is Vala.Struct) {
				var st = (Vala.Struct) sym;
				result.concat (symbol_lookup_inherited (st.base_type.data_type, name, prefix_match));
			} else if (sym is Vala.Interface) {
				var iface = (Vala.Interface) sym;
				foreach (var prerequisite in iface.get_prerequisites ()) {
					result.concat (symbol_lookup_inherited (prerequisite.data_type, name, prefix_match));
				}
			} else if (sym is Vala.LocalVariable) {
				var variable = (Vala.LocalVariable) sym;
				result.concat (symbol_lookup_inherited (variable.variable_type.data_type, name, prefix_match));
			} else if (sym is Vala.Field) {
				var field = (Vala.Field) sym;
				result.concat (symbol_lookup_inherited (field.variable_type.data_type, name, prefix_match));
			} else if (sym is Vala.Property) {
				var prop = (Vala.Property) sym;
				result.concat (symbol_lookup_inherited (prop.property_type.data_type, name, prefix_match));
			} else if (sym is Vala.Parameter) {
				var fp = (Vala.Parameter) sym;
				result.concat (symbol_lookup_inherited (fp.variable_type.data_type, name, prefix_match));
			}

			return result;
		}

		Vala.Expression construct_member_access (string[] names)
		{
			Vala.Expression expr = null;

			for (var i = 0; names[i] != null; i++) {
				if (names[i] != "") {
					expr = new Vala.MemberAccess (expr, names[i]);
					if (names[i+1] != null && names[i+1].chug ().has_prefix ("(")) {
						expr = new Vala.MethodCall (expr);
						i++;
					}
				}
			}

			return expr;
		}
	}
}

