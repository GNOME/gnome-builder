/* gb-source-auto-indenter.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "gb-source-auto-indenter.h"

G_DEFINE_ABSTRACT_TYPE (GbSourceAutoIndenter, gb_source_auto_indenter,
                        G_TYPE_OBJECT)

static gchar *
gb_source_auto_indenter_real_query (GbSourceAutoIndenter *indenter,
                                    GtkTextView          *view,
                                    GtkTextBuffer        *buffer,
                                    GtkTextIter          *iter)
{
  return NULL;
}

gchar *
gb_source_auto_indenter_query (GbSourceAutoIndenter *indenter,
                               GtkTextView          *view,
                               GtkTextBuffer        *buffer,
                               GtkTextIter          *iter)
{
  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER (indenter), NULL);
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (view), NULL);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (iter, NULL);

  return GB_SOURCE_AUTO_INDENTER_GET_CLASS (indenter)->query (indenter, view,
                                                              buffer, iter);
}

static gboolean
gb_source_auto_indenter_real_is_trigger (GbSourceAutoIndenter *indenter,
                                         GdkEventKey          *event)
{
  return FALSE;
}

gboolean
gb_source_auto_indenter_is_trigger (GbSourceAutoIndenter *indenter,
                                    GdkEventKey          *event)
{
  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER (indenter), FALSE);
  g_return_val_if_fail (event, FALSE);

  return GB_SOURCE_AUTO_INDENTER_GET_CLASS (indenter)->is_trigger (indenter,
                                                                   event);
}

gchar *
gb_source_auto_indenter_format (GbSourceAutoIndenter *indenter,
                                GtkTextView          *view,
                                GtkTextBuffer        *buffer,
                                GtkTextIter          *begin,
                                GtkTextIter          *end,
                                gint                 *cursor_offset,
                                GdkEventKey          *event)
{
  gint dummy;

  g_return_val_if_fail (GB_IS_SOURCE_AUTO_INDENTER (indenter), NULL);
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (view), NULL);
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);
  g_return_val_if_fail (event, NULL);

  if (cursor_offset)
    *cursor_offset = 0;
  else
    cursor_offset = &dummy;

  return GB_SOURCE_AUTO_INDENTER_GET_CLASS (indenter)->format (indenter,
                                                               view,
                                                               buffer,
                                                               begin,
                                                               end,
                                                               cursor_offset,
                                                               event);
}

gchar *
gb_source_auto_indenter_real_format (GbSourceAutoIndenter *indenter,
                                     GtkTextView          *view,
                                     GtkTextBuffer        *buffer,
                                     GtkTextIter          *begin,
                                     GtkTextIter          *end,
                                     gint                 *cursor_offset,
                                     GdkEventKey          *event)
{
  return NULL;
}

static void
gb_source_auto_indenter_class_init (GbSourceAutoIndenterClass *klass)
{
  klass->query = gb_source_auto_indenter_real_query;
  klass->is_trigger = gb_source_auto_indenter_real_is_trigger;
  klass->format = gb_source_auto_indenter_real_format;
}

static void
gb_source_auto_indenter_init (GbSourceAutoIndenter *indenter)
{
}
