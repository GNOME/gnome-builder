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

#include "ide-application.h"
#include "ide-header-bar.h"

typedef struct
{
  char *menu_id;

  AdwHeaderBar *header_bar;
  GtkMenuButton *menu_button;
  GtkCenterBox *center_box;
  GtkBox *left;
  GtkBox *left_of_center;
  GtkBox *right;
  GtkBox *right_of_center;
} IdeHeaderBarPrivate;

enum {
  PROP_0,
  PROP_MENU_ID,
  PROP_SHOW_START_TITLE_BUTTONS,
  PROP_SHOW_END_TITLE_BUTTONS,
  PROP_SHOW_MENU,
  N_PROPS
};

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeHeaderBar, ide_header_bar, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (IdeHeaderBar)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GtkBuildableIface *buildable_parent_iface;
static GParamSpec *properties [N_PROPS];

static void
ide_header_bar_dispose (GObject *object)
{
  IdeHeaderBar *self = (IdeHeaderBar *)object;
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_clear_pointer (&priv->menu_id, g_free);
  g_clear_pointer ((GtkWidget **)&priv->header_bar, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_header_bar_parent_class)->dispose (object);
}

static void
ide_header_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeHeaderBar *self = IDE_HEADER_BAR (object);
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_MENU_ID:
      g_value_set_string (value, ide_header_bar_get_menu_id (self));
      break;

    case PROP_SHOW_START_TITLE_BUTTONS:
      g_value_set_boolean (value, adw_header_bar_get_show_start_title_buttons (priv->header_bar));
      break;

    case PROP_SHOW_END_TITLE_BUTTONS:
      g_value_set_boolean (value, adw_header_bar_get_show_end_title_buttons (priv->header_bar));
      break;

    case PROP_SHOW_MENU:
      g_value_set_boolean (value, gtk_widget_get_visible (GTK_WIDGET (priv->menu_button)));
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
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_MENU_ID:
      ide_header_bar_set_menu_id (self, g_value_get_string (value));
      break;

    case PROP_SHOW_START_TITLE_BUTTONS:
      adw_header_bar_set_show_start_title_buttons (priv->header_bar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_END_TITLE_BUTTONS:
      adw_header_bar_set_show_end_title_buttons (priv->header_bar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_MENU:
      gtk_widget_set_visible (GTK_WIDGET (priv->menu_button), g_value_get_boolean (value));
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

  object_class->dispose = ide_header_bar_dispose;
  object_class->get_property = ide_header_bar_get_property;
  object_class->set_property = ide_header_bar_set_property;

  properties [PROP_MENU_ID] =
    g_param_spec_string ("menu-id",
                         "Menu ID",
                         "The id of the menu to display with the window",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_START_TITLE_BUTTONS] =
    g_param_spec_boolean ("show-start-title-buttons", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_END_TITLE_BUTTONS] =
    g_param_spec_boolean ("show-end-title-buttons", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_MENU] =
    g_param_spec_boolean ("show-menu", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-header-bar.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, center_box);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, header_bar);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, left);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, left_of_center);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, menu_button);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, right);
  gtk_widget_class_bind_template_child_private (widget_class, IdeHeaderBar, right_of_center);
}

static void
ide_header_bar_init (IdeHeaderBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_header_bar_new (void)
{
  return g_object_new (IDE_TYPE_HEADER_BAR, NULL);
}

/**
 * ide_header_bar_get_menu_id:
 * @self: a #IdeHeaderBar
 *
 * Gets the menu-id to show in the workspace window.
 *
 * Returns: (nullable): a string containing the menu-id, or %NULL
 */
const gchar *
ide_header_bar_get_menu_id (IdeHeaderBar *self)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_HEADER_BAR (self), NULL);

  return priv->menu_id;
}

static gboolean
menu_has_custom (GMenuModel *model,
                 const char *name)
{
  guint n_items;

  if (model == NULL || name == NULL)
    return FALSE;

  n_items = g_menu_model_get_n_items (model);
  for (int i = 0; i < n_items; i++)
    {
      g_autofree char *custom = NULL;
      g_autoptr(GMenuModel) section = NULL;

      if (g_menu_model_get_item_attribute (model, i, "custom", "s", &custom) &&
          g_strcmp0 (custom, name) == 0)
        return TRUE;

      if ((section = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION)) &&
          menu_has_custom (section, name))
        return TRUE;
    }

  return FALSE;
}

void
ide_header_bar_setup_menu (GtkPopoverMenu *popover)
{
  GMenuModel *model;

  g_return_if_fail (GTK_IS_POPOVER_MENU (popover));

  if (!(model = gtk_popover_menu_get_menu_model (popover)))
    return;

  if (menu_has_custom (model, "theme_selector"))
    gtk_popover_menu_add_child (popover,
                                g_object_new (PANEL_TYPE_THEME_SELECTOR,
                                              "action-name", "app.style-variant",
                                              NULL),
                                "theme_selector");
}

/**
 * ide_header_bar_set_menu_id:
 * @self: a #IdeHeaderBar
 *
 * Sets the menu-id to display in the window.
 *
 * Set to %NULL to hide the workspace menu.
 */
void
ide_header_bar_set_menu_id (IdeHeaderBar *self,
                            const char   *menu_id)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  g_return_if_fail (IDE_IS_HEADER_BAR (self));

  if (g_set_str (&priv->menu_id, menu_id))
    {
      GtkPopover *popover;
      GMenu *menu = NULL;

      if (menu_id != NULL)
        menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, menu_id);

      g_object_set (priv->menu_button, "menu-model", menu, NULL);
      gtk_widget_set_visible (GTK_WIDGET (priv->menu_button), !ide_str_empty0 (menu_id));

      popover = gtk_menu_button_get_popover (priv->menu_button);
      ide_header_bar_setup_menu (GTK_POPOVER_MENU (popover));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MENU_ID]);
    }
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

  if (ADW_IS_HEADER_BAR (child) && priv->header_bar == NULL)
    {
      buildable_parent_iface->add_child (buildable, builder, child, type);
      return;
    }

  if (GTK_IS_WIDGET (child))
    {
      if (ide_str_equal0 (type, "title"))
        gtk_center_box_set_center_widget (priv->center_box, GTK_WIDGET (child));
      else if (ide_str_equal0 (type, "left"))
        ide_header_bar_add (self, IDE_HEADER_BAR_POSITION_LEFT, 0, GTK_WIDGET (child));
      else if (ide_str_equal0 (type, "right"))
        ide_header_bar_add (self, IDE_HEADER_BAR_POSITION_RIGHT, 0, GTK_WIDGET (child));
      else if (ide_str_equal0 (type, "left-of-center"))
        ide_header_bar_add (self, IDE_HEADER_BAR_POSITION_LEFT_OF_CENTER, 0, GTK_WIDGET (child));
      else if (ide_str_equal0 (type, "right-of-center"))
        ide_header_bar_add (self, IDE_HEADER_BAR_POSITION_RIGHT_OF_CENTER, 0, GTK_WIDGET (child));
      else
        goto failure;

      return;
    }

failure:
  g_warning ("No such child \"%s\" for child of type %s",
             type ? type : "NULL", G_OBJECT_TYPE_NAME (child));

}

static GObject *
ide_header_bar_get_internal_child (GtkBuildable *buildable,
                                   GtkBuilder   *builder,
                                   const char   *name)
{
  IdeHeaderBar *self = (IdeHeaderBar *)buildable;
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);

  if (g_strcmp0 (name, "headerbar") == 0)
    return G_OBJECT (priv->header_bar);

  return buildable_parent_iface->get_internal_child (buildable, builder, name);
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  buildable_parent_iface = g_type_interface_peek_parent (iface);

  iface->add_child = ide_header_bar_add_child;
  iface->get_internal_child = ide_header_bar_get_internal_child;
}

#define GET_PRIORITY(w)   GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"PRIORITY"))
#define SET_PRIORITY(w,i) g_object_set_data(G_OBJECT(w),"PRIORITY",GINT_TO_POINTER(i))

void
ide_header_bar_add (IdeHeaderBar         *self,
                    IdeHeaderBarPosition  position,
                    int                   priority,
                    GtkWidget            *widget)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);
  GtkBox *box;
  gboolean append;

  g_return_if_fail (IDE_IS_HEADER_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (position < IDE_HEADER_BAR_POSITION_LAST);

  SET_PRIORITY (widget, priority);

  switch (position)
    {
    case IDE_HEADER_BAR_POSITION_LEFT:
      box = priv->left;
      append = TRUE;
      break;

    case IDE_HEADER_BAR_POSITION_RIGHT:
      box = priv->right;
      append = FALSE;
      break;

    case IDE_HEADER_BAR_POSITION_LEFT_OF_CENTER:
      box = priv->left_of_center;
      append = FALSE;
      break;

    case IDE_HEADER_BAR_POSITION_RIGHT_OF_CENTER:
      box = priv->right_of_center;
      append = TRUE;
      break;

    case IDE_HEADER_BAR_POSITION_LAST:
    default:
      g_assert_not_reached ();
    }

  if (append)
    {
      GtkWidget *sibling = NULL;

      for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (box));
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        {
          if (priority < GET_PRIORITY (child))
            break;
          sibling = child;
        }

      gtk_box_insert_child_after (box, widget, sibling);
    }
  else
    {
      GtkWidget *sibling = NULL;

      for (GtkWidget *child = gtk_widget_get_last_child (GTK_WIDGET (box));
           child != NULL;
           child = gtk_widget_get_prev_sibling (child))
        {
          if (priority < GET_PRIORITY (child))
            break;
          sibling = child;
        }

      gtk_box_insert_child_after (box, widget, sibling);
    }
}

void
ide_header_bar_remove (IdeHeaderBar *self,
                       GtkWidget    *widget)
{
  IdeHeaderBarPrivate *priv = ide_header_bar_get_instance_private (self);
  GtkBox *box;

  g_return_if_fail (IDE_IS_HEADER_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_ancestor (widget, IDE_TYPE_HEADER_BAR) == GTK_WIDGET (self));

  box = GTK_BOX (gtk_widget_get_ancestor (widget, GTK_TYPE_BOX));

  if (box == priv->left ||
      box == priv->right ||
      box == priv->left_of_center ||
      box == priv->right_of_center)
    {
      gtk_box_remove (box, widget);
      return;
    }

  g_warning ("Failed to locate widget of type %s within headerbar",
             G_OBJECT_TYPE_NAME (widget));
}
