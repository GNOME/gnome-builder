/* ide-vala-indenter.c
 *
 * Copyright 2022 JCWasmx86 <JCWasmx86@t-online.de>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtksourceview/gtksource.h>
#include <ctype.h>
#include "ide-vala-indenter.h"

struct _IdeValaIndenter
{
	IdeObject parent_instance;
};

static void indenter_interface_init (GtkSourceIndenterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeValaIndenter, ide_vala_indenter, IDE_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_INDENTER, indenter_interface_init))

static void
ide_vala_indenter_init (IdeValaIndenter *self)
{
}

static void
ide_vala_indenter_class_init (IdeValaIndenterClass *klass)
{
}

// Copied from gtksourceindenter.c
static gboolean trigger_on_newline (GtkSourceIndenter *self,
				    GtkSourceView     *view,
				    const GtkTextIter *location,
				    GdkModifierType    state,
				    guint              keyval)
{
	if ((state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_SUPER_MASK)) != 0)
		return FALSE;

	return (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter);
}

static char *
view_indent (GtkSourceView *view)
{
	if (gtk_source_view_get_insert_spaces_instead_of_tabs (view))
		return g_strnfill (gtk_source_view_get_tab_width (view), ' ');
	return g_strdup ("\t");
}

static bool
has_statement_head (const char *str,
		    const char *needle)
{
	const gchar *after_needle = NULL;

	if (!g_str_has_prefix (str, needle))
		return FALSE;

	after_needle = &str[strlen (needle)];

	for (const char *iter = after_needle; *iter; iter = g_utf8_next_char (iter))
	{
		gunichar ch = g_utf8_get_char (iter);
		if (ch == '(')
			return TRUE;
		if (!g_unichar_isspace (ch))
			return FALSE;
	}
	return FALSE;
}

static gboolean
line_is_a_oneline_block (const char *str)
{
	return (has_statement_head (str, "for")
			|| has_statement_head (str, "foreach")
			|| has_statement_head (str, "if")
			|| has_statement_head (str, "while"))
		   && !g_str_has_suffix (str, ";")
		   && !g_str_has_suffix (str, "{");
}

static gboolean
is_abnormal_indent (GtkSourceView *view,
		    const char    *indent)
{
	if (gtk_source_view_get_insert_spaces_instead_of_tabs (view))
	{
		if (strchr (indent, '\t'))
			return TRUE;

		return !!(strlen (indent) % gtk_source_view_get_tab_width (view));
	}
	return !!strchr (indent, ' ');
}

static char *
extract_indent (const char *str)
{
	int first_nonspace = 0;

	for (const char *iter = str; *iter; iter = g_utf8_next_char (iter))
	{
		gunichar ch = g_utf8_get_char (iter);

		if (ch == '\n' || !g_unichar_isspace (ch))
			break;

		first_nonspace++;
	}

	return g_utf8_substring (str, 0, first_nonspace);
}

static bool
locate_block_comment_start (int         curr_line,
			    GtkTextIter previous_line_iter)
{
	GtkTextIter next_line_iter = {0};
	next_line_iter = previous_line_iter;
	do
	{
		g_autofree char *text = NULL;
		g_autofree char *stripped_text = NULL;

		gtk_text_iter_backward_line (&previous_line_iter);
		text = gtk_text_iter_get_text (&previous_line_iter, &next_line_iter);
		next_line_iter = previous_line_iter;
		stripped_text = g_strstrip (strdup (text));

		if (g_str_has_prefix (stripped_text, "/*"))
			return TRUE;
		curr_line--;
	}
	while (curr_line != -1);
	return FALSE;
}

static int
locate_parenthesis (const char *str)
{
	const char *ptr = NULL;

	ptr = strchr (str, '(');
	return ptr ? (ptr - str) : -1;
}

static void
indent_label (GtkSourceView *view,
	      GtkTextBuffer *buffer,
	      GtkTextIter   *location,
	      const char    *indent)
{
	g_autofree char *additional_indent = NULL;

	additional_indent = view_indent (view);
	gtk_text_buffer_insert (buffer, location, indent, -1);
	gtk_text_buffer_insert (buffer, location, additional_indent, -1);
}

static void
complete_block (GtkSourceView *view,
		GtkTextBuffer *buffer,
		GtkTextIter   *location,
		const char    *new_indent,
		GtkTextIter    old)
{
	g_autofree char *full = NULL;

	if (gtk_source_view_get_insert_spaces_instead_of_tabs (view))
	{
		g_autofree char *spaces = NULL;
		spaces = g_strnfill (gtk_source_view_get_tab_width (view), ' ');
		full = g_strconcat (new_indent, spaces, NULL);
	}
	else
		full = g_strconcat (new_indent, "\t", NULL);

	gtk_text_buffer_insert (buffer, location, full, -1);
}

static void
fix_abnormal_indent (GtkSourceView *view,
		     int            line_no,
		     GtkTextIter   *previous_line_iter,
		     char         **reference_indent)
{
	int backwards = 0;
	int saved_position = 0;
	GtkTextIter old = {0};
	gboolean fixed = FALSE;

	backwards = 2;
	old = *previous_line_iter;
	saved_position = gtk_text_iter_get_offset (previous_line_iter);

	while (line_no > backwards)
	{
		g_autofree char *tmp_str = NULL;
		g_autofree char *tmp_indent = NULL;

		gtk_text_iter_backward_line (previous_line_iter);
		tmp_str = gtk_text_iter_get_text (previous_line_iter, &old);
		old = *previous_line_iter;
		tmp_indent = extract_indent (tmp_str);

		if (!is_abnormal_indent (view, tmp_indent))
		{
			g_free (*reference_indent);
			*reference_indent = strdup (tmp_indent);
			fixed = true;
			break;
		}
		backwards++;
	}
	if (!fixed)
	{
		g_free (*reference_indent);
		*reference_indent = g_strdup ("");
	}
	gtk_text_iter_set_offset (previous_line_iter, saved_position);
}

static gboolean
find_closing_brace (GtkTextBuffer *buffer,
		    int            line_no,
		    GtkTextIter   *previous_line_iter,
		    const char    *reference_indent)
{
	int cnter = 0;
	GtkTextIter old = {0};

	old = *previous_line_iter;

	while (line_no + cnter <= gtk_text_buffer_get_line_count (buffer))
	{
		g_autofree char *str = NULL;
		g_autofree char *str_stripped = NULL;
		g_autofree char *new_indent = NULL;
		if (cnter <= 1)
		{
			gtk_text_iter_forward_line (previous_line_iter);
			old = *previous_line_iter;
			cnter++;
			continue;
		}
		gtk_text_iter_forward_line (previous_line_iter);
		str = gtk_text_iter_get_text (&old, previous_line_iter);
		str_stripped = g_strstrip (g_strdup (str));
		old = *previous_line_iter;
		new_indent = extract_indent (str);
		if (!strcmp (new_indent, reference_indent) && g_str_has_prefix (str_stripped, "}"))
			return TRUE;
		else if (strlen (new_indent) < strlen (reference_indent) || !strcmp (new_indent, reference_indent))
			return FALSE;
		cnter++;
	}
	return FALSE;
}

static void
indent_args (GtkSourceView *view,
	     const char    *previous_line_str,
	     GtkTextBuffer *buffer,
	     GtkTextIter   *location)
{
	g_autofree char *indent_part = NULL;
	g_autofree char *other_part = NULL;
	int paren_index = 0;
	g_autofree char *new_indent = NULL;


	indent_part = extract_indent (previous_line_str);
	other_part = g_utf8_substring (previous_line_str, strlen (indent_part), -1);
	paren_index = locate_parenthesis (other_part);
	new_indent = indent_part;

	if (gtk_source_view_get_insert_spaces_instead_of_tabs (view))
	{
		g_autofree char *nfill = NULL;

		nfill = g_strnfill (paren_index + 1, ' ');
		new_indent = g_strconcat (new_indent, nfill, NULL);
	}
	else
	{
		gint tab_width = 0;
		gint spaces_count = 0;
		gint tab_count = 0;
		g_autofree char *tabs = NULL;
		g_autofree char *spaces = NULL;

		tab_width = gtk_source_view_get_tab_width (view);
		spaces_count = (paren_index + 1) % tab_width;
		tab_count = (paren_index + 1 - spaces_count) / tab_width;
		tabs = g_strnfill (tab_count, '\t');
		spaces = g_strnfill (spaces_count, ' ');
		new_indent = g_strconcat (new_indent, tabs, spaces, NULL);
	}
	gtk_text_buffer_insert (buffer, location, new_indent, -1);
}

static void
vala_indent (GtkSourceIndenter *self,
	     GtkSourceView     *view,
	     GtkTextIter       *location)
{
	int line_no = 0;
	GtkTextIter previous_line_iter = {0};
	GtkTextBuffer *buffer = NULL;
	g_autofree char *previous_line_str = NULL;
	g_autofree char *previous_line_stripped = NULL;
	g_autofree char *indent = NULL;

	g_assert (IDE_IS_VALA_INDENTER (self));
	g_assert (GTK_SOURCE_IS_VIEW (view));
	g_assert_nonnull (location);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	line_no = gtk_text_iter_get_line (location);
	gtk_text_buffer_get_iter_at_line (buffer, &previous_line_iter, line_no - 1);
	previous_line_str = gtk_text_iter_get_text (&previous_line_iter, location);
	previous_line_stripped = g_strstrip (strdup (previous_line_str));
	indent = extract_indent (previous_line_str);

	if (g_str_has_prefix (previous_line_stripped, "//"))
	{
		g_auto(GStrv) strings = NULL;
		g_autofree char *full = NULL;
		strings = g_strsplit (previous_line_stripped, "//", 2);
		full = g_strconcat (indent, "// ", strings[0], NULL);
		gtk_text_buffer_insert (buffer, location, full, -1);
		return;
	}
	else if (g_str_has_prefix (previous_line_stripped, "*") && !g_str_has_prefix (previous_line_stripped, "*/"))
	{
		gboolean found_start = FALSE;

		found_start = locate_block_comment_start (line_no - 2, previous_line_iter);

		if (found_start)
		{
			g_auto(GStrv) strings = NULL;
			g_autofree char *result = NULL;

			strings = g_strsplit (previous_line_str, "*", 2);
			result = g_strconcat (strings[0], "* ", NULL);
			gtk_text_buffer_insert (buffer, location, result, -1);
			return;
		}
	}

	if (g_str_has_prefix (previous_line_stripped, "/*"))
	{
		gtk_text_buffer_insert (buffer, location, previous_line_str, strstr (previous_line_str, "/*") - previous_line_str);
		gtk_text_buffer_insert (buffer, location, " * ", -1);
		return;
	}

	if (g_str_has_prefix (previous_line_stripped, "*/"))
	{
		gtk_text_buffer_insert (buffer, location, previous_line_str, (strstr (previous_line_str, "*/") - previous_line_str) - 1);
		return;
	}

	if (g_str_has_suffix (previous_line_stripped, ","))
	{
		indent_args (view, previous_line_str, buffer, location);
		return;
	}

	if (g_str_has_suffix (previous_line_stripped, "{"))
	{
		gboolean found_closing_brace = FALSE;
		g_autofree char *reference_indent = NULL;
		g_autofree char *indent1 = NULL;
		g_autofree char *full = NULL;
		GtkTextIter old = {0};

		if (line_no >= 2 && !strchr (previous_line_stripped, '(') && strchr (previous_line_stripped, ')'))
		{
			if (g_str_has_suffix (previous_line_stripped, ","))
			{
				int sub = 0;
				int saved_position = 0;

				sub = 2;
				old = previous_line_iter;
				saved_position = gtk_text_iter_get_offset (&previous_line_iter);
				while (sub <= line_no)
				{
					g_autofree char *tmp_str = NULL;
					g_autofree char *new_indent = NULL;

					gtk_text_iter_backward_line (&previous_line_iter);
					tmp_str = gtk_text_iter_get_text (&previous_line_iter, &old);
					old = previous_line_iter;
					sub++;
					new_indent = extract_indent (tmp_str);
					if (strcmp (new_indent, indent))
					{
						complete_block (view, buffer, location, new_indent, old);
						return;
					}
				}
				gtk_text_iter_set_offset (&previous_line_iter, saved_position);
				goto end_block;
			}
		}

		reference_indent = extract_indent (previous_line_str);
		old = previous_line_iter;

		if (is_abnormal_indent (view, reference_indent))
			fix_abnormal_indent (view, line_no, &previous_line_iter, &reference_indent);

		found_closing_brace = find_closing_brace (buffer, line_no, &previous_line_iter, reference_indent);
		indent1 = view_indent (view);

		if (found_closing_brace)
		{
			full = g_strconcat (reference_indent, indent1, NULL);
			gtk_text_buffer_insert (buffer, location, full, -1);
			return;
		}

		full = g_strconcat (reference_indent, indent1, NULL);
		gtk_text_buffer_insert (buffer, location, full, -1);
		return;
	}
	else if (line_is_a_oneline_block (previous_line_stripped))
	{
		g_autofree char *additional_indent = NULL;

		additional_indent = view_indent (view);
		gtk_text_buffer_insert (buffer, location, indent, -1);
		gtk_text_buffer_insert (buffer, location, additional_indent, -1);
		return;
	}

end_block:
	if (!g_str_has_prefix (previous_line_stripped, "{"))
	{
		GtkTextIter prev_prev_iter = {0};
		g_autofree char *prev_prev_str = NULL;
		g_autofree char *prev_prev_stripped = NULL;

		gtk_text_buffer_get_iter_at_line (buffer, &prev_prev_iter, line_no - 2);
		prev_prev_str = gtk_text_iter_get_text (&prev_prev_iter, &previous_line_iter);
		prev_prev_stripped = g_strstrip (strdup (prev_prev_str));

		if (line_is_a_oneline_block (prev_prev_stripped) && *previous_line_stripped && !g_str_has_prefix (prev_prev_stripped, "{"))
		{
			g_autofree char *previous_indent = NULL;
			previous_indent = extract_indent (prev_prev_str);
			gtk_text_buffer_insert (buffer, location, previous_indent, -1);
			return;
		}
	}

	if (strstr (previous_line_stripped, "default:")
		|| (g_str_has_suffix (previous_line_stripped, ":")
		    && strstr (previous_line_stripped, "case ")))
	{
		indent_label (view, buffer, location, indent);
		return;
	}

	if (g_str_has_suffix (previous_line_stripped, ";"))
	{
		int saved_offset = 0;
		int tmp_lineno = 0;
		int normal_differing_length = 0;
		g_autofree char *last_normal_indent = NULL;

		saved_offset = gtk_text_iter_get_offset (location);
		last_normal_indent = extract_indent (previous_line_str);
		gtk_text_iter_set_line_offset (location, 0);
		tmp_lineno = line_no;
		normal_differing_length = gtk_source_view_get_insert_spaces_instead_of_tabs (view) ? gtk_source_view_get_tab_width (view) : 1;

		while (tmp_lineno)
		{
			g_autofree char *content = NULL;
			g_autofree char *tmp_indent = NULL;
			GtkTextIter tmp_iter = {0};

			tmp_iter = *location;
			gtk_text_iter_backward_line (location);
			content = gtk_text_iter_get_text (location, &tmp_iter);
			tmp_indent = extract_indent (content);

			if (strlen (tmp_indent) + normal_differing_length == strlen (last_normal_indent))
			{
				g_autofree char *additional_indent = NULL;
				additional_indent = view_indent (view);
				gtk_text_iter_set_offset (location, saved_offset);
				gtk_text_buffer_insert (buffer, location, tmp_indent, -1);
				gtk_text_buffer_insert (buffer, location, additional_indent, -1);
				return;
			}
			tmp_lineno--;
		}
		gtk_text_iter_set_offset (location, saved_offset);
	}

	if (!gtk_source_view_get_insert_spaces_instead_of_tabs (view))
	{
		gsize idx = 0;
		gboolean disable = FALSE;

		idx = strlen (indent) - 1;
		disable = !!g_str_has_suffix (previous_line_stripped, ";");
		while (strlen (indent) && idx && indent[idx] == ' ' && disable)
			idx--;
		gtk_text_buffer_insert (buffer, location, indent, idx + 1);
		return;
	}

	/* We can modify indent here, as it will never be modified after these lines */
	if (g_str_has_suffix (previous_line_stripped, ";"))
	{
		gsize n = 0;

		n = strlen (indent);
		indent[n - (n % gtk_source_view_get_tab_width (view))] = '\0';
	}
	gtk_text_buffer_insert (buffer, location, indent, -1);
}

static void
indenter_interface_init (GtkSourceIndenterInterface *iface)
{
	iface->is_trigger = trigger_on_newline;
	iface->indent = vala_indent;
}


