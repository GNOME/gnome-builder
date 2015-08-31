/* gb-shortcuts-shortcut.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "gb-accel-label.h"
#include "gb-shortcuts-shortcut.h"

struct _GbShortcutsShortcut
{
  GtkBox        parent_instance;

  GbAccelLabel *accelerator;
  GtkLabel     *title;
};

G_DEFINE_TYPE (GbShortcutsShortcut, gb_shortcuts_shortcut, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_ACCELERATOR_SIZE_GROUP,
  PROP_TITLE,
  PROP_TITLE_SIZE_GROUP,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_shortcuts_shortcut_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbShortcutsShortcut *self = GB_SHORTCUTS_SHORTCUT (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    case PROP_ACCELERATOR:
      g_value_set_string (value, gb_accel_label_get_accelerator (self->accelerator));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_shortcuts_shortcut_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbShortcutsShortcut *self = GB_SHORTCUTS_SHORTCUT (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      gb_accel_label_set_accelerator (self->accelerator, g_value_get_string (value));
      break;

    case PROP_ACCELERATOR_SIZE_GROUP:
      {
        GtkSizeGroup *group = g_value_get_object (value);

        if (group != NULL)
          gtk_size_group_add_widget (group, GTK_WIDGET (self->accelerator));
        break;
      }

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_TITLE_SIZE_GROUP:
      {
        GtkSizeGroup *group = g_value_get_object (value);

        if (group != NULL)
          gtk_size_group_add_widget (group, GTK_WIDGET (self->title));
        break;
      }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_shortcuts_shortcut_class_init (GbShortcutsShortcutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gb_shortcuts_shortcut_get_property;
  object_class->set_property = gb_shortcuts_shortcut_set_property;

  gParamSpecs [PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator",
                         "Accelerator",
                         "Accelerator",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_ACCELERATOR_SIZE_GROUP] =
    g_param_spec_object ("accelerator-size-group",
                         "Accelerator Size Group",
                         "Accelerator Size Group",
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_TITLE_SIZE_GROUP] =
    g_param_spec_object ("title-size-group",
                         "Title Size Group",
                         "Title Size Group",
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_shortcuts_shortcut_init (GbShortcutsShortcut *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (self), 12);

  self->accelerator = g_object_new (GB_TYPE_ACCEL_LABEL,
                                    "visible", TRUE,
                                    NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->accelerator));

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "hexpand", TRUE,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->title));
}
