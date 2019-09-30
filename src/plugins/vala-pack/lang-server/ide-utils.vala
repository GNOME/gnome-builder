namespace Ide {
	public static Ide.Symbol? vala_to_ide_symbol (Vala.CodeNode node, bool simple_name = false)
	{
		if (node is Vala.Block) {
			return null;
		}

		critical ("%s", node.type_name);
		Vala.Symbol? symbol = vala_symbol_from_code_node (node);

		Ide.SymbolKind kind = vala_symbol_kind_from_code_node (node);
		Ide.SymbolFlags flags = vala_symbol_flags_from_code_node (node);
		string? name;
		if (symbol != null) {
			if (!simple_name) {
				name = vala_symbol_name (symbol);
			} else {
				if (symbol is Vala.CreationMethod) {
					name = ((Vala.CreationMethod) symbol).class_name;
				} else {
					name = symbol.name;
				}
			}
		} else {
			name = node.to_string ();
		}

		unowned Vala.SourceReference? source_reference = null;
		if (node is Vala.MethodCall) {
			source_reference = ((Vala.MethodCall) node).call.symbol_reference.source_reference;
		} else if (node is Vala.DataType) {
#if VALA_0_48
			source_reference = ((Vala.DataType) node).symbol.source_reference;
#else
			source_reference = ((Vala.DataType) node).data_type.source_reference;
#endif
		} else if (node is Vala.MemberAccess) {
			weak Vala.Symbol symbol_ref = ((Vala.MemberAccess) node).symbol_reference;
			if (symbol_ref != null) {
				source_reference = symbol_ref.source_reference;
			} else {
				source_reference = node.source_reference;
			}
		} else {
			source_reference = node.source_reference;
		}

		if (source_reference != null) {
			var file = GLib.File.new_for_path (source_reference.file.filename);
			var loc = new Ide.Location (file,
			                            source_reference.begin.line - 1,
			                            source_reference.begin.column - 1);

			return new Ide.Symbol (name, kind, flags, loc, null);
		}

		return new Ide.Symbol (name, kind, flags, null, null);
	}

	public static Ide.SymbolKind vala_symbol_kind_from_code_node (Vala.CodeNode node)
	{
		if (node is Vala.Class)
			return Ide.SymbolKind.CLASS;
		else if (node is Vala.Subroutine) {
			Vala.Symbol? symbol = vala_symbol_from_code_node (node);
			if (symbol != null && symbol.is_instance_member ())
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
		else if (node is Vala.MethodCall) return Ide.SymbolKind.METHOD;

		return Ide.SymbolKind.NONE;
	}

	public static string? vala_symbol_name (Vala.Symbol symbol)
	{
		if (symbol is Vala.Variable) {
			var variable = (symbol as Vala.Variable);
			if (variable.variable_type != null) {
				return variable.variable_type.to_prototype_string () + " " + symbol.name;
			} else {
				return "var " + symbol.name;
			}
		} else if (symbol is Vala.Property) {
			return symbol.name;
		} else if (symbol is Vala.CreationMethod) {
			return ((Vala.CreationMethod) symbol).class_name;
		} else if (symbol is Vala.Method) {
			var type = new Vala.MethodType ((Vala.Method) symbol);
			return type.to_prototype_string (null);
		}

		return symbol.to_string ();
	}

	public static Ide.SymbolFlags vala_symbol_flags_from_code_node (Vala.CodeNode node)
	{
		Vala.Symbol? symbol = vala_symbol_from_code_node (node);
		Ide.SymbolFlags flags = Ide.SymbolFlags.NONE;
		if (symbol != null && symbol.is_instance_member ())
			flags |= Ide.SymbolFlags.IS_MEMBER;

		Vala.MemberBinding? binding = get_member_binding (node);
		if (binding != null && binding == Vala.MemberBinding.STATIC)
			flags |= Ide.SymbolFlags.IS_STATIC;

		if (symbol != null && symbol.version.deprecated)
			flags |= Ide.SymbolFlags.IS_DEPRECATED;

		return flags;
	}

	public static Vala.Symbol? vala_symbol_from_code_node (Vala.CodeNode node)
	{
		if (node is Vala.Expression)
			return ((Vala.Expression) node).symbol_reference;
		else if (node is Vala.MethodCall)
			return ((Vala.MethodCall) node).call.symbol_reference;
		else if (node is Vala.MemberAccess)
			return ((Vala.MemberAccess) node).symbol_reference;
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
