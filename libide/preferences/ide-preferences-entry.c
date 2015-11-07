/* ide-preferences-entry.c
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

#include <glib/gi18n.h>

#include "ide-preferences-entry.h"

typedef struct
{
  GtkEntry *entry;
  GtkLabel *title;
} IdePreferencesEntryPrivate;

enum {
  PROP_0,
  PROP_TITLE,
  PROP_TEXT,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (IdePreferencesEntry, ide_preferences_entry, IDE_TYPE_PREFERENCES_CONTAINER)

static GParamSpec *properties [LAST_PROP];

static void
ide_preferences_entry_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdePreferencesEntry *self = IDE_PREFERENCES_ENTRY (object);
  IdePreferencesEntryPrivate *priv = ide_preferences_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, gtk_entry_get_text (priv->entry));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_text (priv->title));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_entry_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdePreferencesEntry *self = IDE_PREFERENCES_ENTRY (object);
  IdePreferencesEntryPrivate *priv = ide_preferences_entry_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_TEXT:
      gtk_entry_set_text (priv->entry, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gtk_label_set_label (priv->title, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_entry_class_init (IdePreferencesEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_preferences_entry_get_property;
  object_class->set_property = ide_preferences_entry_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-preferences-entry.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdePreferencesEntry, entry);
  gtk_widget_class_bind_template_child_private (widget_class, IdePreferencesEntry, title);

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "Text",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_preferences_entry_init (IdePreferencesEntry *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
