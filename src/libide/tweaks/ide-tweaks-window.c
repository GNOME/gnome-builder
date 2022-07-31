/* ide-tweaks-window.c
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

#define G_LOG_DOMAIN "ide-tweaks-window"

#include "config.h"

#include "ide-tweaks-window.h"

struct _IdeTweaksWindow
{
  AdwWindow  parent_instance;
  IdeTweaks *tweaks;
};

enum {
  PROP_0,
  PROP_TWEAKS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksWindow, ide_tweaks_window, ADW_TYPE_WINDOW)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_window_dispose (GObject *object)
{
  IdeTweaksWindow *self = (IdeTweaksWindow *)object;

  g_clear_object (&self->tweaks);

  G_OBJECT_CLASS (ide_tweaks_window_parent_class)->dispose (object);
}

static void
ide_tweaks_window_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTweaksWindow *self = IDE_TWEAKS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_TWEAKS:
      g_value_set_object (value, ide_tweaks_window_get_tweaks (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_window_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTweaksWindow *self = IDE_TWEAKS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_TWEAKS:
      ide_tweaks_window_set_tweaks (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_window_class_init (IdeTweaksWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_window_dispose;
  object_class->get_property = ide_tweaks_window_get_property;
  object_class->set_property = ide_tweaks_window_set_property;

  properties [PROP_TWEAKS] =
    g_param_spec_object ("tweaks", NULL, NULL,
                         IDE_TYPE_TWEAKS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tweaks/ide-tweaks-window.ui");
}

static void
ide_tweaks_window_init (IdeTweaksWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_tweaks_window_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_WINDOW, NULL);
}

/**
 * ide_tweaks_window_get_tweaks:
 * @self: a #IdeTweaksWindow
 *
 * Gets the tweaks property of the window.
 *
 * Returns: (transfer none) (nullable): an #IdeTweaks or %NULL
 */
IdeTweaks *
ide_tweaks_window_get_tweaks (IdeTweaksWindow *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_WINDOW (self), NULL);

  return self->tweaks;
}

/**
 * ide_tweaks_window_set_tweaks:
 * @self: a #IdeTweaksWindow
 * @tweaks: (nullable): an #IdeTweaks
 *
 * Sets the tweaks to be displayed in the window.
 */
void
ide_tweaks_window_set_tweaks (IdeTweaksWindow *self,
                              IdeTweaks       *tweaks)
{
  g_return_if_fail (IDE_IS_TWEAKS_WINDOW (self));
  g_return_if_fail (!tweaks || IDE_IS_TWEAKS (tweaks));

  if (g_set_object (&self->tweaks, tweaks))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TWEAKS]);
}
