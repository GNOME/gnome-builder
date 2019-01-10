
/* gbp-retab-editor-page-addin.c
 *
 * Copyright 2017 Lucie Charvat <luci.charvat@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-retab-editor-page-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libide-editor.h>

#include "gbp-retab-editor-page-addin.h"

struct _GbpRetabEditorPageAddin
{
  GObject        parent_instance;
  IdeEditorPage *editor_view;
};

static gint
get_buffer_range_indent (GtkTextBuffer *buffer,
                         gint           line,
                         gboolean       to_spaces)
{
  GtkTextIter iter;
  gint indent = 0;

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

  while (!gtk_text_iter_ends_line (&iter) && g_unichar_isspace(gtk_text_iter_get_char (&iter)))
    {
      gtk_text_iter_forward_char (&iter);
      ++indent;
    }

  return indent;
}

/* Removes indent that is mean to be changes and inserts
 * tabs and/or spaces insted */
static void
gbp_retab_editor_page_addin_retab (GtkTextBuffer *buffer,
                            gint           line,
                            gint           tab_width,
                            gint           indent,
                            gboolean       to_spaces)
{
  g_autoptr(GString) new_indent = g_string_new (NULL);
  GtkTextIter iter;
  GtkTextIter begin;
  GtkTextIter end;
  gint tab_num = 0;
  gint space_num = 0;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (line >= 0 && line < gtk_text_buffer_get_line_count(buffer));
  g_assert (tab_width != 0);
  g_assert (new_indent != NULL);

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

  while (!gtk_text_iter_ends_line (&iter) &&
         g_unichar_isspace(gtk_text_iter_get_char (&iter)))
    {
      if (gtk_text_iter_get_char (&iter) == ' ')
        ++space_num;
      else if (gtk_text_iter_get_char (&iter) == '\t')
        ++tab_num;

      gtk_text_iter_forward_char (&iter);
    }

  if (to_spaces)
    {
      for (gint tab = 0; tab < tab_num * tab_width; ++tab)
        g_string_append_c(new_indent, ' ');

      for (gint space = 0; space < space_num; ++space)
        g_string_append_c(new_indent, ' ');
    }
  else
    {
      for (gint tab = 0; tab < tab_num + (space_num / tab_width); ++tab)
        g_string_append_c(new_indent, '\t');

      for (gint space = 0; space < space_num % tab_width; ++space)
        g_string_append_c(new_indent, ' ');
    }

  gtk_text_buffer_get_iter_at_line(buffer, &begin, line);
  gtk_text_buffer_get_iter_at_line_offset (buffer, &end, line, indent);
  gtk_text_buffer_delete (buffer, &begin, &end);

  if (new_indent->len)
    gtk_text_buffer_insert (buffer, &begin, new_indent->str, new_indent->len);
}

static void
gbp_retab_editor_page_addin_action (GSimpleAction *action,
                             GVariant      *variant,
                             gpointer       user_data)
{
  GbpRetabEditorPageAddin *self = user_data;
  IdeSourceView *source_view;
  GtkTextBuffer *buffer;
  IdeCompletion *completion;
  guint tab_width;
  gint start_line;
  gint end_line;
  gint indent;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean editable;
  gboolean to_spaces;

  g_assert (GBP_IS_RETAB_EDITOR_PAGE_ADDIN (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  buffer = GTK_TEXT_BUFFER (ide_editor_page_get_buffer (self->editor_view));
  source_view = ide_editor_page_get_view (self->editor_view);

  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (source_view));
  completion = ide_source_view_get_completion (IDE_SOURCE_VIEW (source_view));
  tab_width = gtk_source_view_get_tab_width(GTK_SOURCE_VIEW (source_view));
  to_spaces = gtk_source_view_get_insert_spaces_instead_of_tabs(GTK_SOURCE_VIEW (source_view));

  if (!editable)
    return;

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  if (!gtk_text_iter_equal (&begin, &end) && gtk_text_iter_starts_line (&end))
    gtk_text_iter_backward_char (&end);

  start_line = gtk_text_iter_get_line (&begin);
  end_line = gtk_text_iter_get_line (&end);

  ide_completion_block_interactive (completion);
  gtk_text_buffer_begin_user_action (buffer);

  for (gint line = start_line; line <= end_line; ++line)
    {
      indent = get_buffer_range_indent (buffer, line, to_spaces);
      if (indent > 0)
        gbp_retab_editor_page_addin_retab (buffer, line, tab_width, indent, to_spaces);
    }

  gtk_text_buffer_end_user_action (buffer);
  ide_completion_unblock_interactive (completion);
}

static const GActionEntry actions[] = {
  { "retab", gbp_retab_editor_page_addin_action },
};

static void
gbp_retab_editor_page_addin_load (IdeEditorPageAddin *addin,
                           IdeEditorPage      *view)
{
  GbpRetabEditorPageAddin *self = (GbpRetabEditorPageAddin *)addin;
  GActionGroup *group;

  g_assert (GBP_IS_RETAB_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (view));

  self->editor_view = view;

  group = gtk_widget_get_action_group (GTK_WIDGET (view), "editor-page");
  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
}

static void
gbp_retab_editor_page_addin_unload (IdeEditorPageAddin *addin,
                             IdeEditorPage      *view)
{
  GActionGroup *group;

  g_assert (GBP_IS_RETAB_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (view));

  group = gtk_widget_get_action_group (GTK_WIDGET (view), "editor-page");
  g_action_map_remove_action (G_ACTION_MAP (group), "retab");
}

static void
editor_view_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_retab_editor_page_addin_load;
  iface->unload = gbp_retab_editor_page_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpRetabEditorPageAddin, gbp_retab_editor_page_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN,
                                                editor_view_addin_iface_init))


static void
gbp_retab_editor_page_addin_class_init (GbpRetabEditorPageAddinClass *klass)
{
}

static void
gbp_retab_editor_page_addin_init (GbpRetabEditorPageAddin *self)
{
}
