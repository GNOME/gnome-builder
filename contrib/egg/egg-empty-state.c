/* egg-empty-state.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <string.h>
#include <stdlib.h>

#include "egg-empty-state.h"

typedef struct
{
  GtkBox   *box;
  GtkImage *image;
  GtkLabel *subtitle;
  GtkLabel *title;
} EggEmptyStatePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EggEmptyState, egg_empty_state, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_PIXEL_SIZE,
  PROP_RESOURCE,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
egg_empty_state_action (GtkWidget   *widget,
                        const gchar *prefix,
                        const gchar *action_name,
                        GVariant    *parameter)
{
  GtkWidget *toplevel;
  GApplication *app;
  GActionGroup *group = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (prefix, FALSE);
  g_return_val_if_fail (action_name, FALSE);

  app = g_application_get_default ();
  toplevel = gtk_widget_get_toplevel (widget);

  while ((group == NULL) && (widget != NULL))
    {
      group = gtk_widget_get_action_group (widget, prefix);
      widget = gtk_widget_get_parent (widget);
    }

  if (!group && g_str_equal (prefix, "win") && G_IS_ACTION_GROUP (toplevel))
    group = G_ACTION_GROUP (toplevel);

  if (!group && g_str_equal (prefix, "app") && G_IS_ACTION_GROUP (app))
    group = G_ACTION_GROUP (app);

  if (group && g_action_group_has_action (group, action_name))
    {
      g_action_group_activate_action (group, action_name, parameter);
      return TRUE;
    }

  if (parameter && g_variant_is_floating (parameter))
    {
      parameter = g_variant_ref_sink (parameter);
      g_variant_unref (parameter);
    }

  g_warning ("Failed to locate action %s.%s", prefix, action_name);

  return FALSE;
}

static gboolean
egg_empty_state_activate_link (EggEmptyState *self,
                               const gchar   *uri,
                               GtkLabel      *label)
{
  g_assert (EGG_IS_EMPTY_STATE (self));
  g_assert (uri != NULL);
  g_assert (GTK_IS_LABEL (label));

  if (g_str_has_prefix (uri, "action://"))
    {
      g_autofree gchar *full_name = NULL;
      g_autofree gchar *action_name = NULL;
      g_autofree gchar *group_name = NULL;
      g_autoptr(GVariant) param = NULL;
      g_autoptr(GError) error = NULL;

      uri += strlen ("action://");

      if (g_action_parse_detailed_name (uri, &full_name, &param, &error))
        {
          const gchar *dot = strchr (full_name, '.');

          if (param != NULL && g_variant_is_floating (param))
            param = g_variant_ref_sink (param);

          if (dot == NULL)
            return FALSE;

          group_name = g_strndup (full_name, dot - full_name);
          action_name = g_strdup (++dot);

          egg_empty_state_action (GTK_WIDGET (self),
                                  group_name,
                                  action_name,
                                  param);

          return TRUE;
        }
      else
        g_warning ("%s", error->message);
    }

  return FALSE;
}

static void
egg_empty_state_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  EggEmptyState *self = EGG_EMPTY_STATE (object);
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, egg_empty_state_get_icon_name (self));
      break;

    case PROP_PIXEL_SIZE:
      g_value_set_int (value, gtk_image_get_pixel_size (priv->image));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, egg_empty_state_get_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, egg_empty_state_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_empty_state_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  EggEmptyState *self = EGG_EMPTY_STATE (object);
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      egg_empty_state_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_PIXEL_SIZE:
      gtk_image_set_pixel_size (priv->image, g_value_get_int (value));
      break;

    case PROP_RESOURCE:
      egg_empty_state_set_resource (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      egg_empty_state_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      egg_empty_state_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_empty_state_class_init (EggEmptyStateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = egg_empty_state_get_property;
  object_class->set_property = egg_empty_state_set_property;

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The name of the icon to display",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PIXEL_SIZE] =
    g_param_spec_int ("pixel-size",
                      "Pixel Size",
                      "Pixel Size",
                      0,
                      G_MAXINT,
                      128,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RESOURCE] =
    g_param_spec_string ("resource",
                         "Resource",
                         "A resource path to use for the icon",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "The subtitle of the empty state",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the empty state",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/libegg-private/egg-empty-state.ui");
  gtk_widget_class_bind_template_child_private (widget_class, EggEmptyState, box);
  gtk_widget_class_bind_template_child_private (widget_class, EggEmptyState, image);
  gtk_widget_class_bind_template_child_private (widget_class, EggEmptyState, title);
  gtk_widget_class_bind_template_child_private (widget_class, EggEmptyState, subtitle);
}

static void
egg_empty_state_init (EggEmptyState *self)
{
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (priv->subtitle,
                           "activate-link",
                           G_CALLBACK (egg_empty_state_activate_link),
                           self,
                           G_CONNECT_SWAPPED);
}

const gchar *
egg_empty_state_get_icon_name (EggEmptyState *self)
{
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);
  const gchar *icon_name = NULL;

  g_return_val_if_fail (EGG_IS_EMPTY_STATE (self), NULL);

  gtk_image_get_icon_name (priv->image, &icon_name, NULL);

  return icon_name;
}

void
egg_empty_state_set_icon_name (EggEmptyState *self,
                               const gchar   *icon_name)
{
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  g_return_if_fail (EGG_IS_EMPTY_STATE (self));

  if (g_strcmp0 (icon_name, egg_empty_state_get_icon_name (self)) != 0)
    {
      GtkStyleContext *context;

      g_object_set (priv->image,
                    "icon-name", icon_name,
                    NULL);

      context = gtk_widget_get_style_context (GTK_WIDGET (priv->image));

      if (icon_name != NULL && g_str_has_suffix (icon_name, "-symbolic"))
        gtk_style_context_add_class (context, "dim-label");
      else
        gtk_style_context_remove_class (context, "dim-label");

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON_NAME]);
    }
}

const gchar *
egg_empty_state_get_subtitle (EggEmptyState *self)
{
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_EMPTY_STATE (self), NULL);

  return gtk_label_get_label (priv->subtitle);
}

void
egg_empty_state_set_subtitle (EggEmptyState *self,
                              const gchar   *subtitle)
{
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  g_return_if_fail (EGG_IS_EMPTY_STATE (self));

  if (g_strcmp0 (subtitle, egg_empty_state_get_subtitle (self)) != 0)
    {
      gtk_label_set_label (priv->subtitle, subtitle);
      gtk_widget_set_visible (GTK_WIDGET (priv->subtitle), subtitle && *subtitle);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
    }
}

const gchar *
egg_empty_state_get_title (EggEmptyState *self)
{
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_EMPTY_STATE (self), NULL);

  return gtk_label_get_label (priv->title);
}

void
egg_empty_state_set_title (EggEmptyState *self,
                           const gchar   *title)
{
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  g_return_if_fail (EGG_IS_EMPTY_STATE (self));

  if (g_strcmp0 (title, egg_empty_state_get_title (self)) != 0)
    {
      gtk_label_set_label (priv->title, title);
      gtk_widget_set_visible (GTK_WIDGET (priv->title), title && *title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

void
egg_empty_state_set_resource (EggEmptyState *self,
                              const gchar   *resource)
{
  EggEmptyStatePrivate *priv = egg_empty_state_get_instance_private (self);

  g_return_if_fail (EGG_IS_EMPTY_STATE (self));

  if (resource != NULL)
    {
      GdkPixbuf *pixbuf;
      GError *error = NULL;
      gint scale_factor;

      scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));

      pixbuf = gdk_pixbuf_new_from_resource_at_scale (resource,
                                                      128 * scale_factor,
                                                      128 * scale_factor,
                                                      TRUE,
                                                      &error);

      if (pixbuf == NULL)
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
          return;
        }

      g_object_set (priv->image,
                    "pixbuf", pixbuf,
                    NULL);

      g_clear_object (&pixbuf);
    }
}
