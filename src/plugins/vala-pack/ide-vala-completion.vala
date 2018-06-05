/*
 * Some of the code below is taken from Anjuta and is licensed under the
 * terms of the GPL v2. The original copyright is preserved.
 *
 * Copyright 2008-2010 Abderrahim Kitouni
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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
	public class ValaCompletion : GLib.Object
	{
		Vala.CodeContext context;
		Vala.SourceLocation location;
		Vala.Block? nearest;

		public ValaCompletion (Vala.CodeContext context,
		                       Vala.SourceLocation location,
		                       Vala.Block? nearest)
		{
			this.context = context;
			this.location = location;
			this.nearest = nearest;
		}

		public void run (Ide.ValaCompletionResults ret, ref Vala.SourceLocation start_pos)
		{
			start_pos.line = this.location.line;
			start_pos.column = this.location.column;

			if (nearest != null) {
				for (var sym = (Vala.Symbol) nearest; sym != null; sym = sym.parent_symbol) {
					symbol_lookup_inherited (ret, sym);
				}

				foreach (var ns in nearest.source_reference.file.current_using_directives) {
					symbol_lookup_inherited (ret, ns.namespace_symbol);
				}
			}
		}

#if 0
		GLib.List<Vala.Symbol> lookup_symbol (Vala.Expression? inner, Vala.Block? block)
		{
			var matching_symbols = new GLib.List<Vala.Symbol> ();

			if (block == null)
				return matching_symbols;

			if (inner == null) {
				for (var sym = (Vala.Symbol) block; sym != null; sym = sym.parent_symbol) {
					matching_symbols.concat (symbol_lookup_inherited (sym));
				}

				foreach (var ns in block.source_reference.file.current_using_directives) {
					matching_symbols.concat (symbol_lookup_inherited (ns.namespace_symbol));
				}
			} else if (inner.symbol_reference != null) {
					matching_symbols.concat (symbol_lookup_inherited (inner.symbol_reference));
			} else if (inner is Vala.MemberAccess) {
				var inner_ma = (Vala.MemberAccess) inner;
				var matching = lookup_symbol (inner_ma.inner, inner_ma.member_name, false, block);
				if (matching != null)
					matching_symbols.concat (symbol_lookup_inherited (matching.data));
			} else if (inner is Vala.MethodCall) {
				var inner_inv = (Vala.MethodCall) inner;
				var inner_ma = inner_inv.call as Vala.MemberAccess;
				if (inner_ma != null) {
					var matching = lookup_symbol (inner_ma.inner, inner_ma.member_name, false, block);
					if (matching != null)
						matching_symbols.concat (symbol_lookup_inherited (matching.data, true));
				}
			}

			return matching_symbols;
		}
#endif

		void symbol_lookup_inherited (Ide.ValaCompletionResults results,
		                              Vala.Symbol? sym,
		                              bool invocation = false)
		{
			// This may happen if we cannot find all the needed packages
			if (sym == null)
				return;

			var symbol_table = sym.scope.get_symbol_table ();

			if (symbol_table != null) {
				foreach (string key in symbol_table.get_keys()) {
					results.add (symbol_table[key]);
				}
			}

			if (invocation && sym is Vala.Method) {
				var func = (Vala.Method) sym;
				symbol_lookup_inherited (results, func.return_type.data_type);
			} else if (sym is Vala.Class) {
				var cl = (Vala.Class) sym;
				foreach (var base_type in cl.get_base_types ()) {
					symbol_lookup_inherited (results, base_type.data_type);
				}
			} else if (sym is Vala.Struct) {
				var st = (Vala.Struct) sym;
				symbol_lookup_inherited (results, st.base_type.data_type);
			} else if (sym is Vala.Interface) {
				var iface = (Vala.Interface) sym;
				foreach (var prerequisite in iface.get_prerequisites ()) {
					symbol_lookup_inherited (results, prerequisite.data_type);
				}
			} else if (sym is Vala.LocalVariable) {
				var variable = (Vala.LocalVariable) sym;
				symbol_lookup_inherited (results, variable.variable_type.data_type);
			} else if (sym is Vala.Field) {
				var field = (Vala.Field) sym;
				symbol_lookup_inherited (results, field.variable_type.data_type);
			} else if (sym is Vala.Property) {
				var prop = (Vala.Property) sym;
				symbol_lookup_inherited (results, prop.property_type.data_type);
			} else if (sym is Vala.Parameter) {
				var fp = (Vala.Parameter) sym;
				symbol_lookup_inherited (results, fp.variable_type.data_type);
			}
		}
	}
}

