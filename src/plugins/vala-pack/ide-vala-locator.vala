/*
 * Copyright © 2008 Abderrahim Kitouni
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
			int line;
			int column;

			public Location (int line, int column) {
				this.line = line;
				this.column = column;
			}

			public bool inside (Vala.SourceReference src) {
				var begin = Location (src.begin.line, src.begin.column);
				var end = Location (src.end.line, src.end.column);

				return begin.before (this) && this.before(end);
			}

			public bool before (Location other) {
				if (line > other.line)
					return false;
				else if (line == other.line && column > other.column)
					return false;
				return true;
			}
		}

		Location location;
		Vala.Symbol innermost;
		Location innermost_begin;
		Location innermost_end;

		public Vala.Symbol? locate (Vala.SourceFile file, int line, int column) {
			location = Location (line, column);
			innermost = null;
			file.accept_children(this);
			return innermost;
		}

		bool update_location (Vala.Symbol s) {
			if (!location.inside (s.source_reference))
				return false;

			var begin = Location (s.source_reference.begin.line, s.source_reference.begin.column);
			var end = Location (s.source_reference.end.line, s.source_reference.end.column);

			if (innermost == null || (innermost_begin.before(begin) && end.before(innermost_end))) {
					innermost = s;
					innermost_begin = begin;
					innermost_end = end;
					return true;
			}

			return false;
		}

		public override void visit_block (Vala.Block b) {
			if (update_location (b))
				b.accept_children(this);
		}

		public override void visit_namespace (Vala.Namespace ns) {
			update_location (ns);
			ns.accept_children(this);
		}
		public override void visit_class (Vala.Class cl) {
			/* the location of a class contains only its declaration, not its content */
			if (update_location (cl))
				return;
			cl.accept_children(this);
		}
		public override void visit_struct (Vala.Struct st) {
			if (update_location (st))
				return;
			st.accept_children(this);
		}
		public override void visit_interface (Vala.Interface iface) {
			if (update_location (iface))
				return;
			iface.accept_children(this);
		}

		public override void visit_method (Vala.Method m) {
			if (update_location (m))
				return;
			m.accept_children(this);
		}
		public override void visit_creation_method (Vala.CreationMethod m) {
			if (update_location (m))
				return;
			m.accept_children(this);
		}
		public override void visit_property (Vala.Property prop) {
			prop.accept_children(this);
		}
		public override void visit_property_accessor (Vala.PropertyAccessor acc) {
			acc.accept_children(this);
		}
		public override void visit_constructor (Vala.Constructor c) {
			c.accept_children(this);
		}
		public override void visit_destructor (Vala.Destructor d) {
			d.accept_children(this);
		}
		public override void visit_if_statement (Vala.IfStatement stmt) {
			stmt.accept_children(this);
		}
		public override void visit_switch_statement (Vala.SwitchStatement stmt) {
			stmt.accept_children(this);
		}
		public override void visit_switch_section (Vala.SwitchSection section) {
			visit_block (section);
		}
		public override void visit_while_statement (Vala.WhileStatement stmt) {
			stmt.accept_children(this);
		}
		public override void visit_do_statement (Vala.DoStatement stmt) {
			stmt.accept_children(this);
		}
		public override void visit_for_statement (Vala.ForStatement stmt) {
			stmt.accept_children(this);
		}
		public override void visit_foreach_statement (Vala.ForeachStatement stmt) {
			stmt.accept_children(this);
		}
		public override void visit_try_statement (Vala.TryStatement stmt) {
			stmt.accept_children(this);
		}
		public override void visit_catch_clause (Vala.CatchClause clause) {
			clause.accept_children(this);
		}
		public override void visit_lock_statement (Vala.LockStatement stmt) {
			stmt.accept_children(this);
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
			if (expr is Vala.LambdaExpression)
				visit_method ((expr as Vala.LambdaExpression).method);
			if (expr is Vala.MethodCall) {
				foreach (Vala.Expression e in (expr as Vala.MethodCall).get_argument_list()) {
					visit_expression (e);
				}
			}
		}
		public override void visit_expression (Vala.Expression expr) {
			if (expr is Vala.LambdaExpression)
				visit_method ((expr as Vala.LambdaExpression).method);
			if (expr is Vala.MethodCall) {
				foreach (Vala.Expression e in (expr as Vala.MethodCall).get_argument_list())
					visit_expression (e);
			}
		}
	}
}
