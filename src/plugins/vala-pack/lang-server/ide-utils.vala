namespace Ide {
	public static Ide.Symbol? vala_to_ide_symbol (Vala.CodeNode node)
	{
		Vala.Symbol? symbol = null;
		if (node is Vala.Expression) {
			warning ("HERE");
			symbol = (node as Vala.Expression).symbol_reference;
		} else
			symbol = (node as Vala.Symbol);

		var kind = Ide.SymbolKind.NONE;
		if (node is Vala.Class)
			kind = Ide.SymbolKind.CLASS;
		else if (node is Vala.Subroutine) {
			if (symbol.is_instance_member ())
				kind = Ide.SymbolKind.METHOD;
			else
				kind = Ide.SymbolKind.FUNCTION;
		}

		else if (node is Vala.Struct) kind = Ide.SymbolKind.STRUCT;
		else if (node is Vala.Field) kind = Ide.SymbolKind.FIELD;
		else if (node is Vala.Enum) kind = Ide.SymbolKind.ENUM;
		else if (node is Vala.EnumValue) kind = Ide.SymbolKind.ENUM_VALUE;
		else if (node is Vala.Variable) kind = Ide.SymbolKind.VARIABLE;
		else if (node is Vala.Namespace) kind = Ide.SymbolKind.NAMESPACE;

		var flags = Ide.SymbolFlags.NONE;
		if (symbol.is_instance_member ())
			flags |= Ide.SymbolFlags.IS_MEMBER;

		var binding = get_member_binding (node);
		if (binding != null && binding == Vala.MemberBinding.STATIC)
			flags |= Ide.SymbolFlags.IS_STATIC;

		if (symbol.version.deprecated)
			flags |= Ide.SymbolFlags.IS_DEPRECATED;

		foreach (var attr in symbol.attributes) {
			critical (attr.name);
		}
		var name = symbol.name;
		/*if (name == null) {
			name = symbol.get_full_name ();
		}*/

		var source_reference = node.source_reference;
		if (source_reference != null) {
			var file = new Ide.File (null, GLib.File.new_for_path (source_reference.file.filename));
			var loc = new Ide.SourceLocation (file,
											  source_reference.begin.line - 1,
											  source_reference.begin.column - 1,
											  0);

			return new Ide.Symbol (name, kind, flags, loc, loc, loc);
		}

		return new Ide.Symbol (name, kind, flags, null, null, null);
	}

	// a member binding is Instance, Class, or Static
	public static Vala.MemberBinding? get_member_binding (Vala.CodeNode sym)
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
}
