/* gb-source-change-monitor.c
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

#define G_LOG_DOMAIN "change-monitor"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-source-change-monitor.h"

struct _GbSourceChangeMonitorPrivate
{
  GtkTextBuffer *buffer;
  GArray        *state;

  guint delete_range_before_handler;
  guint delete_range_after_handler;
  guint insert_text_before_handler;
  guint insert_text_after_handler;

  guint insert_begin_line;
  guint insert_begin_offset;
};

enum
{
  PROP_0,
  PROP_BUFFER,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceChangeMonitor,
                            gb_source_change_monitor,
                            G_TYPE_OBJECT)

static GParamSpec *gParamSpecs [LAST_PROP];


static void
gb_source_change_monitor_insert (GbSourceChangeMonitor *monitor,
                                 guint                  line,
                                 GbSourceChangeFlags    flags)
{
  guint8 byte = (guint8)flags;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));

  if (line == monitor->priv->state->len)
    g_array_append_val (monitor->priv->state, byte);
  else
    g_array_insert_val (monitor->priv->state, line, byte);
}

static void
gb_source_change_monitor_remove (GbSourceChangeMonitor *monitor,
                                 guint                  line)
{
  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));
  g_return_if_fail (line < monitor->priv->state->len);

  g_array_remove_index (monitor->priv->state, line);
}

static void
gb_source_change_monitor_set_line (GbSourceChangeMonitor *monitor,
                                   guint                  line,
                                   GbSourceChangeFlags    flags)
{
  GbSourceChangeFlags old_flags;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));
  g_return_if_fail (line < monitor->priv->state->len);

  old_flags = g_array_index (monitor->priv->state, guint8, line);

  /*
   * Don't allow ourselves to go from "added" to "changed" unless we
   * previously did not have a dirty bit.
   */
  if (((old_flags & GB_SOURCE_CHANGE_ADDED) != 0) &&
      ((old_flags & GB_SOURCE_CHANGE_DIRTY) != 0))
    {
      flags &= ~GB_SOURCE_CHANGE_CHANGED;
      flags |= GB_SOURCE_CHANGE_ADDED;
    }

  g_array_index (monitor->priv->state, guint8, line) = (guint8)flags;
}

static void
gb_source_change_monitor_ensure_bounds (GbSourceChangeMonitor *monitor)
{
  GbSourceChangeMonitorPrivate *priv;
  GtkTextIter begin;
  GtkTextIter end;
  guint line;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));

  priv = monitor->priv;

  gtk_text_buffer_get_bounds (priv->buffer, &begin, &end);

  line = gtk_text_iter_get_line (&end);

  if (line + 1 > priv->state->len)
    g_array_set_size (priv->state, line + 1);
}

static void
get_line_mutation (const GtkTextIter *begin,
                   const GtkTextIter *end,
                   guint              line,
                   gboolean          *delete_line,
                   gboolean          *is_changed)
{
  guint begin_line;
  guint begin_offset;
  guint end_line;
  guint end_offset;

  ENTRY;

  begin_line = gtk_text_iter_get_line (begin);
  begin_offset = gtk_text_iter_get_line_offset (begin);
  end_line = gtk_text_iter_get_line (end);
  end_offset = gtk_text_iter_get_line_offset (end);

  if (begin_line == end_line)
    {
      *delete_line = FALSE;
      *is_changed = TRUE;
      EXIT;
    }
  else if ((line == begin_line) &&
           gtk_text_iter_starts_line (begin) &&
           gtk_text_iter_starts_line (end) &&
           !gtk_text_iter_ends_line (end))
    {
      *delete_line = TRUE;
      *is_changed = FALSE;
      EXIT;
    }
  else if ((line == begin_line) &&
           ((begin_line + 1) == end_line) &&
           gtk_text_iter_ends_line (begin) &&
           gtk_text_iter_starts_line (end))
    {
      *delete_line = FALSE;
      *is_changed = FALSE;
      EXIT;
    }
  else if ((begin_offset != 0) && (line == begin_line))
    {
      *delete_line = FALSE;
      *is_changed = TRUE;
      EXIT;
    }
  else if ((line == end_line) && (end_offset == 0))
    {
      *delete_line = FALSE;
      *is_changed = FALSE;
      EXIT;
    }
  else if ((begin_offset == 0) && (line == begin_line) && (begin_line != end_line))
    {
      *delete_line = TRUE;
      *is_changed = FALSE;
      EXIT;
    }
  else if ((line != begin_line) && (line != end_line))
    {
      *delete_line = TRUE;
      *is_changed = FALSE;
      EXIT;
    }
  else if ((line != begin_line) && (line == end_line) && (begin_offset != 0))
    {
      *delete_line = TRUE;
      *is_changed = FALSE;
      EXIT;
    }
  else if ((line != begin_line) && (line == end_line) && (begin_offset == 0))
    {
      *delete_line = FALSE;
      *is_changed = TRUE;
      EXIT;
    }
  else
    {
      g_warning ("Unknown outcome: begin=%d:%d line=%d end_line=%d:%d",
                 begin_line, begin_offset, line, end_line, end_offset);

      *is_changed = TRUE;
      *delete_line = FALSE;
      EXIT;
    }

  EXIT;
}

static void
on_delete_range_before_cb (GbSourceChangeMonitor *monitor,
                           GtkTextIter           *begin,
                           GtkTextIter           *end,
                           GtkTextBuffer         *buffer)
{
  GArray *ar;
  guint begin_line;
  guint end_line;
  guint i;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (gtk_text_iter_compare (begin, end) == 0)
    return;

  begin_line = gtk_text_iter_get_line (begin);
  end_line = gtk_text_iter_get_line (end);

  ar = g_array_new (FALSE, FALSE, sizeof (guint));

  for (i = begin_line; i <= end_line; i++)
    {
      gboolean delete_line;
      gboolean is_changed;

      get_line_mutation (begin, end, i, &delete_line, &is_changed);

      if (delete_line)
        g_array_append_val (ar, i);
      else if (is_changed)
        gb_source_change_monitor_set_line (monitor, i,
                                           (GB_SOURCE_CHANGE_CHANGED |
                                            GB_SOURCE_CHANGE_DIRTY));
    }

  for (i = ar->len; i > 0; i--)
    {
      guint line;

      line = g_array_index (ar, guint, i - 1);
      gb_source_change_monitor_remove (monitor, line);
    }

  g_array_unref (ar);
}

static void
on_delete_range_after_cb (GbSourceChangeMonitor *monitor,
                          GtkTextIter           *begin,
                          GtkTextIter           *end,
                          GtkTextBuffer         *buffer)
{
  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
}

static void
on_insert_text_before_cb (GbSourceChangeMonitor *monitor,
                          GtkTextIter           *location,
                          gchar                 *text,
                          gint                   len,
                          GtkTextBuffer         *buffer)
{
  GbSourceChangeMonitorPrivate *priv;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  priv = monitor->priv;

  priv->insert_begin_line = gtk_text_iter_get_line (location);
  priv->insert_begin_offset = gtk_text_iter_get_line_offset (location);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  /*
   * WORKAROUND:
   *
   * Mark the first line as added if we are typing our first character.
   * We should consider removing this GSignal handler after it has
   * hit this to save on signal emission.
   */
  if (G_UNLIKELY (gtk_text_iter_compare (&begin, &end) == 0))
    gb_source_change_monitor_set_line (monitor, 0,
                                       (GB_SOURCE_CHANGE_ADDED |
                                        GB_SOURCE_CHANGE_DIRTY));
}

static void
on_insert_text_after_cb (GbSourceChangeMonitor *monitor,
                         GtkTextIter           *location,
                         gchar                 *text,
                         gint                   len,
                         GtkTextBuffer         *buffer)
{
  GbSourceChangeMonitorPrivate *priv;
  GbSourceChangeFlags flags = 0;
  guint line;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  priv = monitor->priv;

  line = gtk_text_iter_get_line (location);

  if (g_strcmp0 (text, "\n") == 0)
    {
      flags = (GB_SOURCE_CHANGE_ADDED | GB_SOURCE_CHANGE_DIRTY);
      if (priv->insert_begin_offset == 0)
        gb_source_change_monitor_insert (monitor, line-1, flags);
      else
        gb_source_change_monitor_insert (monitor, line, flags);
    }
  else if (strchr (text, '\n') == NULL)
    {
      flags = (GB_SOURCE_CHANGE_CHANGED | GB_SOURCE_CHANGE_DIRTY);
      gb_source_change_monitor_set_line (monitor, line, flags);
    }
  else
    {
      GtkTextIter end;
      GtkTextIter iter;
      guint last_line;

      len = g_utf8_strlen (text, len);

      gtk_text_iter_assign (&iter, location);
      gtk_text_iter_assign (&end, location);
      gtk_text_iter_backward_chars (&iter, len);

      last_line = gtk_text_iter_get_line (&iter);

      while (gtk_text_iter_compare (&iter, &end) <= 0)
        {
          line = gtk_text_iter_get_line (&iter);

          if (line != last_line)
            {
              flags = (GB_SOURCE_CHANGE_ADDED | GB_SOURCE_CHANGE_DIRTY);
              gb_source_change_monitor_insert (monitor, line, flags);
              last_line = line;
            }

          if (!gtk_text_iter_forward_char (&iter))
            break;
        }
    }

  gb_source_change_monitor_ensure_bounds (monitor);
}

GbSourceChangeFlags
gb_source_change_monitor_get_line (GbSourceChangeMonitor *monitor,
                                   guint                  lineno)
{
  g_return_val_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor), 0);

  if (lineno < monitor->priv->state->len)
    return monitor->priv->state->data [lineno];

  g_warning ("No such line: %u", lineno);

  return 0;
}

void
gb_source_change_monitor_saved (GbSourceChangeMonitor *monitor)
{
  GbSourceChangeMonitorPrivate *priv;
  GbSourceChangeFlags flags;
  guint i;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));

  priv = monitor->priv;

  for (i = 0; i < priv->state->len; i++)
    {
      flags = g_array_index (priv->state, guint8, i);
      flags &= ~GB_SOURCE_CHANGE_DIRTY;
      g_array_index (priv->state, guint8, i) = flags;
    }
}

void
gb_source_change_monitor_reset (GbSourceChangeMonitor *monitor)
{
  GbSourceChangeMonitorPrivate *priv;
  GtkTextIter begin;
  GtkTextIter end;
  guint line;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));

  priv = monitor->priv;

  if (priv->buffer)
    {
      gtk_text_buffer_get_bounds (priv->buffer, &begin, &end);
      line = gtk_text_iter_get_line (&end);
      g_array_set_size (priv->state, line + 1);
      memset (priv->state->data, 0, priv->state->len);
    }
}

GtkTextBuffer *
gb_source_change_monitor_get_buffer (GbSourceChangeMonitor *monitor)
{
  g_return_val_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor), NULL);

  return monitor->priv->buffer;
}

static void
gb_source_change_monitor_set_buffer (GbSourceChangeMonitor *monitor,
                                     GtkTextBuffer         *buffer,
                                     gboolean               notify)
{
  GbSourceChangeMonitorPrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_MONITOR (monitor));
  g_return_if_fail (!buffer || GB_IS_SOURCE_CHANGE_MONITOR (monitor));

  priv = monitor->priv;

  if (priv->buffer)
    {
      g_signal_handler_disconnect (priv->buffer,
                                   priv->delete_range_before_handler);
      g_signal_handler_disconnect (priv->buffer,
                                   priv->delete_range_after_handler);
      g_signal_handler_disconnect (priv->buffer,
                                   priv->insert_text_before_handler);
      g_signal_handler_disconnect (priv->buffer,
                                   priv->insert_text_after_handler);
      priv->delete_range_before_handler = 0;
      priv->delete_range_after_handler = 0;
      priv->insert_text_before_handler = 0;
      priv->insert_text_after_handler = 0;
      g_clear_object (&priv->buffer);
    }

  if (buffer)
    {
      priv->buffer = g_object_ref (buffer);
      priv->delete_range_before_handler =
        g_signal_connect_object (priv->buffer,
                                 "delete-range",
                                 G_CALLBACK (on_delete_range_before_cb),
                                 monitor,
                                 G_CONNECT_SWAPPED);
      priv->delete_range_after_handler =
        g_signal_connect_object (priv->buffer,
                                 "delete-range",
                                 G_CALLBACK (on_delete_range_after_cb),
                                 monitor,
                                 (G_CONNECT_SWAPPED | G_CONNECT_AFTER));
      priv->insert_text_before_handler =
        g_signal_connect_object (priv->buffer,
                                 "insert-text",
                                 G_CALLBACK (on_insert_text_before_cb),
                                 monitor,
                                 G_CONNECT_SWAPPED);
      priv->insert_text_after_handler =
        g_signal_connect_object (priv->buffer,
                                 "insert-text",
                                 G_CALLBACK (on_insert_text_after_cb),
                                 monitor,
                                 (G_CONNECT_SWAPPED | G_CONNECT_AFTER));

      gb_source_change_monitor_ensure_bounds (monitor);
    }

  if (notify)
    g_object_notify_by_pspec (G_OBJECT (monitor), gParamSpecs [PROP_BUFFER]);

  EXIT;
}

static void
gb_source_change_monitor_dispose (GObject *object)
{
  ENTRY;

  gb_source_change_monitor_set_buffer (GB_SOURCE_CHANGE_MONITOR (object),
                                       NULL, FALSE);

  G_OBJECT_CLASS (gb_source_change_monitor_parent_class)->dispose (object);

  EXIT;
}

static void
gb_source_change_monitor_finalize (GObject *object)
{
  GbSourceChangeMonitorPrivate *priv;

  ENTRY;

  priv = GB_SOURCE_CHANGE_MONITOR (object)->priv;

  g_clear_pointer (&priv->state, g_array_unref);

  G_OBJECT_CLASS (gb_source_change_monitor_parent_class)->finalize (object);

  EXIT;
}

static void
gb_source_change_monitor_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GbSourceChangeMonitor *monitor = GB_SOURCE_CHANGE_MONITOR (object);

  switch (prop_id) {
  case PROP_BUFFER:
    g_value_set_object (value,
                        gb_source_change_monitor_get_buffer (monitor));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gb_source_change_monitor_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GbSourceChangeMonitor *monitor = GB_SOURCE_CHANGE_MONITOR (object);

  switch (prop_id) {
  case PROP_BUFFER:
    gb_source_change_monitor_set_buffer (monitor,
                                         g_value_get_object (value),
                                         TRUE);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gb_source_change_monitor_class_init (GbSourceChangeMonitorClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = gb_source_change_monitor_dispose;
  object_class->finalize = gb_source_change_monitor_finalize;
  object_class->get_property = gb_source_change_monitor_get_property;
  object_class->set_property = gb_source_change_monitor_set_property;

  gParamSpecs [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         _("Buffer"),
                         _("The text buffer to monitor."),
                         GTK_TYPE_TEXT_BUFFER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BUFFER,
                                   gParamSpecs [PROP_BUFFER]);
}

static void
gb_source_change_monitor_init (GbSourceChangeMonitor *monitor)
{
  ENTRY;

  monitor->priv = gb_source_change_monitor_get_instance_private (monitor);

  monitor->priv->state = g_array_new (FALSE, TRUE, sizeof (guint8));
  g_array_set_size (monitor->priv->state, 1);

  EXIT;
}
