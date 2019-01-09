/* ide-indenter.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-indenter"

#include "config.h"

#include <libide-code.h>

#include "ide-indenter.h"

G_DEFINE_INTERFACE (IdeIndenter, ide_indenter, IDE_TYPE_OBJECT)

static gchar *
ide_indenter_default_format (IdeIndenter *self,
                             GtkTextView *text_view,
                             GtkTextIter *begin,
                             GtkTextIter *end,
                             gint        *cursor_offset,
                             GdkEventKey *event)
{
  return NULL;
}

static gboolean
ide_indenter_default_is_trigger (IdeIndenter *self,
                                 GdkEventKey *event)
{
  return FALSE;
}

static void
ide_indenter_default_init (IdeIndenterInterface *iface)
{
  iface->format = ide_indenter_default_format;
  iface->is_trigger = ide_indenter_default_is_trigger;
}

static gchar *
ide_indenter_mimic_source_view (GtkTextView *text_view,
                                GtkTextIter *begin,
                                GtkTextIter *end,
                                gint        *cursor_offset,
                                GdkEventKey *event)
{
  GtkTextIter copy_begin;
  GtkTextIter copy_end;
  gchar *ret;

  IDE_ENTRY;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (cursor_offset != NULL);
  g_assert (event != NULL);

  *cursor_offset = 0;
  *begin = *end;

  if (event->keyval != GDK_KEY_Return &&
      event->keyval != GDK_KEY_KP_Enter)
    IDE_RETURN (NULL);

  copy_begin = *end;

  /* We might already be at the beginning of the file */
  if (!gtk_text_iter_backward_char (&copy_begin))
    IDE_RETURN (NULL);

  gtk_text_iter_set_line_offset (&copy_begin, 0);
  copy_end = copy_begin;
  while (g_unichar_isspace (gtk_text_iter_get_char (&copy_end)))
    {
      if (gtk_text_iter_ends_line (&copy_end) ||
          !gtk_text_iter_forward_char (&copy_end))
        break;
    }

  ret = gtk_text_iter_get_slice (&copy_begin, &copy_end);

  IDE_RETURN (ret);
}

/**
 * ide_indenter_format:
 * @self: (nullable): An #IdeIndenter or %NULL for the fallback
 * @text_view: a #GtkTextView
 * @begin: a #GtkTextIter for the beginning region of text to replace.
 * @end: a #GtkTextIter for the end region of text to replace.
 * @cursor_offset: (out): The offset in characters from @end to place the
 *   cursor. Negative values are okay.
 * @event: The #GdkEventKey that triggered the event.
 *
 * This function performs an indentation for the key press activated by @event.
 * The implementation is free to move the @begin and @end iters to swallow
 * adjacent content. The result, a string, is the contents that will replace
 * the content inbetween @begin and @end.
 *
 * @cursor_offset may be set to jump the cursor starting from @end. Negative
 * values are allowed.
 *
 * If @self is %NULL, the fallback indenter is used, which tries to mimic the
 * indentation style of #GtkSourceView.
 *
 * Returns: (nullable) (transfer full): A string containing the replacement
 *   text, or %NULL.
 *
 * Since: 3.32
 */
gchar *
ide_indenter_format (IdeIndenter *self,
                     GtkTextView *text_view,
                     GtkTextIter *begin,
                     GtkTextIter *end,
                     gint        *cursor_offset,
                     GdkEventKey *event)
{
  g_return_val_if_fail (!self || IDE_IS_INDENTER (self), NULL);
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);
  g_return_val_if_fail (begin != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (cursor_offset != NULL, NULL);
  g_return_val_if_fail (event != NULL, NULL);

  if (self == NULL)
    return ide_indenter_mimic_source_view (text_view, begin, end, cursor_offset, event);
  else
    return IDE_INDENTER_GET_IFACE (self)->format (self, text_view, begin, end, cursor_offset, event);
}

/**
 * ide_indenter_is_trigger:
 * @self: (nullable): an #IdeIndenter or %NULL for the fallback
 * @event: a #GdkEventKey
 *
 * Determines if @event should trigger an indentation request. If %TRUE is
 * returned then ide_indenter_format() will be called.
 *
 * If @self is %NULL, the fallback indenter is used, which tries to mimic
 * the default indentation style of #GtkSourceView.
 *
 * Returns: %TRUE if @event should trigger an indentation request.
 *
 * Since: 3.32
 */
gboolean
ide_indenter_is_trigger (IdeIndenter *self,
                         GdkEventKey *event)
{
  g_return_val_if_fail (!self || IDE_IS_INDENTER (self), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (self == NULL)
    {
      switch (event->keyval)
        {
        case GDK_KEY_KP_Enter:
        case GDK_KEY_Return:
          return TRUE;

        default:
          return FALSE;
        }
    }

  return IDE_INDENTER_GET_IFACE (self)->is_trigger (self, event);
}
