/* ide-preferences-container.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "ide-preferences-container.h"

typedef struct
{
  GtkBin parent_instance;
  gint   priority;
  gchar *keywords;
} IdePreferencesContainerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdePreferencesContainer, ide_preferences_container, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_KEYWORDS,
  PROP_PRIORITY,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_preferences_container_finalize (GObject *object)
{
  IdePreferencesContainer *self = (IdePreferencesContainer *)object;
  IdePreferencesContainerPrivate *priv = ide_preferences_container_get_instance_private (self);

  g_clear_pointer (&priv->keywords, g_free);

  G_OBJECT_CLASS (ide_preferences_container_parent_class)->finalize (object);
}

static void
ide_preferences_container_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  IdePreferencesContainer *self = IDE_PREFERENCES_CONTAINER (object);
  IdePreferencesContainerPrivate *priv = ide_preferences_container_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_KEYWORDS:
      g_value_set_string (value, priv->keywords);
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, priv->priority);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_container_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdePreferencesContainer *self = IDE_PREFERENCES_CONTAINER (object);
  IdePreferencesContainerPrivate *priv = ide_preferences_container_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_KEYWORDS:
      priv->keywords = g_value_dup_string (value);
      break;

    case PROP_PRIORITY:
      priv->priority = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_container_class_init (IdePreferencesContainerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_preferences_container_finalize;
  object_class->get_property = ide_preferences_container_get_property;
  object_class->set_property = ide_preferences_container_set_property;

  properties [PROP_KEYWORDS] =
    g_param_spec_string ("keywords",
                         "Keywords",
                         "Search keywords for the widget.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The widget priority within the group.",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_preferences_container_init (IdePreferencesContainer *self)
{
}
