/* ide-cursor.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-cursor"

#include "config.h"

#include <dazzle.h>

#include "ide-source-view.h"
#include "ide-cursor.h"
#include "ide-text-util.h"

struct _IdeCursor
{
  GObject                      parent_instance;

  IdeSourceView               *source_view;
  GtkSourceSearchContext      *search_context;

  GList                       *cursors;

  GtkTextTag                  *highlight_tag;

  DzlSignalGroup              *operations_signals;

  guint                        overwrite : 1;
};

typedef struct
{
  GtkTextMark *selection_bound;
  GtkTextMark *insert;
} VirtualCursor;

G_DEFINE_TYPE (IdeCursor, ide_cursor, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_IDE_SOURCE_VIEW,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void ide_cursor_set_visible        (IdeCursor     *self,
                                           GtkTextBuffer *buffer,
                                           gboolean       visible);

static void ide_cursor_set_real_cursor    (IdeCursor     *self,
                                           GtkTextBuffer *buffer,
                                           VirtualCursor *vc);

static void ide_cursor_set_virtual_cursor (IdeCursor     *self,
                                           GtkTextBuffer *buffer,
                                           VirtualCursor *vc);

static void
ide_cursor_dispose (GObject *object)
{
  IdeCursor *self = (IdeCursor *)object;
  GtkTextBuffer *buffer = NULL;

  g_return_if_fail (IDE_IS_CURSOR (self));

  if (self->source_view != NULL)
    {
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));
      if (self->highlight_tag != NULL)
        gtk_text_tag_table_remove (gtk_text_buffer_get_tag_table (buffer),
                                   self->highlight_tag);
      g_clear_weak_pointer (&self->source_view);
    }

  if (self->operations_signals != NULL)
    {
      dzl_signal_group_set_target (self->operations_signals, NULL);
      g_clear_object (&self->operations_signals);
    }

  g_clear_object (&self->highlight_tag);
  g_clear_object (&self->search_context);

  if (buffer != NULL && self->cursors != NULL)
    {
      for (const GList *iter = self->cursors; iter != NULL; iter = iter->next)
        {
          VirtualCursor *vc;

          vc = iter->data;

          if (buffer != NULL)
            {
              gtk_text_buffer_delete_mark (buffer, vc->insert);
              gtk_text_buffer_delete_mark (buffer, vc->selection_bound);
            }

          g_slice_free (VirtualCursor, vc);
        }

    }

  g_clear_pointer (&self->cursors, g_list_free);

  G_OBJECT_CLASS (ide_cursor_parent_class)->dispose (object);
}


/* toggles the visibility of cursors */
static void
ide_cursor_set_visible (IdeCursor      *self,
                        GtkTextBuffer  *buffer,
                        gboolean        visible)
{
  g_assert (IDE_IS_CURSOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (self->cursors != NULL)
    {
      for (const GList *iter = self->cursors; iter != NULL; iter = iter->next)
        {
          VirtualCursor *vc;
          GtkTextIter selection_bound, insert;

          vc = iter->data;
          gtk_text_buffer_get_iter_at_mark (buffer, &selection_bound, vc->selection_bound);
          gtk_text_buffer_get_iter_at_mark (buffer, &insert, vc->insert);

          if (gtk_text_iter_equal (&insert, &selection_bound))
            {
              if (self->overwrite)
                {
                  gtk_text_iter_forward_char (&selection_bound);
                }
              else
                {
                  gtk_text_mark_set_visible (vc->insert, visible);
                  continue;
                }
            }

          if (visible)
            gtk_text_buffer_apply_tag (buffer, self->highlight_tag, &selection_bound, &insert);
          else
            gtk_text_buffer_remove_tag (buffer, self->highlight_tag, &selection_bound, &insert);
        }
    }
}

/* sets real cursor at virtual cursor position */
static void
ide_cursor_set_real_cursor (IdeCursor     *self,
                            GtkTextBuffer *buffer,
                            VirtualCursor *vc)
{
  GtkTextIter selection_bound, insert;

  g_assert (IDE_IS_CURSOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_get_iter_at_mark (buffer, &selection_bound, vc->selection_bound);
  gtk_text_buffer_get_iter_at_mark (buffer, &insert, vc->insert);

  gtk_text_buffer_select_range (buffer, &insert, &selection_bound);
}

/* sets virutal cursor at actual cursor position */
static void
ide_cursor_set_virtual_cursor (IdeCursor     *self,
                               GtkTextBuffer *buffer,
                               VirtualCursor *vc)
{
  GtkTextIter selection_bound, insert;

  g_assert (IDE_IS_CURSOR (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_get_iter_at_mark (buffer, &insert, gtk_text_buffer_get_insert (buffer));
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_bound, gtk_text_buffer_get_selection_bound (buffer));

  gtk_text_buffer_move_mark (buffer, vc->selection_bound, &selection_bound);
  gtk_text_buffer_move_mark (buffer, vc->insert, &insert);
}

void
ide_cursor_remove_cursors (IdeCursor *self)
{
  g_return_if_fail (IDE_IS_CURSOR (self));

  if (self->cursors != NULL)
    {
      GtkTextBuffer *buffer;
      VirtualCursor *vc;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

      ide_cursor_set_visible (self, buffer, FALSE);

      for (const GList *iter = self->cursors; iter != NULL; iter = iter->next)
        {
          vc = iter->data;

          gtk_text_buffer_delete_mark (buffer, vc->insert);
          gtk_text_buffer_delete_mark (buffer, vc->selection_bound);

          g_slice_free (VirtualCursor, vc);
        }

      g_clear_pointer (&self->cursors, g_list_free);
    }
}

static void
ide_cursor_add_cursor_by_column (IdeCursor *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin, end, temp;
  gint begin_line, begin_offset, end_line, end_offset, offset;
  GtkTextIter iter;

  g_assert (IDE_IS_CURSOR (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

  if (!gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    return;

  gtk_text_buffer_get_iter_at_mark (buffer, &temp, gtk_text_buffer_get_insert (buffer));
  offset = gtk_text_iter_get_line_offset (&temp);

  begin_line = gtk_text_iter_get_line (&begin);
  begin_offset = gtk_text_iter_get_line_offset (&begin);
  end_line = gtk_text_iter_get_line (&end);
  end_offset = gtk_text_iter_get_line_offset (&end);

  if (begin_line == end_line)
    return;

  for (int i = begin_line; i <= end_line; i++)
    {
      VirtualCursor *vc;

      if ((i == begin_line && offset < begin_offset) ||
          (i == end_line && offset > end_offset))
        continue;

      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, i, offset);

      vc = g_slice_new0 (VirtualCursor);
      vc->selection_bound = gtk_text_buffer_create_mark (buffer, NULL, &iter, FALSE);

      vc->insert = gtk_text_buffer_create_mark (buffer, NULL, &iter, FALSE);
      self->cursors = g_list_prepend (self->cursors, vc);

      if (self->overwrite)
        {
          GtkTextIter iter1 = iter;

          gtk_text_iter_forward_char (&iter1);
          gtk_text_buffer_apply_tag (buffer, self->highlight_tag, &iter, &iter1);
        }
      else
        {
          gtk_text_mark_set_visible (vc->insert, TRUE);
        }
    }

  gtk_text_buffer_select_range (buffer, &iter, &iter);
}

static void
ide_cursor_add_cursor_by_position (IdeCursor *self)
{
  GtkTextIter insert_iter, selection_bound_iter;
  VirtualCursor *vc;
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_CURSOR (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

  gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, gtk_text_buffer_get_insert (buffer));
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_bound_iter, gtk_text_buffer_get_selection_bound (buffer));

  vc = g_slice_new0 (VirtualCursor);
  vc->selection_bound = gtk_text_buffer_create_mark (buffer, NULL, &insert_iter, FALSE);
  vc->insert = gtk_text_buffer_create_mark (buffer, NULL, &selection_bound_iter, FALSE);
  self->cursors = g_list_prepend (self->cursors, vc);

  if (gtk_text_iter_equal (&insert_iter, &selection_bound_iter))
    {
      if (self->overwrite)
        {
          gtk_text_iter_forward_char (&selection_bound_iter);
          gtk_text_buffer_apply_tag (buffer, self->highlight_tag, &insert_iter, &selection_bound_iter);
        }
      else
        {
          gtk_text_mark_set_visible (vc->insert, TRUE);
        }
    }
  else
    {
      gtk_text_buffer_apply_tag (buffer, self->highlight_tag, &insert_iter, &selection_bound_iter);
    }
}

static void
ide_cursor_add_cursor_by_match (IdeCursor *self)
{
  g_autofree gchar *text = NULL;
  GtkTextIter begin, end, match_begin, match_end;
  gboolean has_wrapped_around = FALSE;
  VirtualCursor *vc;
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_CURSOR (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

  if (!gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    return;
  gtk_text_iter_order (&begin, &end);

  text = gtk_text_buffer_get_text (buffer, &begin, &end, FALSE);

  search_context = self->search_context;
  search_settings = gtk_source_search_context_get_settings (search_context);

  if (g_strcmp0 (gtk_source_search_settings_get_search_text (search_settings), text) != 0)
    gtk_source_search_settings_set_search_text (search_settings, text);

  if (!gtk_source_search_context_forward (search_context, &end,
                                          &match_begin, &match_end, &has_wrapped_around))
    return;

  if (self->cursors == NULL)
    {
      vc = g_slice_new0 (VirtualCursor);
      vc->selection_bound = gtk_text_buffer_create_mark (buffer, NULL, &begin, FALSE);
      vc->insert = gtk_text_buffer_create_mark (buffer, NULL, &end, FALSE);

      self->cursors = g_list_prepend (self->cursors, vc);

      gtk_text_buffer_apply_tag (buffer, self->highlight_tag, &begin, &end);
    }

  vc = g_slice_new0 (VirtualCursor);
  vc->selection_bound = gtk_text_buffer_create_mark (buffer, NULL, &match_begin, FALSE);
  vc->insert = gtk_text_buffer_create_mark (buffer, NULL, &match_end, FALSE);

  self->cursors = g_list_prepend (self->cursors, vc);

  gtk_text_buffer_apply_tag (buffer, self->highlight_tag, &match_begin, &match_end);

  gtk_text_buffer_select_range (buffer, &match_begin, &match_end);

  ide_source_view_scroll_mark_onscreen (self->source_view, vc->insert, TRUE, 0.5, 0.5);
}

void
ide_cursor_add_cursor (IdeCursor      *self,
                       IdeCursorType   type)
{
  g_return_if_fail (IDE_IS_CURSOR (self));
  g_return_if_fail (type<=IDE_CURSOR_MATCH);

  if (type == IDE_CURSOR_COLUMN)
    ide_cursor_add_cursor_by_column (self);
  else if (type == IDE_CURSOR_SELECT)
    ide_cursor_add_cursor_by_position (self);
  else if (type == IDE_CURSOR_MATCH)
    ide_cursor_add_cursor_by_match (self);
}

void
ide_cursor_insert_text (IdeCursor *self,
                        gchar     *text,
                        gint       len)
{
  g_return_if_fail (IDE_IS_CURSOR (self));

  if (self->cursors != NULL)
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

      ide_cursor_set_visible (self, buffer, FALSE);
      ide_cursor_set_virtual_cursor (self, buffer, self->cursors->data);

      for (const GList *iter = self->cursors->next; iter != NULL; iter = iter->next)
        {
          VirtualCursor *vc = iter->data;
          GtkTextIter begin, end;

          gtk_text_buffer_get_iter_at_mark (buffer, &begin, vc->insert);
          gtk_text_buffer_get_iter_at_mark (buffer, &end, vc->selection_bound);

          if (gtk_text_iter_equal (&begin, &end))
            {
              if (self->overwrite)
                {
                  gtk_text_iter_forward_char (&end);
                  gtk_text_buffer_delete (buffer, &begin, &end);
                  gtk_text_buffer_get_iter_at_mark (buffer, &end, vc->insert);
                }
              gtk_text_buffer_insert (buffer, &end, text, len);
            }
          else
            {
              gtk_text_buffer_delete (buffer, &begin, &end);
              gtk_text_buffer_get_iter_at_mark (buffer, &end, vc->insert);
              gtk_text_buffer_insert (buffer, &end, text, len);
            }
        }

      ide_cursor_set_visible (self, buffer, TRUE);
    }
}

static void
ide_cursor_backspace (GtkTextView *text_view,
                      IdeCursor   *self)
{
  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_CURSOR (self));

  if (self->cursors != NULL)
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (text_view);

      ide_cursor_set_visible (self, buffer, FALSE);
      ide_cursor_set_virtual_cursor (self, buffer, self->cursors->data);

      gtk_text_buffer_begin_user_action (buffer);

      for (const GList *iter = self->cursors->next; iter != NULL; iter = iter->next)
        {
          VirtualCursor *vc = iter->data;
          GtkTextIter begin, end;

          gtk_text_buffer_get_iter_at_mark (buffer, &begin, vc->selection_bound);
          gtk_text_buffer_get_iter_at_mark (buffer, &end, vc->insert);

          if (gtk_text_iter_equal (&begin, &end))
            gtk_text_buffer_backspace (buffer, &end, TRUE, gtk_text_view_get_editable (text_view));
          else
            gtk_text_buffer_delete (buffer, &begin, &end);
        }

      gtk_text_buffer_end_user_action (buffer);

      ide_cursor_set_visible (self, buffer, TRUE);
    }
}

static void
ide_cursor_delete_from_cursor (GtkTextView    *text_view,
                               GtkDeleteType   delete_type,
                               gint            count,
                               IdeCursor      *self)
{
  g_assert (IDE_IS_SOURCE_VIEW (text_view));
  g_assert (IDE_IS_CURSOR (self));

  if (self->cursors != NULL)
    {
      GtkTextBuffer *buffer;
      GtkTextIter ins;
      GtkTextMark *mark;

      buffer = gtk_text_view_get_buffer (text_view);

      gtk_text_buffer_get_iter_at_mark (buffer, &ins, gtk_text_buffer_get_insert(buffer));
      mark = gtk_text_buffer_create_mark (buffer, NULL, &ins, FALSE);

      ide_cursor_set_visible (self, buffer, FALSE);
      ide_cursor_set_virtual_cursor (self, buffer, self->cursors->data);

      gtk_text_buffer_begin_user_action (buffer);

      for (const GList *iter = self->cursors->next; iter != NULL; iter = iter->next)
        {
          VirtualCursor *vc = iter->data;
          GtkTextIter begin, end;

          gtk_text_buffer_get_iter_at_mark (buffer, &begin, vc->selection_bound);
          gtk_text_buffer_get_iter_at_mark (buffer, &end, vc->insert);

          ide_cursor_set_real_cursor (self, buffer, vc);

          if (delete_type == GTK_DELETE_PARAGRAPHS)
            ide_text_util_delete_line (text_view, count);
          else
            GTK_TEXT_VIEW_GET_CLASS (text_view)->delete_from_cursor (text_view,
                                                                     delete_type,
                                                                     count);
          ide_cursor_set_virtual_cursor (self, buffer, vc);
        }

      gtk_text_buffer_end_user_action (buffer);

      ide_cursor_set_visible (self, buffer, TRUE);
      gtk_text_buffer_get_iter_at_mark (buffer, &ins, mark);
      gtk_text_buffer_select_range (buffer, &ins, &ins);
    }
}

static void
ide_cursor_delete_selection (IdeSourceView *source_view,
                             IdeCursor     *self)
{
  GtkTextBuffer *buffer;
  gboolean editable;

  g_assert (IDE_IS_SOURCE_VIEW (source_view));
  g_assert (IDE_IS_CURSOR (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (source_view));

  if (!editable)
    return;

  if (self->cursors != NULL)
    {
      ide_cursor_set_visible (self, buffer, FALSE);
      ide_cursor_set_virtual_cursor (self, buffer, self->cursors->data);

      gtk_text_buffer_begin_user_action (buffer);

      for (const GList *iter = self->cursors->next; iter != NULL; iter = iter->next)
        {
          VirtualCursor *vc = iter->data;
          GtkTextIter begin, end;

          gtk_text_buffer_get_iter_at_mark (buffer, &begin, vc->selection_bound);
          gtk_text_buffer_get_iter_at_mark (buffer, &end, vc->insert);

          gtk_text_iter_order (&begin, &end);

          if (gtk_text_iter_is_end (&end) && gtk_text_iter_starts_line (&begin))
            gtk_text_iter_backward_char (&begin);

          gtk_text_buffer_delete (buffer, &begin, &end);
        }

      gtk_text_buffer_end_user_action (buffer);
      ide_cursor_set_visible (self, buffer, TRUE);
    }
}

static void
ide_cursor_move_cursor (GtkTextView    *text_view,
                        GtkMovementStep step,
                        gint            count,
                        gboolean        extend_selection,
                        IdeCursor      *self)
{
  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_CURSOR (self));

  if (self->cursors != NULL)
    {
      GtkTextBuffer *buffer;
      GtkTextIter sel, ins;

      buffer = gtk_text_view_get_buffer (text_view);

      gtk_text_buffer_get_iter_at_mark (buffer, &ins, gtk_text_buffer_get_insert (buffer));
      gtk_text_buffer_get_iter_at_mark (buffer, &sel, gtk_text_buffer_get_selection_bound (buffer));

      ide_cursor_set_visible (self, buffer, FALSE);
      ide_cursor_set_virtual_cursor (self, buffer, self->cursors->data);

      for (const GList *iter = self->cursors->next; iter != NULL; iter = iter->next)
        {
          ide_cursor_set_real_cursor (self, buffer, iter->data);

          GTK_TEXT_VIEW_GET_CLASS (text_view)->move_cursor (text_view,
                                                            step,
                                                            count,
                                                            extend_selection);

          ide_cursor_set_virtual_cursor (self, buffer, iter->data);
        }

      ide_cursor_set_visible (self, buffer, TRUE);
      gtk_text_buffer_select_range (buffer, &ins, &sel);

      gtk_text_view_scroll_mark_onscreen (text_view, gtk_text_buffer_get_insert (buffer));
    }
}

static void
ide_cursor_movement (IdeSourceView         *source_view,
                     IdeSourceViewMovement  movement,
                     gboolean               extend_selection,
                     gboolean               exclusive,
                     gboolean               apply_count,
                     IdeCursor             *self)
{
  g_assert (IDE_IS_SOURCE_VIEW (source_view));
  g_assert (IDE_IS_CURSOR (self));

  if (self->cursors)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
      GtkTextIter sel,ins;

      gtk_text_buffer_get_iter_at_mark (buffer, &sel, gtk_text_buffer_get_selection_bound(buffer));
      gtk_text_buffer_get_iter_at_mark (buffer, &ins, gtk_text_buffer_get_insert(buffer));

      ide_cursor_set_visible (self, buffer, FALSE);
      ide_cursor_set_virtual_cursor (self, buffer, self->cursors->data);

      for (const GList *iter = self->cursors->next; iter != NULL; iter = iter->next)
        {
          ide_cursor_set_real_cursor (self, buffer, iter->data);

          IDE_SOURCE_VIEW_GET_CLASS (source_view)->movement (source_view,
                                                             movement,
                                                             extend_selection,
                                                             exclusive,
                                                             apply_count);

          ide_cursor_set_virtual_cursor (self, buffer, iter->data);
        }
      ide_cursor_set_visible (self, buffer, TRUE);
      gtk_text_buffer_select_range (buffer, &ins, &sel);
    }
}

static void
ide_cursor_select_inner (IdeSourceView *source_view,
                         const gchar   *inner_left,
                         const gchar   *inner_right,
                         gboolean       exclusive,
                         gboolean       string_mode,
                         IdeCursor     *self)
{
  g_assert (IDE_IS_SOURCE_VIEW (source_view));
  g_assert (IDE_IS_CURSOR (self));

  if (self->cursors != NULL)
    {
      GtkTextBuffer *buffer;
      GtkTextIter sel,ins;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));

      gtk_text_buffer_get_iter_at_mark (buffer, &sel, gtk_text_buffer_get_selection_bound (buffer));
      gtk_text_buffer_get_iter_at_mark (buffer, &ins, gtk_text_buffer_get_insert (buffer));

      ide_cursor_set_visible (self, buffer, FALSE);
      ide_cursor_set_virtual_cursor (self, buffer, self->cursors->data);

      for (const GList *iter = self->cursors->next; iter != NULL; iter = iter->next)
        {
          ide_cursor_set_real_cursor (self, buffer, iter->data);

          IDE_SOURCE_VIEW_GET_CLASS (source_view)->select_inner (source_view,
                                                                 inner_left,
                                                                 inner_right,
                                                                 exclusive,
                                                                 string_mode);

          ide_cursor_set_virtual_cursor (self, buffer, iter->data);
        }

      ide_cursor_set_visible (self, buffer, TRUE);
      gtk_text_buffer_select_range (buffer, &ins, &sel);
    }
}

static void
ide_cursor_toggle_overwrite (GtkTextView *text_view,
                             IdeCursor   *self)
{
  GtkTextBuffer *buffer;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_CURSOR (self));

  buffer = gtk_text_view_get_buffer (text_view);

  ide_cursor_set_visible (self, buffer, FALSE);
  self->overwrite = gtk_text_view_get_overwrite (text_view);
  ide_cursor_set_visible (self, buffer, TRUE);
}

static void
ide_cursor_constructed (GObject *object)
{
  IdeCursor *self = (IdeCursor *)object;
  GtkTextView *text_view;
  GtkTextBuffer *buffer;
  g_autoptr(GtkSourceSearchSettings) search_settings = NULL;

  G_OBJECT_CLASS (ide_cursor_parent_class)->constructed (object);

  text_view = GTK_TEXT_VIEW (self->source_view);

  buffer = gtk_text_view_get_buffer (text_view);

  search_settings = g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                                  "wrap-around", FALSE,
                                  "regex-enabled", FALSE,
                                  "case-sensitive", TRUE,
                                  NULL);
  self->search_context = g_object_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                                       "buffer", buffer,
                                       "highlight", FALSE,
                                       "settings", search_settings,
                                       NULL);

  gtk_text_tag_table_add (gtk_text_buffer_get_tag_table (buffer),
                          self->highlight_tag);

  self->overwrite = gtk_text_view_get_overwrite (text_view);

  dzl_signal_group_set_target (self->operations_signals, self->source_view);
}

static void
ide_cursor_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeCursor *self = IDE_CURSOR (object);

  switch (prop_id)
    {
    case PROP_IDE_SOURCE_VIEW:
      g_value_set_object (value, self->source_view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_cursor_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeCursor *self = IDE_CURSOR (object);

  switch (prop_id)
    {
    case PROP_IDE_SOURCE_VIEW:
      g_set_weak_pointer (&self->source_view, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_cursor_class_init (IdeCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_cursor_constructed;
  object_class->dispose = ide_cursor_dispose;
  object_class->get_property = ide_cursor_get_property;
  object_class->set_property = ide_cursor_set_property;

  properties [PROP_IDE_SOURCE_VIEW] =
    g_param_spec_object ("ide-source-view",
                         "IdeSourceView",
                         "The IdeSourceView on which cursors are there",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_cursor_init (IdeCursor *self)
{
  self->highlight_tag = g_object_new (GTK_TYPE_TEXT_TAG,
                                      "underline", PANGO_UNDERLINE_SINGLE,
                                      NULL);

  self->operations_signals = dzl_signal_group_new (IDE_TYPE_SOURCE_VIEW);

  dzl_signal_group_connect_object (self->operations_signals,
                                   "move-cursor",
                                   G_CALLBACK (ide_cursor_move_cursor),
                                   self,
                                   G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->operations_signals,
                                   "delete-from-cursor",
                                   G_CALLBACK (ide_cursor_delete_from_cursor),
                                   self,
                                   G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->operations_signals,
                                   "backspace",
                                   G_CALLBACK (ide_cursor_backspace),
                                   self,
                                   G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->operations_signals,
                                   "toggle-overwrite",
                                   G_CALLBACK (ide_cursor_toggle_overwrite),
                                   self,
                                   G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->operations_signals,
                                   "movement",
                                   G_CALLBACK (ide_cursor_movement),
                                   self,
                                   G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->operations_signals,
                                   "select-inner",
                                   G_CALLBACK (ide_cursor_select_inner),
                                   self,
                                   G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->operations_signals,
                                   "delete-selection",
                                   G_CALLBACK (ide_cursor_delete_selection),
                                   self,
                                   G_CONNECT_AFTER);
}

gboolean
ide_cursor_is_enabled (IdeCursor *self)
{
  g_return_val_if_fail (IDE_IS_CURSOR (self), FALSE);

  return (self->cursors != NULL);
}
