/* gb-workspace.c
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

#include "gb-workspace.h"

typedef struct
{
  gchar *icon_name;
  gchar *title;
} GbWorkspacePrivate;

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_TITLE,
  LAST_PROP
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GbWorkspace, gb_workspace, GTK_TYPE_BIN)

static GParamSpec *gParamSpecs[LAST_PROP];

const gchar *
gb_workspace_get_icon_name (GbWorkspace *self)
{
  GbWorkspacePrivate *priv = gb_workspace_get_instance_private (self);

  g_return_val_if_fail (GB_IS_WORKSPACE (self), NULL);

  return priv->icon_name;
}

void
gb_workspace_set_icon_name (GbWorkspace *self,
                            const gchar *icon_name)
{
  GbWorkspacePrivate *priv = gb_workspace_get_instance_private (self);

  g_return_if_fail (GB_IS_WORKSPACE (self));

  if (priv->icon_name != icon_name)
    {
      g_free (priv->icon_name);
      priv->icon_name = g_strdup (icon_name);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs[PROP_ICON_NAME]);
    }
}

const gchar *
gb_workspace_get_title (GbWorkspace *self)
{
  GbWorkspacePrivate *priv = gb_workspace_get_instance_private (self);

  g_return_val_if_fail (GB_IS_WORKSPACE (self), NULL);

  return priv->title;
}

void
gb_workspace_set_title (GbWorkspace *self,
                        const gchar *title)
{
  GbWorkspacePrivate *priv = gb_workspace_get_instance_private (self);

  g_return_if_fail (GB_IS_WORKSPACE (self));

  if (priv->title != title)
    {
      g_free (priv->title);
      priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_TITLE]);
    }
}

static GtkSizeRequestMode
gb_workspace_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gb_workspace_get_preferred_height (GtkWidget *widget,
                                   gint      *min_height,
                                   gint      *nat_height)
{
  /*
   * FIXME: Workaround for GtkStack changes
   *
   * Various changes in Gtk+ is causing the stack to allocate a very large size
   * based on the child requisitions. So we force our natural size to the
   * minimum size so we just fill things in.
   *
   * This can probably be removed as soon as upstream fixes land.
   */
  GTK_WIDGET_CLASS (gb_workspace_parent_class)->get_preferred_height (widget, min_height, nat_height);
  *nat_height = *min_height;
}

static void
gb_workspace_finalize (GObject *object)
{
  GbWorkspace *self = (GbWorkspace *)object;
  GbWorkspacePrivate *priv = gb_workspace_get_instance_private (self);

  g_clear_pointer (&priv->icon_name, g_free);
  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (gb_workspace_parent_class)->finalize (object);
}

static void
gb_workspace_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbWorkspace *self = GB_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, gb_workspace_get_icon_name (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, gb_workspace_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workspace_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GbWorkspace *self = GB_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      gb_workspace_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      gb_workspace_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workspace_class_init (GbWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_workspace_finalize;
  object_class->get_property = gb_workspace_get_property;
  object_class->set_property = gb_workspace_set_property;

  widget_class->get_preferred_height = gb_workspace_get_preferred_height;
  widget_class->get_request_mode = gb_workspace_get_request_mode;

  gParamSpecs[PROP_TITLE] =
    g_param_spec_string ("title",
                         _("Title"),
                         _("The title of the workspace."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  gParamSpecs[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         _("Icon Name"),
                         _("The name of the icon to use."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_workspace_init (GbWorkspace *workspace)
{
}

void
gb_workspace_views_foreach (GbWorkspace *self,
                            GtkCallback  callback,
                            gpointer     callback_data)
{
  g_return_if_fail (GB_IS_WORKSPACE (self));
  g_return_if_fail (callback != NULL);

  if (GB_WORKSPACE_GET_CLASS (self)->views_foreach)
    GB_WORKSPACE_GET_CLASS (self)->views_foreach (self, callback, callback_data);
}
