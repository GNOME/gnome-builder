/* ide-surface.c
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

#define G_LOG_DOMAIN "ide-surface"

#include "config.h"

#include "ide-gui-private.h"
#include "ide-surface.h"

typedef struct
{
  char *icon_name;
  char *title;
} IdeSurfacePrivate;

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeSurface, ide_surface, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_surface_finalize (GObject *object)
{
  IdeSurface *self = (IdeSurface *)object;
  IdeSurfacePrivate *priv = ide_surface_get_instance_private (self);

  g_clear_pointer (&priv->icon_name, g_free);
  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (ide_surface_parent_class)->finalize (object);
}

static void
ide_surface_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeSurface *self = IDE_SURFACE (object);
  IdeSurfacePrivate *priv = ide_surface_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, priv->icon_name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_surface_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeSurface *self = IDE_SURFACE (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      ide_surface_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_surface_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_surface_class_init (IdeSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_surface_finalize;
  object_class->get_property = ide_surface_get_property;
  object_class->set_property = ide_surface_set_property;

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The icon name for the surface",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title for the surface, if any",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_surface_init (IdeSurface *self)
{
}

/**
 * ide_surface_new:
 *
 * Creates a new #IdeSurface.
 *
 * Surfaces contain the main window contents that are placed inside of an
 * #IdeWorkspace (window). You may have multiple surfaces in a workspace,
 * and the user can switch between them.
 *
 * Returns: (transfer full): an #IdeSurface or %NULL
 */
GtkWidget *
ide_surface_new (void)
{
  return g_object_new (IDE_TYPE_SURFACE, NULL);
}

void
ide_surface_set_icon_name (IdeSurface  *self,
                           const gchar *icon_name)
{
  IdeSurfacePrivate *priv = ide_surface_get_instance_private (self);

  g_return_if_fail (IDE_IS_SURFACE (self));

  if (!ide_str_equal0 (priv->icon_name, icon_name))
    {
      g_free (priv->icon_name);
      priv->icon_name = g_strdup (icon_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON_NAME]);
    }
}

void
ide_surface_set_title (IdeSurface  *self,
                       const gchar *title)
{
  IdeSurfacePrivate *priv = ide_surface_get_instance_private (self);

  g_return_if_fail (IDE_IS_SURFACE (self));

  if (!ide_str_equal0 (priv->title, title))
    {
      g_free (priv->title);
      priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

/**
 * ide_surface_foreach_page:
 * @self: a #IdeSurface
 * @callback: (scope call): callback to execute for each page
 * @user_data: closure data for @callback
 *
 * Calls @callback for every page found within the surface @self.
 */
void
ide_surface_foreach_page (IdeSurface      *self,
                          IdePageCallback  callback,
                          gpointer         user_data)
{
  g_return_if_fail (IDE_IS_SURFACE (self));
  g_return_if_fail (callback != NULL);

  if (IDE_SURFACE_GET_CLASS (self)->foreach_page)
    IDE_SURFACE_GET_CLASS (self)->foreach_page (self, callback, user_data);
}

gboolean
ide_surface_agree_to_shutdown (IdeSurface *self)
{
  g_return_val_if_fail (IDE_IS_SURFACE (self), FALSE);

  if (IDE_SURFACE_GET_CLASS (self)->agree_to_shutdown)
    return IDE_SURFACE_GET_CLASS (self)->agree_to_shutdown (self);

  return TRUE;
}

void
_ide_surface_set_fullscreen (IdeSurface *self,
                             gboolean    fullscreen)
{
  g_return_if_fail (IDE_IS_SURFACE (self));

  if (IDE_SURFACE_GET_CLASS (self)->set_fullscreen)
    IDE_SURFACE_GET_CLASS (self)->set_fullscreen (self, fullscreen);
}
