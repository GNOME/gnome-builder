namespace Ide {
	public static Ide.Symbol? vala_to_ide_symbol (Vala.CodeNode node)
	{
		Vala.Symbol? symbol = vala_symbol_from_code_node (node);
		Ide.SymbolKind kind = vala_symbol_kind_from_code_node (node);
		Ide.SymbolFlags flags = vala_symbol_flags_from_code_node (node);
		string name = vala_symbol_name (symbol);

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

	public static Ide.SymbolKind vala_symbol_kind_from_code_node (Vala.CodeNode node)
	{
		if (node is Vala.Class)
			return Ide.SymbolKind.CLASS;
		else if (node is Vala.Subroutine) {
			Vala.Symbol? symbol = vala_symbol_from_code_node (node);
			if (symbol.is_instance_member ())
				if (node is Vala.CreationMethod || node is Vala.Constructor) {
					return Ide.SymbolKind.CONSTRUCTOR;
				} else {
					return Ide.SymbolKind.METHOD;
				}
			else
				return Ide.SymbolKind.FUNCTION;
		}
		else if (node is Vala.Struct) return Ide.SymbolKind.STRUCT;
		else if (node is Vala.Field) return Ide.SymbolKind.FIELD;
		else if (node is Vala.Property) return Ide.SymbolKind.PROPERTY;
		else if (node is Vala.Enum) return Ide.SymbolKind.ENUM;
		else if (node is Vala.EnumValue) return Ide.SymbolKind.ENUM_VALUE;
		else if (node is Vala.Variable) return Ide.SymbolKind.VARIABLE;
		else if (node is Vala.Namespace) return Ide.SymbolKind.NAMESPACE;
		else if (node is Vala.Delegate) return Ide.SymbolKind.TEMPLATE;
		else if (node is Vala.Signal) return Ide.SymbolKind.UI_SIGNAL;

		return Ide.SymbolKind.NONE;
	}

	public static unowned string vala_symbol_name (Vala.Symbol symbol)
	{
		unowned string name = symbol.name;
		if (symbol is Vala.CreationMethod) {
			name = (symbol as Vala.CreationMethod).class_name;
		}

		if (name == null) {
			critical ("HERE");
			critical ("%s (%s)", symbol.type_name, symbol.get_full_name ());
			critical (symbol.to_string ());
			critical ("~~~~~~~~~~");
		}

		return name;
	}

	public static Ide.SymbolFlags vala_symbol_flags_from_code_node (Vala.CodeNode node)
	{
		Vala.Symbol? symbol = vala_symbol_from_code_node (node);
		var flags = Ide.SymbolFlags.NONE;
		if (symbol.is_instance_member ())
			flags |= Ide.SymbolFlags.IS_MEMBER;

		var binding = get_member_binding (node);
		if (binding != null && binding == Vala.MemberBinding.STATIC)
			flags |= Ide.SymbolFlags.IS_STATIC;

		if (symbol.version.deprecated)
			flags |= Ide.SymbolFlags.IS_DEPRECATED;

		return flags;
	}

	public static Vala.Symbol? vala_symbol_from_code_node (Vala.CodeNode node)
	{
		if (node is Vala.Expression)
			return (node as Vala.Expression).symbol_reference;
		else
			return (node as Vala.Symbol);
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
