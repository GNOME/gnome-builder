/* gbp-comment-code-editor-page-addin.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libide-editor.h>

#include "gbp-comment-code-editor-page-addin.h"

#define I_(s) g_intern_static_string(s)

struct _GbpCommentCodeEditorPageAddin
{
  GObject        parent_instance;

  IdeEditorPage *editor_view;
};

static void editor_view_addin_iface_init (IdeEditorPageAddinInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpCommentCodeEditorPageAddin, gbp_comment_code_editor_page_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_PAGE_ADDIN, editor_view_addin_iface_init))

/* If there's only empty lines, G_MAXINT is returned */
static gint
get_buffer_range_min_indent (GtkTextBuffer *buffer,
                             gint           start_line,
                             gint           end_line)
{
  GtkTextIter iter;
  gint current_indent;
  gint min_indent = G_MAXINT;

  for (gint line = start_line; line <= end_line; ++line)
    {
      current_indent = 0;
      gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
      while (!gtk_text_iter_ends_line (&iter) && g_unichar_isspace (gtk_text_iter_get_char (&iter)))
        {
          gtk_text_iter_forward_char (&iter);
          ++current_indent;
        }

      if (gtk_text_iter_ends_line (&iter))
        continue;
      else
        min_indent = MIN (min_indent, current_indent);
    }

  return min_indent;
}

/* Empty lines, with only spaces and tabs or already commented from the start
 * are returned as not commentables.
 */
static gboolean
is_line_commentable (GtkTextBuffer *buffer,
                     gint           line,
                     const gchar   *start_tag)
{
  GtkTextIter iter;

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
  if (gtk_text_iter_is_end (&iter))
    return FALSE;

  while (g_unichar_isspace (gtk_text_iter_get_char (&iter)))
    {
      if (gtk_text_iter_ends_line (&iter) ||
          !gtk_text_iter_forward_char (&iter))
        return FALSE;
    }

  if (ide_text_iter_find_chars_forward (&iter, NULL, NULL, start_tag, TRUE))
    return FALSE;

  return TRUE;
}

/* Empty lines, with only spaces and tabs or not commented from the start
 * are returned as not uncommentables.
 * If TRUE, the start_tag_begin and start_tag_end are updated respectively
 * to the start_tag begin and end positions.
 */
static gboolean
is_line_uncommentable (GtkTextBuffer *buffer,
                       gint           line,
                       const gchar   *start_tag,
                       GtkTextIter   *start_tag_begin,
                       GtkTextIter   *start_tag_end)
{
  GtkTextIter iter;

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
  if (gtk_text_iter_is_end (&iter))
    return FALSE;

  while (g_unichar_isspace (gtk_text_iter_get_char (&iter)))
    {
      if (gtk_text_iter_ends_line (&iter) ||
          !gtk_text_iter_forward_char (&iter))
        return FALSE;
    }

  if (ide_text_iter_find_chars_forward (&iter, NULL, start_tag_end, start_tag, TRUE))
    {
      *start_tag_begin = iter;
      return TRUE;
    }

  return FALSE;
}

/* start_offset, in chars, is where we insert the start_tag.
 * Empty lines or containing only spaces or tabs are skipped.
 */
static void
gbp_comment_code_editor_page_addin_comment_line (GtkTextBuffer *buffer,
                                                 const gchar   *start_tag,
                                                 const gchar   *end_tag,
                                                 gint           line,
                                                 gint           start_offset,
                                                 gboolean       is_block_tag)
{
  g_autofree gchar *start_tag_str = NULL;
  g_autofree gchar *end_tag_str = NULL;
  GtkTextIter start;
  GtkTextIter previous;
  GtkTextIter end_of_line;
  gboolean res;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (!dzl_str_empty0 (start_tag));
  g_assert ((is_block_tag && !dzl_str_empty0 (end_tag)) || !is_block_tag);
  g_assert (line >= 0 && line < gtk_text_buffer_get_line_count(buffer));

  if (!is_line_commentable (buffer, line, start_tag))
    return;

  gtk_text_buffer_get_iter_at_line_offset (buffer, &start, line, start_offset);
  if (gtk_text_iter_ends_line (&start))
    return;

  start_tag_str = g_strconcat (start_tag, " ", NULL);
  gtk_text_buffer_insert (buffer, &start, start_tag_str, -1);
  if (!is_block_tag)
    return;

  end_of_line = start;
  gtk_text_iter_forward_to_line_end (&end_of_line);

  while ((res = ide_text_iter_find_chars_forward (&start, &end_of_line, NULL, start_tag, FALSE)))
    {
      previous = start;
      gtk_text_iter_backward_char (&previous);
      if (gtk_text_iter_get_char (&previous) != '\\')
        break;

      gtk_text_iter_forward_char (&start);
    }

  if (!res)
    {
      start = end_of_line;
      end_tag_str = g_strconcat (" ", end_tag, NULL);
    }
  else
    end_tag_str = g_strconcat (" ", end_tag, " ", NULL);

  gtk_text_buffer_insert (buffer, &start, end_tag_str, -1);
}

static void
gbp_comment_code_editor_page_addin_uncomment_line (GtkTextBuffer *buffer,
                                                   const gchar   *start_tag,
                                                   const gchar   *end_tag,
                                                   gint           line,
                                                   gboolean       is_block_tag)
{
  GtkTextIter end_of_line;
  GtkTextIter tag_begin;
  GtkTextIter tag_end;
  GtkTextIter tmp_iter;
  GtkTextIter previous;
  gunichar ch;
  gboolean res;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (!dzl_str_empty0 (start_tag));
  g_assert ((is_block_tag && !dzl_str_empty0 (end_tag)) || !is_block_tag);
  g_assert (line >= 0 && line < gtk_text_buffer_get_line_count(buffer));

  if (!is_line_uncommentable (buffer, line, start_tag, &tag_begin, &tag_end))
    return;

  gtk_text_buffer_delete (buffer, &tag_begin, &tag_end);
  ch = gtk_text_iter_get_char (&tag_begin);
  if (ch == ' ' || ch == '\t')
    {
      gtk_text_iter_forward_char (&tag_end);
      gtk_text_buffer_delete (buffer, &tag_begin, &tag_end);
    }

  if (!is_block_tag)
    return;

  end_of_line = tag_begin;
  gtk_text_iter_forward_to_line_end (&end_of_line);
  while ((res = ide_text_iter_find_chars_forward (&tag_begin, &end_of_line, &tag_end, end_tag, FALSE)))
    {
      previous = tag_begin;
      gtk_text_iter_backward_char (&previous);
      if (gtk_text_iter_get_char (&previous) != '\\')
        break;

      gtk_text_iter_forward_char (&tag_begin);
    }

  if (res)
    {
      tmp_iter = tag_begin;
      gtk_text_iter_backward_char (&tmp_iter);
      ch = gtk_text_iter_get_char (&tmp_iter);
      if (ch == ' ' || ch == '\t')
        tag_begin = tmp_iter;

      tmp_iter = tag_end;
      if (!gtk_text_iter_ends_line (&tmp_iter))
        {
          gtk_text_iter_forward_char (&tmp_iter);
          ch = gtk_text_iter_get_char (&tmp_iter);
          if (ch == ' ' || ch == '\t')
            {
              tag_end = tmp_iter;
              gtk_text_iter_forward_char (&tag_end);
            }
        }

      gtk_text_buffer_delete (buffer, &tag_begin, &tag_end);
    }
}

static void
gbp_comment_code_editor_page_addin_comment_action (GSimpleAction *action,
                                                   GVariant      *variant,
                                                   gpointer       user_data)
{
  GbpCommentCodeEditorPageAddin *self = GBP_COMMENT_CODE_EDITOR_PAGE_ADDIN (user_data);
  IdeEditorPage *editor_view = self->editor_view;
  IdeSourceView *source_view;
  GtkTextBuffer *buffer;
  const gchar *param;
  IdeCompletion *completion;
  GtkSourceLanguage *lang;
  const gchar *start_tag;
  const gchar *end_tag = NULL;
  gint start_line;
  gint end_line;
  gint indent;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean editable;
  gboolean block_comment = TRUE;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  buffer = GTK_TEXT_BUFFER (ide_editor_page_get_buffer (editor_view));
  source_view = ide_editor_page_get_view (editor_view);
  if (source_view == NULL || !GTK_SOURCE_IS_VIEW (source_view))
    return;

  editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (source_view));
  completion = ide_source_view_get_completion (IDE_SOURCE_VIEW (source_view));
  lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  if (!editable || lang == NULL)
    return;

  if (dzl_str_equal0 (gtk_source_language_get_id(lang), "c"))
    {
      start_tag = gtk_source_language_get_metadata (lang, "block-comment-start");
      end_tag = gtk_source_language_get_metadata (lang, "block-comment-end");
      if (start_tag == NULL || end_tag == NULL)
        {
          block_comment = FALSE;
          start_tag = gtk_source_language_get_metadata (lang, "line-comment-start");
          if (start_tag == NULL)
            return;
        }
    }
  else
    {
      start_tag = gtk_source_language_get_metadata (lang, "line-comment-start");
      if (start_tag == NULL)
        {
          start_tag = gtk_source_language_get_metadata (lang, "block-comment-start");
          end_tag = gtk_source_language_get_metadata (lang, "block-comment-end");
          if (start_tag == NULL || end_tag == NULL)
            return;
        }
      else
        block_comment = FALSE;
    }

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  if (!gtk_text_iter_equal (&begin, &end) &&
      gtk_text_iter_starts_line (&end))
    gtk_text_iter_backward_char (&end);

  start_line = gtk_text_iter_get_line (&begin);
  end_line = gtk_text_iter_get_line (&end);

  param = g_variant_get_string (variant, NULL);

  if (*param == '0')
    {
      indent = get_buffer_range_min_indent (buffer, start_line, end_line);
     if (indent == G_MAXINT)
       return;

      ide_completion_block_interactive (completion);
      gtk_text_buffer_begin_user_action (buffer);

      for (gint line = start_line; line <= end_line; ++line)
        gbp_comment_code_editor_page_addin_comment_line (buffer, start_tag, end_tag, line, indent, block_comment);

      gtk_text_buffer_end_user_action (buffer);
      ide_completion_unblock_interactive (completion);
    }
  else if (*param == '1')
    {
      ide_completion_block_interactive (completion);
      gtk_text_buffer_begin_user_action (buffer);

      for (gint line = start_line; line <= end_line; ++line)
        gbp_comment_code_editor_page_addin_uncomment_line (buffer, start_tag, end_tag, line, block_comment);

      gtk_text_buffer_end_user_action (buffer);
      ide_completion_unblock_interactive (completion);
    }
  else
    g_assert_not_reached ();
}

static const DzlShortcutEntry comment_code_shortcut_entries[] = {
  { "org.gnome.builder.editor-page.comment-code",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Editing"),
    N_("Comment the code") },

  { "org.gnome.builder.editor-page.uncomment-code",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Editing"),
    N_("Uncomment the code") },
};

static const GActionEntry actions[] = {
  { "comment-code", gbp_comment_code_editor_page_addin_comment_action, "s" },
};

static void
gbp_comment_code_editor_page_addin_load (IdeEditorPageAddin *addin,
                                         IdeEditorPage      *view)
{
  GbpCommentCodeEditorPageAddin *self;
  g_autoptr(GSimpleActionGroup) group = NULL;
  DzlShortcutController *controller;

  g_assert (GBP_IS_COMMENT_CODE_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (view));

  self = GBP_COMMENT_CODE_EDITOR_PAGE_ADDIN (addin);
  self->editor_view = view;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (view), "comment-code", G_ACTION_GROUP (group));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (view));
  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.editor-page.comment-code",
                                              I_("<primary>m"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "comment-code.comment-code::0");

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.editor-page.uncomment-code",
                                              I_("<primary><shift>m"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "comment-code.comment-code::1");

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             comment_code_shortcut_entries,
                                             G_N_ELEMENTS (comment_code_shortcut_entries),
                                             GETTEXT_PACKAGE);
}

static void
gbp_comment_code_editor_page_addin_unload (IdeEditorPageAddin *addin,
                                           IdeEditorPage      *view)
{
  g_assert (GBP_IS_COMMENT_CODE_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (view));

  gtk_widget_insert_action_group (GTK_WIDGET (view), "comment-code", NULL);
}

static void
gbp_comment_code_editor_page_addin_class_init (GbpCommentCodeEditorPageAddinClass *klass)
{
}

static void
gbp_comment_code_editor_page_addin_init (GbpCommentCodeEditorPageAddin *self)
{
}

static void
editor_view_addin_iface_init (IdeEditorPageAddinInterface *iface)
{
  iface->load = gbp_comment_code_editor_page_addin_load;
  iface->unload = gbp_comment_code_editor_page_addin_unload;
}
