/* gbp-messages-panel.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-messages-panel"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-messages-panel.h"

struct _GbpMessagesPanel
{
  IdePane         parent_instance;
  GtkColumnView  *column_view;
  GtkNoSelection *selection;
};

G_DEFINE_FINAL_TYPE (GbpMessagesPanel, gbp_messages_panel, IDE_TYPE_PANE)

static char *
severity_to_string (GObject        *object,
                    GLogLevelFlags  flags)
{
  const char *ret = "";

  flags &= G_LOG_LEVEL_MASK;

  if (flags & G_LOG_LEVEL_DEBUG)
    ret = _("Debug");
  else if (flags & G_LOG_LEVEL_INFO)
    ret = _("Info");
  else if (flags & G_LOG_LEVEL_MESSAGE)
    ret = _("Message");
  else if (flags & G_LOG_LEVEL_WARNING)
    ret = _("Warning");
  else if (flags & G_LOG_LEVEL_CRITICAL)
    ret = _("Critical");

  return g_strdup (ret);
}

static char *
date_time_to_string (GObject   *object,
                     GDateTime *dt)
{
  return g_date_time_format (dt, "%X");
}

static void
gbp_messages_panel_set_context (GtkWidget  *widget,
                                IdeContext *context)
{
  GbpMessagesPanel *self = (GbpMessagesPanel *)widget;
  g_autoptr(GListModel) model = NULL;

  g_assert (GBP_IS_MESSAGES_PANEL (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context != NULL)
    model = ide_context_ref_logs (context);

  gtk_no_selection_set_model (self->selection, model);
}

static void
gbp_messages_panel_class_init (GbpMessagesPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/messages/gbp-messages-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpMessagesPanel, column_view);
  gtk_widget_class_bind_template_child (widget_class, GbpMessagesPanel, selection);
  gtk_widget_class_bind_template_callback (widget_class, date_time_to_string);
  gtk_widget_class_bind_template_callback (widget_class, severity_to_string);
}

static void
gbp_messages_panel_init (GbpMessagesPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  ide_widget_set_context_handler (GTK_WIDGET (self), gbp_messages_panel_set_context);
}
