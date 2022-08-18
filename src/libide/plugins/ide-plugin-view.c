/* ide-plugin-view.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-plugin-view"

#include "config.h"

#include "ide-plugin-view.h"

struct _IdePluginView
{
  GtkWidget  parent_instance;
  IdePlugin *plugin;
};

enum {
  PROP_0,
  PROP_PLUGIN,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdePluginView, ide_plugin_view, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_plugin_view_dispose (GObject *object)
{
  IdePluginView *self = (IdePluginView *)object;
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
    gtk_widget_unparent (child);

  g_clear_object (&self->plugin);

  G_OBJECT_CLASS (ide_plugin_view_parent_class)->dispose (object);
}

static void
ide_plugin_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdePluginView *self = IDE_PLUGIN_VIEW (object);

  switch (prop_id)
    {
    case PROP_PLUGIN:
      g_value_set_object (value, self->plugin);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_plugin_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdePluginView *self = IDE_PLUGIN_VIEW (object);

  switch (prop_id)
    {
    case PROP_PLUGIN:
      self->plugin = g_value_dup_object (value);
      for (guint i = 1; i < N_PROPS; i++)
        g_object_notify_by_pspec (object, properties[i]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_plugin_view_class_init (IdePluginViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_plugin_view_dispose;
  object_class->get_property = ide_plugin_view_get_property;
  object_class->set_property = ide_plugin_view_set_property;

  properties[PROP_PLUGIN] =
    g_param_spec_object ("plugin", NULL, NULL,
                         IDE_TYPE_PLUGIN,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-plugins/ide-plugin-view.ui");
}

static void
ide_plugin_view_init (IdePluginView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_plugin_view_new (IdePlugin *plugin)
{
  return g_object_new (IDE_TYPE_PLUGIN_VIEW,
                       "plugin", plugin,
                       NULL);
}
