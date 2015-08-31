/* gb-shortcuts-gesture.c
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

#include "gb-shortcuts-gesture.h"

struct _GbShortcutsGesture
{
  GtkBox    parent_instance;

  GtkImage *image;
  GtkLabel *title;
  GtkLabel *subtitle;
  GtkBox   *desc_box;
};

G_DEFINE_TYPE (GbShortcutsGesture, gb_shortcuts_gesture, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_DESC_SIZE_GROUP,
  PROP_ICON_NAME,
  PROP_ICON_SIZE_GROUP,
  PROP_SUBTITLE,
  PROP_TITLE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_shortcuts_gesture_set_icon_name (GbShortcutsGesture *self,
                                    const gchar        *icon_name)
{
  g_autofree gchar *basedir = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GBytes) bytes = NULL;

  g_assert (GB_IS_SHORTCUTS_GESTURE (self));

  if (icon_name == NULL)
    {
      g_object_set (self->image, "icon-name", NULL, NULL);
      return;
    }

  basedir = g_build_filename (g_application_get_resource_base_path (g_application_get_default ()),
                              "icons", "scalable", "actions",
                              NULL);
  path = g_strdup_printf ("%s"G_DIR_SEPARATOR_S"%s.svg", basedir, icon_name);
  bytes = g_resources_lookup_data (path, 0, NULL);

  if (bytes != NULL)
    g_object_set (self->image, "resource", path, NULL);
  else
    g_object_set (self->image, "icon-name", icon_name, NULL);
}

static void
gb_shortcuts_gesture_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbShortcutsGesture *self = GB_SHORTCUTS_GESTURE (object);

  switch (prop_id)
    {
    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (self->subtitle));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_shortcuts_gesture_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbShortcutsGesture *self = GB_SHORTCUTS_GESTURE (object);

  switch (prop_id)
    {
    case PROP_DESC_SIZE_GROUP:
      {
        GtkSizeGroup *group = g_value_get_object (value);

        if (group != NULL)
          gtk_size_group_add_widget (group, GTK_WIDGET (self->desc_box));
        break;
      }

    case PROP_ICON_NAME:
      gb_shortcuts_gesture_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ICON_SIZE_GROUP:
      {
        GtkSizeGroup *group = g_value_get_object (value);

        if (group != NULL)
          gtk_size_group_add_widget (group, GTK_WIDGET (self->image));
        break;
      }

    case PROP_SUBTITLE:
      gtk_label_set_label (self->subtitle, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_shortcuts_gesture_class_init (GbShortcutsGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gb_shortcuts_gesture_get_property;
  object_class->set_property = gb_shortcuts_gesture_set_property;

  gParamSpecs [PROP_DESC_SIZE_GROUP] =
    g_param_spec_object ("desc-size-group",
                         "Description Size Group",
                         "Description Size Group",
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Icon Name",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_ICON_SIZE_GROUP] =
    g_param_spec_object ("icon-size-group",
                         "Icon Size Group",
                         "Icon Size Group",
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "Subtitle",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_shortcuts_gesture_init (GbShortcutsGesture *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (self), 12);

  self->image = g_object_new (GTK_TYPE_IMAGE,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->image));

  self->desc_box = g_object_new (GTK_TYPE_BOX,
                                 "hexpand", TRUE,
                                 "orientation", GTK_ORIENTATION_VERTICAL,
                                 "visible", TRUE,
                                 NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->desc_box));

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              NULL);
  gtk_container_add (GTK_CONTAINER (self->desc_box), GTK_WIDGET (self->title));

  self->subtitle = g_object_new (GTK_TYPE_LABEL,
                                 "visible", TRUE,
                                 "xalign", 0.0f,
                                 NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->subtitle)),
                               "dim-label");
  gtk_container_add (GTK_CONTAINER (self->desc_box), GTK_WIDGET (self->subtitle));
}
