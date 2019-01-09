/* ide-header-bar.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-header-bar"

#include "config.h"

#include <dazzle.h>

#include "ide-gui-private.h"
#include "ide-header-bar.h"

typedef struct
{
  gchar              *menu_id;

  GtkToggleButton    *fullscreen_button;
  GtkImage           *fullscreen_image;
  DzlShortcutTooltip *fullscreen_tooltip;
  DzlMenuButton      *menu_button;
  DzlShortcutTooltip *menu_tooltip;
  GtkBox             *primary;
  GtkBox             *secondary;

  guint               show_fullscreen_button : 1;
} IdeHeaderBarPrivate;

enum {
  PROP_0,
  PROP_MENU_ID,
  PROP_SHOW_FULLSCREEN_BUTTON,
  N_PROPS
};

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeHeaderBar, ide_header_bar, GTK_TYPE_HEADER_BAR,
                         G_ADD_PRIVATE (IdeHeaderBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GParamSpec        *properties [N_PROPS];
static GtkBuildableIface *buildable_parent;

static void
on_fullscreen_toggled_cb (GtkToggleButton *button,
                          GParamSpec      *pspec,
                          GtkImage        *image)
{
  const gchar *icon_name;

  g_assert (GTK_IS_TOGGLE_BUTTON (button));
  g_assert (GTK_IS_IMAGE (image));

  if (gtk_toggle_button_get_active (button))
    icon_name = "view-restore-symbolic";
  else
    icon_name = "view-fullscreen-symbolic";

  g_object_set (image, "icon-name", icon_name, NULL);
}

static void
ide_header_bar_finalize (GObject *object)
{
  IdeHeaderBar *self = (IdeHeaderBar *)object;
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_clear_pointer (&priv->menu_id, g_free);

  G_OBJECT_CLASS (ide_header_bar_parent_class)->finalize (object);
}

static void
ide_header_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeHeaderBar *self = IDE_HEADER_BAR (object);

  switch (prop_id)
    {
    case PROP_MENU_ID:
      g_value_set_string (value, ide_header_bar_get_menu_id (self));
      break;

    case PROP_SHOW_FULLSCREEN_BUTTON:
      g_value_set_boolean (value, ide_header_bar_get_show_fullscreen_button (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_header_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeHeaderBar *self = IDE_HEADER_BAR (object);

  switch (prop_id)
    {
    case PROP_MENU_ID:
      ide_header_bar_set_menu_id (self, g_value_get_string (value));
      break;

    case PROP_SHOW_FULLSCREEN_BUTTON:
      ide_header_bar_set_show_fullscreen_button (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_header_bar_class_init (IdeHeaderBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_header_bar_finalize;
  object_class->get_property = ide_header_bar_get_property;
  object_class->set_property = ide_header_bar_set_property;

  properties [PROP_SHOW_FULLSCREEN_BUTTON] =
    g_param_spec_boolean ("show-fullscreen-button",
                          "Show Fullscreen Button",
                          "If the fullscreen button should be shown",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_MENU_ID] =
    g_param_spec_string ("menu-id",
                         "Menu ID",
                         "The id of the menu to display with the window",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-header-bar.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, fullscreen_button);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, fullscreen_image);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, fullscreen_tooltip);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, menu_button);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, menu_tooltip);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, primary);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, secondary);

  g_type_ensure (DZL_TYPE_PRIORITY_BOX);
  g_type_ensure (DZL_TYPE_SHORTCUT_TOOLTIP);
}

static void
ide_header_bar_init (IdeHeaderBar *self)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (priv->fullscreen_button,
                           "notify::active",
                           G_CALLBACK (on_fullscreen_toggled_cb),
                           priv->fullscreen_image,
                           0);

  _ide_header_bar_init_shortcuts (self);
}

GtkWidget *
ide_header_bar_new (void)
{
  return g_object_new (IDE_TYPE_HEADER_BAR, NULL);
}

/**
 * ide_header_bar_get_show_fullscreen_button:
 * @self: a #IdeHeaderBar
 *
 * Gets if the fullscreen button should be displayed in the header bar.
 *
 * Returns: %TRUE if it should be displayed
 *
 * Since: 3.32
 */
gboolean
ide_header_bar_get_show_fullscreen_button (IdeHeaderBar *self)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_HEADER_BAR (self), FALSE);

  return priv->show_fullscreen_button;
}

/**
 * ide_header_bar_set_show_fullscreen_button:
 * @self: a #IdeHeaderBar
 * @show_fullscreen_button: if the fullscreen button should be displayed
 *
 * Changes the visibility of the fullscreen button.
 *
 * Since: 3.32
 */
void
ide_header_bar_set_show_fullscreen_button (IdeHeaderBar *self,
                                           gboolean      show_fullscreen_button)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_HEADER_BAR (self));

  show_fullscreen_button = !!show_fullscreen_button;

  if (show_fullscreen_button != priv->show_fullscreen_button)
    {
      const gchar *session;

      priv->show_fullscreen_button = show_fullscreen_button;

      session = g_getenv ("DESKTOP_SESSION");
      if (ide_str_equal0 (session, "pantheon"))
        show_fullscreen_button = FALSE;

      gtk_widget_set_visible (GTK_WIDGET (priv->fullscreen_button), show_fullscreen_button);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_FULLSCREEN_BUTTON]);
    }
}

/**
 * ide_header_bar_get_menu_id:
 * @self: a #IdeHeaderBar
 *
 * Gets the menu-id to show in the workspace window.
 *
 * Returns: (nullable): a string containing the menu-id, or %NULL
 *
 * Since: 3.32
 */
const gchar *
ide_header_bar_get_menu_id (IdeHeaderBar *self)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_HEADER_BAR (self), NULL);

  return priv->menu_id;
}

/**
 * ide_header_bar_set_menu_id:
 * @self: a #IdeHeaderBar
 *
 * Sets the menu-id to display in the window.
 *
 * Set to %NULL to hide the workspace menu.
 *
 * Since: 3.32
 */
void
ide_header_bar_set_menu_id (IdeHeaderBar *self,
                            const gchar  *menu_id)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_HEADER_BAR (self));

  if (!ide_str_equal0 (menu_id, priv->menu_id))
    {
      g_free (priv->menu_id);
      priv->menu_id = g_strdup (menu_id);
      g_object_set (priv->menu_button, "menu-id", menu_id, NULL);
      gtk_widget_set_visible (GTK_WIDGET (priv->menu_button), !ide_str_empty0 (menu_id));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MENU_ID]);
    }
}

/**
 * ide_header_bar_add_primary:
 * @self: a #IdeHeaderBar
 *
 * Adds a widget to the primary button section of the workspace header.
 * This is the left, for LTR languages.
 *
 * Since: 3.32
 */
void
ide_header_bar_add_primary (IdeHeaderBar *self,
                            GtkWidget    *widget)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_HEADER_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_container_add (GTK_CONTAINER (priv->primary), widget);
}

void
ide_header_bar_add_center_left (IdeHeaderBar *self,
                                GtkWidget    *child)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_HEADER_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_add_with_properties (GTK_CONTAINER (priv->primary), child,
                                     "pack-type", GTK_PACK_END,
                                     NULL);
}

/**
 * ide_header_bar_add_secondary:
 * @self: a #IdeHeaderBar
 *
 * Adds a widget to the secondary button section of the workspace header.
 * This is the right, for LTR languages.
 *
 * Since: 3.32
 */
void
ide_header_bar_add_secondary (IdeHeaderBar *self,
                              GtkWidget    *widget)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_HEADER_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_container_add (GTK_CONTAINER (priv->secondary), widget);
}

void
_ide_header_bar_show_menu (IdeHeaderBar *self)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_HEADER_BAR (self));

  gtk_widget_activate (GTK_WIDGET (priv->menu_button));
}

static void
ide_header_bar_add_child (GtkBuildable  *buildable,
                          GtkBuilder    *builder,
                          GObject       *child,
                          const gchar   *type)
{
  IdeHeaderBar *self = (IdeHeaderBar *)buildable;
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_assert (IDE_IS_HEADER_BAR (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (child));

  if (ide_str_equal0 (type, "left-of-center"))
    {
      if (GTK_IS_WIDGET (child))
        {
          gtk_container_add_with_properties (GTK_CONTAINER (priv->primary), GTK_WIDGET (child),
                                             "pack-type", GTK_PACK_END,
                                             NULL);
          return;
        }

      goto warning;
    }

  if (ide_str_equal0 (type, "left") || ide_str_equal0 (type, "primary"))
    {
      if (GTK_IS_WIDGET (child))
        {
          gtk_container_add_with_properties (GTK_CONTAINER (priv->primary), GTK_WIDGET (child),
                                             "pack-type", GTK_PACK_START,
                                             NULL);
          return;
        }

      goto warning;
    }

  if (ide_str_equal0 (type, "right-of-center"))
    {
      if (GTK_IS_WIDGET (child))
        {
          gtk_container_add_with_properties (GTK_CONTAINER (priv->secondary), GTK_WIDGET (child),
                                             "pack-type", GTK_PACK_START,
                                             NULL);
          return;
        }

      goto warning;
    }

  if (ide_str_equal0 (type, "right") || ide_str_equal0 (type, "secondary"))
    {
      if (GTK_IS_WIDGET (child))
        {
          gtk_container_add_with_properties (GTK_CONTAINER (priv->secondary), GTK_WIDGET (child),
                                             "pack-type", GTK_PACK_END,
                                             NULL);
          return;
        }

      goto warning;
    }

  buildable_parent->add_child (buildable, builder, child, type);

  return;

warning:
  g_warning ("'%s' child type must be a GtkWidget, not %s",
             type, G_OBJECT_TYPE_NAME (child));
}

static GObject *
ide_header_bar_get_internal_child (GtkBuildable *buildable,
                                   GtkBuilder   *builder,
                                   const gchar  *child_name)
{
  IdeHeaderBar *self = (IdeHeaderBar *)buildable;
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_assert (IDE_IS_HEADER_BAR (self));
  g_assert (GTK_IS_BUILDER (builder));

  if (ide_str_equal0 (child_name, "primary"))
    return G_OBJECT (priv->primary);

  if (ide_str_equal0 (child_name, "secondary"))
    return G_OBJECT (priv->secondary);

  if (buildable_parent->get_internal_child)
    return buildable_parent->get_internal_child (buildable, builder, child_name);

  return NULL;
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  buildable_parent = g_type_interface_peek_parent (iface);
  iface->add_child = ide_header_bar_add_child;
  iface->get_internal_child = ide_header_bar_get_internal_child;
}
