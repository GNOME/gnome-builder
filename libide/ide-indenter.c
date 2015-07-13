/* ide-indenter.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-indenter"

#include "ide-context.h"
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

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            IDE_TYPE_CONTEXT,
                                                            (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));
}

/**
 * ide_indenter_format:
 * @text_view: A #GtkTextView
 * @begin: A #GtkTextIter for the beginning region of text to replace.
 * @end: A #GtkTextIter for the end region of text to replace.
 * @cursor_offset: The offset in characters from @end to place the cursor.
 *   Negative values are okay.
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
 * Returns: (transfer full): A string containing the replacement text, or %NULL.
 */
gchar *
ide_indenter_format (IdeIndenter *self,
                     GtkTextView *text_view,
                     GtkTextIter *begin,
                     GtkTextIter *end,
                     gint        *cursor_offset,
                     GdkEventKey *event)
{
  g_return_val_if_fail (IDE_IS_INDENTER (self), NULL);
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);
  g_return_val_if_fail (cursor_offset, NULL);
  g_return_val_if_fail (event, NULL);

  return IDE_INDENTER_GET_IFACE (self)->format (self, text_view, begin, end, cursor_offset, event);
}

/**
 * ide_indenter_is_trigger:
 * @self: an #IdeIndenter
 * @event: a #GdkEventKey
 *
 * Determines if @event should trigger an indentation request. If %TRUE is
 * returned then ide_indenter_format() will be called.
 *
 * Returns: %TRUE if @event should trigger an indentation request.
 */
gboolean
ide_indenter_is_trigger (IdeIndenter *self,
                         GdkEventKey *event)
{
  g_return_val_if_fail (IDE_IS_INDENTER (self), FALSE);
  g_return_val_if_fail (event, FALSE);

  return IDE_INDENTER_GET_IFACE (self)->is_trigger (self, event);
}
