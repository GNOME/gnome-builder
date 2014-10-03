/* gb-preferences-window.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-preferences-window.h"
#include "gb-sidebar.h"

struct _GbPreferencesWindowPrivate
{
  void *d;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesWindow, gb_preferences_window,
                            GTK_TYPE_WINDOW)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_preferences_window_new (void)
{
  return g_object_new (GB_TYPE_PREFERENCES_WINDOW, NULL);
}

static void
gb_preferences_window_finalize (GObject *object)
{
  GbPreferencesWindowPrivate *priv = GB_PREFERENCES_WINDOW (object)->priv;

  G_OBJECT_CLASS (gb_preferences_window_parent_class)->finalize (object);
}

static void
gb_preferences_window_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbPreferencesWindow *self = GB_PREFERENCES_WINDOW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_window_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbPreferencesWindow *self = GB_PREFERENCES_WINDOW (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_window_class_init (GbPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_preferences_window_finalize;
  object_class->get_property = gb_preferences_window_get_property;
  object_class->set_property = gb_preferences_window_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-preferences-window.ui");

  g_type_ensure (GB_TYPE_SIDEBAR);
}

static void
gb_preferences_window_init (GbPreferencesWindow *self)
{
  self->priv = gb_preferences_window_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
