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
  IdePane             parent_instance;

  GtkColumnView      *column_view;
  GtkNoSelection     *selection;
  GtkFilterListModel *filter_model;

  GtkCustomFilter    *filter;

  GLogLevelFlags      severity;
};

G_DEFINE_FINAL_TYPE (GbpMessagesPanel, gbp_messages_panel, IDE_TYPE_PANE)

enum {
  PROP_0,
  PROP_SEVERITY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static const char *
gbp_messages_panel_get_severity (GbpMessagesPanel *self)
{
  switch ((int)self->severity)
    {
    case G_LOG_LEVEL_DEBUG: return "debug";
    case G_LOG_LEVEL_WARNING: return "warning";
    case G_LOG_LEVEL_CRITICAL: return "critical";
    case G_LOG_LEVEL_INFO: return "info";

    case G_LOG_LEVEL_MESSAGE:
    default:
      return "message";
    }
}

static void
gbp_messages_panel_set_severity (GbpMessagesPanel *self,
                                 const char       *severity)
{
  GLogLevelFlags val = G_LOG_LEVEL_MESSAGE;

  if (ide_str_equal0 (severity, "debug"))
    val = G_LOG_LEVEL_DEBUG;
  else if (ide_str_equal0 (severity, "message"))
    val = G_LOG_LEVEL_MESSAGE;
  else if (ide_str_equal0 (severity, "info"))
    val = G_LOG_LEVEL_INFO;
  else if (ide_str_equal0 (severity, "critical"))
    val = G_LOG_LEVEL_CRITICAL;
  else if (ide_str_equal0 (severity, "warning"))
    val = G_LOG_LEVEL_WARNING;

  if (val != self->severity)
    {
      self->severity = val;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SEVERITY]);
      gtk_filter_changed (GTK_FILTER (self->filter), GTK_FILTER_CHANGE_DIFFERENT);
    }
}

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

  gtk_filter_list_model_set_model (self->filter_model, model);
}

static gboolean
gbp_messages_panel_filter (gpointer item,
                           gpointer user_data)
{
  IdeLogItem *log = item;
  GbpMessagesPanel *self = user_data;

  return ide_log_item_get_severity (log) <= self->severity;
}

static void
gbp_messages_panel_dispose (GObject *object)
{
  GbpMessagesPanel *self = (GbpMessagesPanel *)object;

  if (self->selection != NULL)
    gtk_no_selection_set_model (self->selection, NULL);

  if (self->filter_model)
    gtk_filter_list_model_set_model (self->filter_model, NULL);

  if (self->filter != NULL)
    {
      gtk_custom_filter_set_filter_func (self->filter, NULL, NULL, NULL);
      g_clear_object (&self->filter);
    }

  gtk_widget_dispose_template (GTK_WIDGET (self), GBP_TYPE_MESSAGES_PANEL);

  G_OBJECT_CLASS (gbp_messages_panel_parent_class)->dispose (object);
}

static void
gbp_messages_panel_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpMessagesPanel *self = GBP_MESSAGES_PANEL (object);

  switch (prop_id)
    {
    case PROP_SEVERITY:
      g_value_set_static_string (value, gbp_messages_panel_get_severity (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_messages_panel_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbpMessagesPanel *self = GBP_MESSAGES_PANEL (object);

  switch (prop_id)
    {
    case PROP_SEVERITY:
      gbp_messages_panel_set_severity (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_messages_panel_class_init (GbpMessagesPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  PanelWidgetClass *panel_widget_class = PANEL_WIDGET_CLASS (klass);

  object_class->dispose = gbp_messages_panel_dispose;
  object_class->get_property = gbp_messages_panel_get_property;
  object_class->set_property = gbp_messages_panel_set_property;

  properties [PROP_SEVERITY] =
    g_param_spec_string ("severity", NULL, NULL,
                         "message",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/messages/gbp-messages-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpMessagesPanel, column_view);
  gtk_widget_class_bind_template_child (widget_class, GbpMessagesPanel, selection);
  gtk_widget_class_bind_template_child (widget_class, GbpMessagesPanel, filter_model);
  gtk_widget_class_bind_template_callback (widget_class, date_time_to_string);
  gtk_widget_class_bind_template_callback (widget_class, severity_to_string);

  panel_widget_class_install_property_action (panel_widget_class, "messages.severity", "severity");
}

static void
gbp_messages_panel_init (GbpMessagesPanel *self)
{
  self->severity = G_LOG_LEVEL_MESSAGE;

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_widget_set_context_handler (GTK_WIDGET (self), gbp_messages_panel_set_context);

  self->filter = gtk_custom_filter_new (gbp_messages_panel_filter, self, NULL);
  gtk_filter_list_model_set_filter (self->filter_model, GTK_FILTER (self->filter));
}
