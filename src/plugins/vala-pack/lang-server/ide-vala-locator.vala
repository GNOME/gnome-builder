/*
 * Copyright 2008 Abderrahim Kitouni
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

/* Finds the innermost block containing the given location */
namespace Ide {
	public class ValaLocator: Vala.CodeVisitor {
		struct Location {
			uint line;
			uint column;

			public Location (uint line, uint column) {
				this.line = line;
				this.column = column;
			}

			public bool inside (Vala.CodeNode node) {
				unowned Vala.SourceReference? src = node.source_reference;
				if (src == null) {
					return false;
				}

				unowned Vala.SourceLocation src_begin = src.begin;
				unowned Vala.SourceLocation src_end = src.end;
				critical ("[%s]: %s", node.type_name, node.to_string ());
				if (line > src_begin.line && line < src_end.line) {
					return true;
				} else if (line == src_begin.line && line == src_end.line) {
					return column >= src_begin.column && column <= src_end.column;
				} else if (line == src_begin.line) {
					return column >= src_begin.column;
				} else if (line == src_end.line) {
					return column <= src_end.column;
				} else {
					return false;
				}
			}
		}

		Location location;
		Vala.CodeNode innermost;

		public Vala.CodeNode? locate (Vala.SourceFile file, uint line, uint column) {
			critical ("~~ START ~~");
			location = Location (line, column);
			innermost = null;
			file.accept_children(this);
			return innermost;
		}

		private bool is_closer_to_location (Vala.CodeNode node) {
			if (innermost == null) {
				return true;
			}

			if (innermost == node) {
				return false;
			}

			unowned Vala.SourceReference? node_src = node.source_reference;
			unowned Vala.SourceReference? innermost_src = innermost.source_reference;
			if (node_src.begin.line > innermost_src.begin.line || node_src.end.line < innermost_src.end.line) {
				return true;
			} else if (node_src.begin.line == innermost_src.begin.line && node_src.begin.column > innermost_src.begin.column) {
				return true;
			} else if (node_src.end.line == innermost_src.end.line && node_src.end.column < innermost_src.end.column) {
				return true;
			}

			return false;
		}

		bool update_location (Vala.CodeNode node, bool assign = true) {
			if (!location.inside (node)) {
				if (node is Vala.Subroutine) {
					unowned Vala.Block body = ((Vala.Subroutine)node).body;
					if (location.inside (body)) {
						visit_block (body);
					}
				}

				return false;
			}
			critical ("[%s] [%u.%u:%u.%u] matches location [%u.%u]", node.type_name, node.source_reference.begin.line, node.source_reference.begin.column, node.source_reference.end.line, node.source_reference.end.column, location.line, location.column);

			if (is_closer_to_location (node)) {
				critical ("Replacing innermost");
				if (assign) {
					innermost = node;
				}

				return true;
			}

			return false;
		}

		public override void visit_assignment (Vala.Assignment a) {
			if (update_location (a, false)) {
				a.accept_children (this);
			}
		}

		public override void visit_block (Vala.Block b) {
			if (update_location (b, false)) {
				b.accept_children (this);
			}
		}

		public override void visit_namespace (Vala.Namespace ns) {
			// This is only the namespace declaration, source_reference is only one line long
			update_location (ns);
			ns.accept_children (this);
		}

		public override void visit_class (Vala.Class cl) {
			// This is only the class declaration, source_reference is only one line long
			update_location (cl);
			cl.accept_children (this);
		}

		public override void visit_data_type (Vala.DataType type) {
			if (update_location (type)) {
				type.accept_children (this);
			}
		}

		public override void visit_struct (Vala.Struct st) {
			if (update_location (st)) {
				st.accept_children (this);
			}
		}

		public override void visit_interface (Vala.Interface iface) {
			if (update_location (iface)) {
				iface.accept_children(this);
			}
		}

		public override void visit_method (Vala.Method m) {
			if (update_location (m)) {
				m.accept_children (this);
			}
		}

		public override void visit_method_call (Vala.MethodCall expr) {
			if (update_location (expr)) {
				expr.accept_children (this);
			}
		}

		public override void visit_member_access (Vala.MemberAccess expr) {
			if (update_location (expr)) {
				expr.accept_children (this);
			}
		}

		public override void visit_creation_method (Vala.CreationMethod m) {
			if (update_location (m)) {
				m.accept_children (this);
			}
		}

		public override void visit_object_creation_expression (Vala.ObjectCreationExpression expr) {
			if (update_location (expr, false)) {
				expr.accept_children (this);
			}
		}

		public override void visit_property (Vala.Property prop) {
			prop.accept_children (this);
		}

		public override void visit_property_accessor (Vala.PropertyAccessor acc) {
			if (update_location (acc)) {
				acc.accept_children (this);
			}
		}

		public override void visit_constructor (Vala.Constructor c) {
			if (update_location (c, false)) {
				c.accept_children (this);
			}
		}

		public override void visit_destructor (Vala.Destructor d) {
			if (update_location (d, false)) {
				d.accept_children (this);
			}
		}

		public override void visit_if_statement (Vala.IfStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_switch_statement (Vala.SwitchStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_switch_section (Vala.SwitchSection section) {
			visit_block (section);
		}

		public override void visit_while_statement (Vala.WhileStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_do_statement (Vala.DoStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_for_statement (Vala.ForStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_foreach_statement (Vala.ForeachStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_try_statement (Vala.TryStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_catch_clause (Vala.CatchClause clause) {
			clause.accept_children (this);
		}

		public override void visit_lock_statement (Vala.LockStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_expression_statement (Vala.ExpressionStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_declaration_statement (Vala.DeclarationStatement stmt) {
			stmt.accept_children (this);
		}

		public override void visit_local_variable (Vala.LocalVariable variable) {
			variable.accept_children (this);
		}

		public override void visit_end_full_expression (Vala.Expression expr) {
			if (expr is Vala.LambdaExpression) {
				if ((expr as Vala.LambdaExpression).method != null)
					visit_method ((expr as Vala.LambdaExpression).method);
			}
			if (expr is Vala.MethodCall) {
				foreach (Vala.Expression e in (expr as Vala.MethodCall).get_argument_list()) {
					visit_expression (e);
				}
			}
		}

		public override void visit_expression (Vala.Expression expr) {
			if (expr is Vala.LambdaExpression) {
				if ((expr as Vala.LambdaExpression).method != null)
					visit_method ((expr as Vala.LambdaExpression).method);
			}
			/*if (expr is Vala.MethodCall) {
				update_location (expr);
				foreach (Vala.Expression e in (expr as Vala.MethodCall).get_argument_list())
					visit_expression (e);

			}*/
			if (expr is Vala.Assignment) {
				(expr as Vala.Assignment).accept_children (this);
			}
		}
	}
}
