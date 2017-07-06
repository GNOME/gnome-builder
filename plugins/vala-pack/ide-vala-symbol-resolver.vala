/* ide-vala-symbol-resolver.vala
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
using Ide;
using Vala;

namespace Ide
{
	public class ValaSymbolResolver: Ide.Object, Ide.SymbolResolver
	{
		public async Ide.SymbolTree? get_symbol_tree_async (GLib.File file,
		                                                    Ide.Buffer buffer,
		                                                    GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			var context = this.get_context ();
			var service = (Ide.ValaService)context.get_service_typed (typeof (Ide.ValaService));
			var index = service.index;
			var symbol_tree = yield index.get_symbol_tree (file, cancellable);

			return symbol_tree;
		}

		Ide.Symbol? create_symbol (Ide.File file, Vala.Symbol symbol)
		{
			var kind = Ide.SymbolKind.NONE;
			if (symbol is Vala.Class)
				kind = Ide.SymbolKind.CLASS;
			else if (symbol is Vala.Subroutine) {
				if (symbol.is_instance_member ())
					kind = Ide.SymbolKind.METHOD;
				else
					kind = Ide.SymbolKind.FUNCTION;
			}
			else if (symbol is Vala.Struct) kind = Ide.SymbolKind.STRUCT;
			else if (symbol is Vala.Field) kind = Ide.SymbolKind.FIELD;
			else if (symbol is Vala.Enum) kind = Ide.SymbolKind.ENUM;
			else if (symbol is Vala.EnumValue) kind = Ide.SymbolKind.ENUM_VALUE;
			else if (symbol is Vala.Variable) kind = Ide.SymbolKind.VARIABLE;
			else if (symbol is Vala.Namespace) kind = Ide.SymbolKind.NAMESPACE;

			var flags = Ide.SymbolFlags.NONE;
			if (symbol.is_instance_member ())
				flags |= Ide.SymbolFlags.IS_MEMBER;

			var binding = get_member_binding (symbol);
			if (binding != null && binding == Vala.MemberBinding.STATIC)
				flags |= Ide.SymbolFlags.IS_STATIC;

#if ENABLE_VALA_SYMBOL_GET_DEPRECATED
			if (symbol.deprecated)
#else
			if (symbol.version.deprecated)
#endif
				flags |= Ide.SymbolFlags.IS_DEPRECATED;

			var source_reference = symbol.source_reference;

			if (source_reference != null) {
				var loc = new Ide.SourceLocation (file,
												  source_reference.begin.line - 1,
												  source_reference.begin.column - 1,
												  0);
				return new Ide.Symbol (symbol.name, kind, flags, loc, loc, loc);
			}

			return null;
		}

		public async Ide.Symbol? lookup_symbol_async (Ide.SourceLocation location,
		                                              GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			var context = this.get_context ();
			var service = (Ide.ValaService)context.get_service_typed (typeof (Ide.ValaService));
			var index = service.index;
			var file = location.get_file ();
			var line = (int)location.get_line () + 1;
			var column = (int)location.get_line_offset () + 1;

			Vala.Symbol? symbol = yield index.find_symbol_at (file.get_file (), line, column);

			if (symbol != null)
				return create_symbol (file, symbol);

			return null;
		}

		// a member binding is Instance, Class, or Static
		private Vala.MemberBinding? get_member_binding (Vala.Symbol sym)
		{
			if (sym is Vala.Constructor)
				return ((Vala.Constructor)sym).binding;
			if (sym is Vala.Destructor)
				return ((Vala.Destructor)sym).binding;
			if (sym is Vala.Field)
				return ((Vala.Field)sym).binding;
			if (sym is Vala.Method)
				return ((Vala.Method)sym).binding;
			if (sym is Vala.Property)
				return ((Vala.Property)sym).binding;
			return null;
		}

		public void load () {}

		public async GLib.GenericArray<weak Ide.SourceRange> find_references_async (Ide.SourceLocation location,
		                                                                            GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			return new GLib.GenericArray<weak Ide.SourceRange> ();
		}

		public async Ide.Symbol? find_nearest_scope_async (Ide.SourceLocation location,
		                                                   GLib.Cancellable? cancellable)
			throws GLib.Error
		{
			var context = this.get_context ();
			var service = (Ide.ValaService)context.get_service_typed (typeof (Ide.ValaService));
			var index = service.index;
			var file = location.get_file ();
			var line = (int)location.get_line () + 1;
			var column = (int)location.get_line_offset () + 1;

			var symbol = yield index.find_symbol_at (file.get_file (), line, column);

			while (symbol != null) {
				if (symbol is Vala.Class ||
					symbol is Vala.Subroutine ||
					symbol is Vala.Namespace ||
					symbol is Vala.Struct)
					break;

				if (symbol.owner != null)
					symbol = symbol.owner.owner;
				else
					symbol = symbol.parent_symbol;
			}

			if (symbol != null)
				return create_symbol (file, symbol);

			throw new GLib.IOError.NOT_FOUND ("Failed to locate nearest scope");
		}
	}
}

