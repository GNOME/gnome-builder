/* ide-tree-expander.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tree-expander"

#include "config.h"

#include "ide-tree-expander.h"

struct _IdeTreeExpander
{
  GtkWidget       parent_instance;

  GtkWidget      *image;
  GtkWidget      *title;
  GtkWidget      *suffix;

  GMenuModel     *menu_model;

  GtkTreeListRow *list_row;

  const char     *icon_name;
  const char     *expanded_icon_name;

  gulong          list_row_notify_depth;
  gulong          list_row_notify_expanded;
};

enum {
  PROP_0,
  PROP_EXPANDED,
  PROP_EXPANDED_ICON_NAME,
  PROP_ICON_NAME,
  PROP_ITEM,
  PROP_LIST_ROW,
  PROP_MENU_MODEL,
  PROP_SUFFIX,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTreeExpander, ide_tree_expander, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_tree_expander_update_depth (IdeTreeExpander *self)
{
  static GType builtin_icon_type = G_TYPE_INVALID;
  guint depth;

  g_assert (IDE_IS_TREE_EXPANDER (self));

  if (self->list_row != NULL)
    depth = gtk_tree_list_row_get_depth (self->list_row);
  else
    depth = 0;

  for (;;)
    {
      GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self));

      if (child == self->image)
        break;

      gtk_widget_unparent (child);
    }

  if (builtin_icon_type == G_TYPE_INVALID)
    builtin_icon_type = g_type_from_name ("GtkBuiltinIcon");

  for (guint i = 0; i < depth; i++)
    {
      GtkWidget *child;

      child = g_object_new (builtin_icon_type,
                            "css-name", "indent",
                            "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                            NULL);
      gtk_widget_insert_after (child, GTK_WIDGET (self), NULL);
    }

  /* The level property is >= 1 */
  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LEVEL, depth + 1,
                                  -1);
}

static void
ide_tree_expander_update_icon (IdeTreeExpander *self)
{
  const char *icon_name;

  g_assert (IDE_IS_TREE_EXPANDER (self));

  if (self->list_row != NULL && gtk_tree_list_row_get_expanded (self->list_row))
    icon_name = self->expanded_icon_name ? self->expanded_icon_name : self->icon_name;
  else
    icon_name = self->icon_name;

  gtk_image_set_from_icon_name (GTK_IMAGE (self->image), icon_name);
}

static void
ide_tree_expander_notify_depth_cb (IdeTreeExpander *self,
                                   GParamSpec      *pspec,
                                   GtkTreeListRow  *list_row)
{
  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (GTK_IS_TREE_LIST_ROW (list_row));

  ide_tree_expander_update_depth (self);
}

static void
ide_tree_expander_notify_expanded_cb (IdeTreeExpander *self,
                                      GParamSpec      *pspec,
                                      GtkTreeListRow  *list_row)
{
  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (GTK_IS_TREE_LIST_ROW (list_row));

  ide_tree_expander_update_icon (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXPANDED]);
}

static void
ide_tree_expander_click_pressed_cb (IdeTreeExpander *self,
                                    int              n_press,
                                    double           x,
                                    double           y,
                                    GtkGestureClick *click)
{
  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (n_press != 1 ||
      self->list_row == NULL ||
      !gtk_tree_list_row_is_expandable (self->list_row))
    return;

  gtk_widget_activate_action (GTK_WIDGET (self), "listitem.toggle-expand", NULL);

  gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE, FALSE);
  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
ide_tree_expander_click_released_cb (IdeTreeExpander *self,
                                     int              n_press,
                                     double           x,
                                     double           y,
                                     GtkGestureClick *click)
{
  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (n_press != 1 ||
      self->list_row == NULL ||
      !gtk_tree_list_row_is_expandable (self->list_row))
    return;

  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);
  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
ide_tree_expander_click_cancel_cb (IdeTreeExpander  *self,
                                   GdkEventSequence *sequence,
                                   GtkGestureClick  *click)
{
  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);
  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
ide_tree_expander_toggle_expand (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *parameter)
{
  IdeTreeExpander *self = (IdeTreeExpander *)widget;

  g_assert (IDE_IS_TREE_EXPANDER (self));

  if (self->list_row == NULL)
    return;

  gtk_tree_list_row_set_expanded (self->list_row,
                                  !gtk_tree_list_row_get_expanded (self->list_row));
}

static void
ide_tree_expander_dispose (GObject *object)
{
  IdeTreeExpander *self = (IdeTreeExpander *)object;

  ide_tree_expander_set_list_row (self, NULL);

  g_clear_pointer (&self->image, gtk_widget_unparent);
  g_clear_pointer (&self->title, gtk_widget_unparent);
  g_clear_pointer (&self->suffix, gtk_widget_unparent);

  g_clear_object (&self->list_row);
  g_clear_object (&self->menu_model);

  G_OBJECT_CLASS (ide_tree_expander_parent_class)->dispose (object);
}

static void
ide_tree_expander_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTreeExpander *self = IDE_TREE_EXPANDER (object);

  switch (prop_id)
    {
    case PROP_EXPANDED:
      g_value_set_boolean (value, gtk_tree_list_row_get_expanded (self->list_row));
      break;

    case PROP_EXPANDED_ICON_NAME:
      g_value_set_string (value, ide_tree_expander_get_expanded_icon_name (self));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, ide_tree_expander_get_icon_name (self));
      break;

    case PROP_ITEM:
      g_value_take_object (value, ide_tree_expander_get_item (self));
      break;

    case PROP_LIST_ROW:
      g_value_set_object (value, ide_tree_expander_get_list_row (self));
      break;

    case PROP_MENU_MODEL:
      g_value_set_object (value, ide_tree_expander_get_menu_model (self));
      break;

    case PROP_SUFFIX:
      g_value_set_object (value, ide_tree_expander_get_suffix (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tree_expander_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_expander_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTreeExpander *self = IDE_TREE_EXPANDER (object);

  switch (prop_id)
    {
    case PROP_EXPANDED_ICON_NAME:
      ide_tree_expander_set_expanded_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      ide_tree_expander_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_LIST_ROW:
      ide_tree_expander_set_list_row (self, g_value_get_object (value));
      break;

    case PROP_MENU_MODEL:
      ide_tree_expander_set_menu_model (self, g_value_get_object (value));
      break;

    case PROP_SUFFIX:
      ide_tree_expander_set_suffix (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      ide_tree_expander_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_expander_class_init (IdeTreeExpanderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_tree_expander_dispose;
  object_class->get_property = ide_tree_expander_get_property;
  object_class->set_property = ide_tree_expander_set_property;

  properties [PROP_EXPANDED] =
    g_param_spec_boolean ("expanded", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_EXPANDED_ICON_NAME] =
    g_param_spec_string ("expanded-icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_LIST_ROW] =
    g_param_spec_object ("list-row", NULL, NULL,
                         GTK_TYPE_TREE_LIST_ROW,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_MENU_MODEL] =
    g_param_spec_object ("menu-model", NULL, NULL,
                         G_TYPE_MENU_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SUFFIX] =
    g_param_spec_object ("suffix", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "treeexpander");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);

  gtk_widget_class_install_action (widget_class, "listitem.toggle-expand", NULL, ide_tree_expander_toggle_expand);
}

static void
ide_tree_expander_init (IdeTreeExpander *self)
{
  GtkEventController *controller;

  self->image = g_object_new (GTK_TYPE_IMAGE, NULL);
  gtk_widget_insert_after (self->image, GTK_WIDGET (self), NULL);

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "halign", GTK_ALIGN_START,
                              "ellipsize", PANGO_ELLIPSIZE_END,
                              "margin-start", 3,
                              "margin-end", 3,
                              NULL);
  gtk_widget_insert_after (self->title, GTK_WIDGET (self), self->image);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect_object (controller,
                           "pressed",
                           G_CALLBACK (ide_tree_expander_click_pressed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (controller,
                           "released",
                           G_CALLBACK (ide_tree_expander_click_released_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (controller,
                           "cancel",
                           G_CALLBACK (ide_tree_expander_click_cancel_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

GtkWidget *
ide_tree_expander_new (void)
{
  return g_object_new (IDE_TYPE_TREE_EXPANDER, NULL);
}

/**
 * ide_tree_expander_get_item:
 * @self: a #IdeTreeExpander
 *
 * Gets the item instance from the model.
 *
 * Returns: (transfer full) (nullable) (type GObject): a #GObject or %NULL
 */
gpointer
ide_tree_expander_get_item (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  if (self->list_row == NULL)
    return NULL;

  return gtk_tree_list_row_get_item (self->list_row);
}

/**
 * ide_tree_expander_get_menu_model:
 * @self: a #IdeTreeExpander
 *
 * Sets the menu model to use for context menus.
 *
 * Returns: (transfer none) (nullable): a #GMenuModel or %NULL
 */
GMenuModel *
ide_tree_expander_get_menu_model (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  return self->menu_model;
}

void
ide_tree_expander_set_menu_model (IdeTreeExpander *self,
                                  GMenuModel      *menu_model)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));
  g_return_if_fail (!menu_model || G_IS_MENU_MODEL (menu_model));

  if (g_set_object (&self->menu_model, menu_model))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MENU_MODEL]);
}

const char *
ide_tree_expander_get_expanded_icon_name (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  return self->expanded_icon_name;
}

void
ide_tree_expander_set_expanded_icon_name (IdeTreeExpander *self,
                                          const char      *expanded_icon_name)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));

  if (!ide_str_equal0 (self->expanded_icon_name, expanded_icon_name))
    {
      self->expanded_icon_name = g_intern_string (expanded_icon_name);
      ide_tree_expander_update_icon (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPANDED_ICON_NAME]);
    }
}

const char *
ide_tree_expander_get_icon_name (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  return self->icon_name;
}

void
ide_tree_expander_set_icon_name (IdeTreeExpander *self,
                                 const char      *icon_name)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));

  if (!ide_str_equal0 (self->icon_name, icon_name))
    {
      self->icon_name = g_intern_string (icon_name);
      ide_tree_expander_update_icon (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_NAME]);
    }
}

/**
 * ide_tree_expander_get_suffix:
 * @self: a #IdeTreeExpander
 *
 * Get the suffix widget, if any.
 *
 * Returns: (transfer none) (nullable): a #GtkWidget
 */
GtkWidget *
ide_tree_expander_get_suffix (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  return self->suffix;
}

void
ide_tree_expander_set_suffix (IdeTreeExpander *self,
                              GtkWidget       *suffix)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));
  g_return_if_fail (!suffix || GTK_IS_WIDGET (suffix));

  if (self->suffix == suffix)
    return;

  g_clear_pointer (&self->suffix, gtk_widget_unparent);

  self->suffix = suffix;

  if (self->suffix)
    gtk_widget_insert_before (suffix, GTK_WIDGET (self), NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SUFFIX]);
}

const char *
ide_tree_expander_get_title (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  return gtk_label_get_label (GTK_LABEL (self->title));
}

void
ide_tree_expander_set_title (IdeTreeExpander *self,
                             const char      *title)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));

  if (!ide_str_equal0 (title, ide_tree_expander_get_title (self)))
    {
      gtk_label_set_label (GTK_LABEL (self->title), title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

/**
 * ide_tree_expander_get_list_row:
 * @self: a #IdeTreeExpander
 *
 * Gets the list row for the expander.
 *
 * Returns: (transfer none) (nullable): a #GtkTreeListRow or %NULL
 */
GtkTreeListRow *
ide_tree_expander_get_list_row (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  return self->list_row;
}

void
ide_tree_expander_set_list_row (IdeTreeExpander *self,
                                GtkTreeListRow  *list_row)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));
  g_return_if_fail (!list_row || GTK_IS_TREE_LIST_ROW (list_row));

  if (self->list_row == list_row)
    return;

  if (self->list_row != NULL)
    {
      g_clear_signal_handler (&self->list_row_notify_depth, self->list_row);
      g_clear_signal_handler (&self->list_row_notify_expanded, self->list_row);
    }

  g_set_object (&self->list_row, list_row);

  if (self->list_row != NULL)
    {
      self->list_row_notify_expanded = g_signal_connect_object (self->list_row,
                                                                "notify::expanded",
                                                                G_CALLBACK (ide_tree_expander_notify_expanded_cb),
                                                                self,
                                                                G_CONNECT_SWAPPED);
      self->list_row_notify_depth = g_signal_connect_object (self->list_row,
                                                             "notify::depth",
                                                             G_CALLBACK (ide_tree_expander_notify_depth_cb),
                                                             self,
                                                             G_CONNECT_SWAPPED);
    }

  ide_tree_expander_update_depth (self);
  ide_tree_expander_update_icon (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIST_ROW]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}
