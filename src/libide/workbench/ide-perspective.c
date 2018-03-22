/* ide-perspective.c
 *
 * Copyright 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-perspective"

#include "config.h"

#include "workbench/ide-perspective.h"

G_DEFINE_INTERFACE (IdePerspective, ide_perspective, G_TYPE_OBJECT)

static gboolean
ide_perspective_real_agree_to_shutdown (IdePerspective *self)
{
  return TRUE;
}

static gchar *
ide_perspective_real_get_icon_name (IdePerspective *self)
{
  return NULL;
}

static gchar *
ide_perspective_real_get_id (IdePerspective *self)
{
  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_perspective_real_get_needs_attention (IdePerspective *self)
{
  return FALSE;
}

static gchar *
ide_perspective_real_get_title (IdePerspective *self)
{
  return NULL;
}

static GtkWidget *
ide_perspective_real_get_titlebar (IdePerspective *self)
{
  return NULL;
}

static void
ide_perspective_real_set_fullscreen (IdePerspective *self,
                                     gboolean        fullscreen)
{
}

static void
ide_perspective_real_views_foreach (IdePerspective *self,
                                    GtkCallback     callback,
                                    gpointer        user_data)
{
}

static void
ide_perspective_default_init (IdePerspectiveInterface *iface)
{
  iface->agree_to_shutdown = ide_perspective_real_agree_to_shutdown;
  iface->get_icon_name = ide_perspective_real_get_icon_name;
  iface->get_id = ide_perspective_real_get_id;
  iface->get_needs_attention = ide_perspective_real_get_needs_attention;
  iface->get_title = ide_perspective_real_get_title;
  iface->get_titlebar = ide_perspective_real_get_titlebar;
  iface->set_fullscreen = ide_perspective_real_set_fullscreen;
  iface->views_foreach = ide_perspective_real_views_foreach;
}

/**
 * ide_perspective_agree_to_shutdown:
 * @self: An #IdePerspective.
 *
 * This interface method is called when the workbench would like to shutdown.
 * If the perspective needs to focus and ask the user a question, this is the place
 * to do so. You may run a #GtkDialog using gtk_dialog_run() or simply focus your
 * perspective and return %FALSE.
 *
 * Returns: %TRUE to allow the workbench to continue shutting down.
 */
gboolean
ide_perspective_agree_to_shutdown (IdePerspective *self)
{
  g_return_val_if_fail (IDE_IS_PERSPECTIVE (self), FALSE);

  return IDE_PERSPECTIVE_GET_IFACE (self)->agree_to_shutdown (self);
}

/**
 * ide_perspective_get_icon_name:
 * @self: An #IdePerspective.
 *
 * This interface methods retrieves the icon name to use when displaying the
 * perspective selection sidebar.
 *
 * If you implement an "icon-name" property, the icon may change at runtime.
 *
 * Returns: (nullable): A newly allcoated string that contains the icon-name
 *   to use for the perspective.
 */
gchar *
ide_perspective_get_icon_name (IdePerspective *self)
{
  g_return_val_if_fail (IDE_IS_PERSPECTIVE (self), NULL);

  return IDE_PERSPECTIVE_GET_IFACE (self)->get_icon_name (self);
}

/**
 * ide_perspective_get_id:
 * @self: An #IdePerspective
 *
 * This interface method is used to identify the perspective. It should be a short
 * internal name, such as "editor" which should not be translated. Internally, the
 * default implementation of this method will return the name of the instances #GType.
 *
 * The identifier must be alpha-numeric only (a-z A-Z 0-9).
 *
 * This value should be unique per workspace.
 *
 * Returns: (nullable): A string identifier for the perspective.
 */
gchar *
ide_perspective_get_id (IdePerspective *self)
{
  gchar *ret;

  g_return_val_if_fail (IDE_IS_PERSPECTIVE (self), NULL);

  ret = IDE_PERSPECTIVE_GET_IFACE (self)->get_id (self);

  g_return_val_if_fail (g_str_is_ascii (ret), NULL);

  return ret;
}

/**
 * ide_perspective_get_needs_attention:
 * @self: An #IdePerspective.
 *
 * This interface method returns %TRUE if the interface needs attention.
 *
 * One such use of this would be to indicate that contents within a perspective have
 * changed since the user last focused the perspective. This should also be implemented
 * with a boolean property named "needs-attention". If you call g_object_notify() (or one
 * of its variants), the notifcation visual will be rendered with your icon.
 *
 * Returns: %TRUE if the perspective needs attention.
 */
gboolean
ide_perspective_get_needs_attention (IdePerspective *self)
{
  g_return_val_if_fail (IDE_IS_PERSPECTIVE (self), FALSE);

  return IDE_PERSPECTIVE_GET_IFACE (self)->get_needs_attention (self);
}

/**
 * ide_perspective_get_title:
 * @self: An #IdePerspective
 *
 * This interface method gets the title of the perspective. This is used for tooltips
 * in the perspective selector and potentially other UI components.
 *
 * Returns: A string which will not be modified or freed.
 */
gchar *
ide_perspective_get_title (IdePerspective *self)
{
  g_return_val_if_fail (IDE_IS_PERSPECTIVE (self), NULL);

  return IDE_PERSPECTIVE_GET_IFACE (self)->get_title (self);
}

/**
 * ide_perspective_get_titlebar:
 * @self: An #IdePerspective.
 *
 * This interface method should return a #GtkWidget suitable for being embedded as the
 * titlebar for the application. If you return %NULL from this method, a suitable titlebar
 * will be created for you.
 *
 * You may use #IdeHeaderBar for a base implementation to save you the trouble of
 * creating a titlebar similar to other perspectives in Builder.
 *
 * Returns: (transfer none) (nullable): a #GtkWidget or %NULL.
 */
GtkWidget *
ide_perspective_get_titlebar (IdePerspective *self)
{
  g_return_val_if_fail (IDE_IS_PERSPECTIVE (self), NULL);

  return IDE_PERSPECTIVE_GET_IFACE (self)->get_titlebar (self);
}

/**
 * ide_perspective_set_fullscreen:
 * @self: An #IdePerspective.
 * @fullscreen: If fullscreen mode should be activated.
 *
 * This interface method is used to notify the perspective that it is going into
 * fullscreen mode. The #IdeWorkbench will notify the perspective before it is displayed.
 */
void
ide_perspective_set_fullscreen (IdePerspective *self,
                                gboolean        fullscreen)
{
  g_return_if_fail (IDE_IS_PERSPECTIVE (self));

  IDE_PERSPECTIVE_GET_IFACE (self)->set_fullscreen (self, fullscreen);
}

/**
 * ide_perspective_views_foreach:
 * @self: An #IdePerspective.
 * @callback: (scope call): a #GtkCallback.
 * @user_data: user data for @callback.
 *
 * This interface method is used to iterate all #IdeLayoutView's that are descendents of @self.
 */
void
ide_perspective_views_foreach (IdePerspective *self,
                               GtkCallback     callback,
                               gpointer        user_data)
{
  g_return_if_fail (IDE_IS_PERSPECTIVE (self));
  g_return_if_fail (callback != NULL);

  IDE_PERSPECTIVE_GET_IFACE (self)->views_foreach (self, callback, user_data);
}

/**
 * ide_perspective_is_early:
 *
 * If %TRUE, the perspective can be used before loading a project.
 */
gboolean
ide_perspective_is_early (IdePerspective *self)
{
  g_return_val_if_fail (IDE_IS_PERSPECTIVE (self), FALSE);

  if (IDE_PERSPECTIVE_GET_IFACE (self)->is_early)
    return IDE_PERSPECTIVE_GET_IFACE (self)->is_early (self);
  return FALSE;
}

/**
 * ide_perspective_get_accelerator:
 *
 * Gets the accelerator to use to jump to the perspective. The workbench will
 * register this accelerator on behalf of the perspective.
 *
 * Returns: (nullable) (transfer full): A newly allocated string or %NULL.
 */
gchar *
ide_perspective_get_accelerator (IdePerspective *self)
{
  g_return_val_if_fail (IDE_IS_PERSPECTIVE (self), NULL);

  if (IDE_PERSPECTIVE_GET_IFACE (self)->get_accelerator)
   return IDE_PERSPECTIVE_GET_IFACE (self)->get_accelerator (self);

  return NULL;
}

void
ide_perspective_restore_state (IdePerspective *self)
{
  g_return_if_fail (IDE_IS_PERSPECTIVE (self));

  if (IDE_PERSPECTIVE_GET_IFACE (self)->restore_state)
    IDE_PERSPECTIVE_GET_IFACE (self)->restore_state (self);
}
