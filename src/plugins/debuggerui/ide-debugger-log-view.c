/* ide-debugger-log-view.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#define G_LOG_DOMAIN "ide-debugger-log-view"

#include <libide-terminal.h>

#include "ide-debugger-log-view.h"

struct _IdeDebuggerLogView
{
  GtkBox parent_instance;

  IdeTerminal *terminal;
  GtkEntry *commandentry;

  IdeDebugger *debugger;
};

G_DEFINE_FINAL_TYPE (IdeDebuggerLogView, ide_debugger_log_view, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_DEBUGGER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

IdeDebuggerLogView *
ide_debugger_log_view_new (void)
{
  return g_object_new (IDE_TYPE_DEBUGGER_LOG_VIEW, NULL);
}

static void
ide_debugger_log_view_dispose (GObject *object)
{
  IdeDebuggerLogView *self = (IdeDebuggerLogView *)object;

  g_clear_object (&self->debugger);

  G_OBJECT_CLASS (ide_debugger_log_view_parent_class)->dispose (object);
}

static void
ide_debugger_log_view_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeDebuggerLogView *self = IDE_DEBUGGER_LOG_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      g_value_set_object (value, ide_debugger_log_view_get_debugger (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_log_view_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeDebuggerLogView *self = IDE_DEBUGGER_LOG_VIEW (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      ide_debugger_log_view_set_debugger (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
on_entry_activate_cb (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  IdeDebugger *debugger = (IdeDebugger *)source;
  g_autoptr(IdeDebuggerLogView) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DEBUGGER_LOG_VIEW (self));

  gtk_editable_set_text (GTK_EDITABLE (self->commandentry), "");
  gtk_widget_set_sensitive (GTK_WIDGET (self->commandentry), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->commandentry));

  if (!ide_debugger_interpret_finish (debugger, result, &error))
    {
      vte_terminal_feed (VTE_TERMINAL (self->terminal),
                         error->message,
                         strlen (error->message));
      vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", 2);
    }

  IDE_EXIT;
}

static void
on_entry_activate (IdeDebuggerLogView *self,
                   GtkEntry           *entry)
{
  g_autofree gchar *text = NULL;

  g_return_if_fail (IDE_IS_DEBUGGER_LOG_VIEW (self));
  g_return_if_fail (GTK_IS_ENTRY (entry));

  text = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry))));
  if (ide_str_empty0 (text))
    return;

  vte_terminal_feed (VTE_TERMINAL (self->terminal), "> ", 2);
  vte_terminal_feed (VTE_TERMINAL (self->terminal), text, strlen (text));
  vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", 2);

  if (self->debugger != NULL)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->commandentry), FALSE);
      ide_debugger_interpret_async (self->debugger,
                                    text,
                                    NULL,
                                    on_entry_activate_cb,
                                    g_object_ref (self));
    }
}

static void
ide_debugger_log_view_class_init (IdeDebuggerLogViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_debugger_log_view_dispose;
  object_class->get_property = ide_debugger_log_view_get_property;
  object_class->set_property = ide_debugger_log_view_set_property;

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "Debugger",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/debuggerui/ide-debugger-log-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLogView, terminal);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerLogView, commandentry);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_activate);
}

static void
ide_debugger_log_view_init (IdeDebuggerLogView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
ide_debugger_log_view_debugger_log (IdeDebuggerLogView *self,
                                    IdeDebuggerStream   stream,
                                    GBytes             *content,
                                    IdeDebugger        *debugger)
{
  g_assert (IDE_IS_DEBUGGER_LOG_VIEW (self));
  g_assert (IDE_IS_DEBUGGER_STREAM (stream));
  g_assert (content != NULL);
  g_assert (IDE_IS_DEBUGGER (debugger));

  if (stream == IDE_DEBUGGER_CONSOLE)
    {
      IdeLineReader reader;
      const gchar *str;
      gchar *line;
      gsize len;
      gsize line_len;

      str = g_bytes_get_data (content, &len);

      /*
       * Ignore \n so we can add \r\n. Otherwise we get problematic
       * output in the terminal.
       */
      ide_line_reader_init (&reader, (gchar *)str, len);
      while (NULL != (line = ide_line_reader_next (&reader, &line_len)))
        {
          vte_terminal_feed (VTE_TERMINAL (self->terminal), line, line_len);

          if ((line + line_len) < (str + len))
            {
              if (line[line_len] == '\r' || line[line_len] == '\n')
                vte_terminal_feed (VTE_TERMINAL (self->terminal), "\r\n", 2);
            }
        }
    }
}

void
ide_debugger_log_view_set_debugger (IdeDebuggerLogView *self,
                                    IdeDebugger        *debugger)
{
  g_return_if_fail (IDE_IS_DEBUGGER_LOG_VIEW (self));
  g_return_if_fail (!debugger || IDE_IS_DEBUGGER (debugger));

  if (g_set_object (&self->debugger, debugger))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);
}

IdeDebugger *
ide_debugger_log_view_get_debugger (IdeDebuggerLogView *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_LOG_VIEW (self), NULL);

  return self->debugger;
}
