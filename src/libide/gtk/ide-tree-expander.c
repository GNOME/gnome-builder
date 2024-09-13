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
  GtkWidget        parent_instance;

  GtkWidget       *image;
  GtkWidget       *title;
  GtkWidget       *suffix;

  GMenuModel      *menu_model;

  GtkTreeListRow  *list_row;

  GIcon           *icon;
  GIcon           *expanded_icon;

  GtkPopover      *popover;

  gulong           list_row_notify_expanded;
};

enum {
  PROP_0,
  PROP_EXPANDED,
  PROP_EXPANDED_ICON,
  PROP_EXPANDED_ICON_NAME,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_ITEM,
  PROP_LIST_ROW,
  PROP_MENU_MODEL,
  PROP_SUFFIX,
  PROP_TITLE,
  PROP_IGNORED,
  PROP_USE_MARKUP,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTreeExpander, ide_tree_expander, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_tree_expander_update_depth (IdeTreeExpander *self)
{
  static GType builtin_icon_type = G_TYPE_INVALID;
  GtkWidget *child;
  guint depth;

  g_assert (IDE_IS_TREE_EXPANDER (self));

  if (self->list_row != NULL)
    depth = gtk_tree_list_row_get_depth (self->list_row);
  else
    depth = 0;

  if G_UNLIKELY (builtin_icon_type == G_TYPE_INVALID)
    builtin_icon_type = g_type_from_name ("GtkBuiltinIcon");

  child = gtk_widget_get_prev_sibling (self->image);

  while (child)
    {
      GtkWidget *prev = gtk_widget_get_prev_sibling (child);
      g_assert (G_TYPE_CHECK_INSTANCE_TYPE (child, builtin_icon_type));
      gtk_widget_unparent (child);
      child = prev;
    }

  for (guint i = 0; i < depth; i++)
    {
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
  GIcon *icon = NULL;

  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (gtk_widget_get_parent (self->image) == GTK_WIDGET (self));

  if (self->list_row != NULL)
    {
      if (gtk_tree_list_row_get_expanded (self->list_row))
        icon = self->expanded_icon ? self->expanded_icon : self->icon;
      else
        icon = self->icon;
    }

  gtk_image_set_from_gicon (GTK_IMAGE (self->image), icon);
}

static void
ide_tree_expander_update_expanded_state (IdeTreeExpander *self,
                                         GtkTreeListRow  *list_row)
{
  gboolean expanded;

  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (GTK_IS_TREE_LIST_ROW (list_row));

  ide_tree_expander_update_icon (self);

  if (gtk_tree_list_row_is_expandable (list_row))
    {
      expanded = gtk_tree_list_row_get_expanded (list_row);
      gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_STATE_EXPANDED, expanded,
                                  -1);
    }
}

static void
ide_tree_expander_notify_expanded_cb (IdeTreeExpander *self,
                                      GParamSpec      *pspec,
                                      GtkTreeListRow  *list_row)
{
  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (GTK_IS_TREE_LIST_ROW (list_row));

  ide_tree_expander_update_expanded_state (self, list_row);

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

  if (n_press != 1 || self->list_row == NULL)
    return;

  gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE, FALSE);
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

  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_ACTIVE);

  if (n_press != 1 ||
      self->list_row == NULL ||
      !gtk_tree_list_row_is_expandable (self->list_row))
    return;


  gtk_widget_activate_action (GTK_WIDGET (self), "listitem.select", "(bb)", FALSE, FALSE);
  gtk_widget_activate_action (GTK_WIDGET (self), "listitem.toggle-expand", NULL);
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
  GtkWidget *child;

  ide_tree_expander_set_list_row (self, NULL);

  g_clear_pointer (&self->image, gtk_widget_unparent);
  g_clear_pointer (&self->title, gtk_widget_unparent);
  g_clear_pointer (&self->suffix, gtk_widget_unparent);

  g_clear_object (&self->list_row);
  g_clear_object (&self->menu_model);

  g_clear_object (&self->icon);
  g_clear_object (&self->expanded_icon);

  child = gtk_widget_get_first_child (GTK_WIDGET (self));

  while (child != NULL)
    {
      GtkWidget *cur = child;
      child = gtk_widget_get_next_sibling (child);
      if (GTK_IS_POPOVER (cur))
        gtk_widget_unparent (cur);
    }

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

    case PROP_EXPANDED_ICON:
      g_value_set_object (value, ide_tree_expander_get_expanded_icon (self));
      break;

    case PROP_ICON:
      g_value_set_object (value, ide_tree_expander_get_icon (self));
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

    case PROP_IGNORED:
      g_value_set_boolean (value, ide_tree_expander_get_ignored (self));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, ide_tree_expander_get_use_markup (self));
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
    case PROP_EXPANDED_ICON:
      ide_tree_expander_set_expanded_icon (self, g_value_get_object (value));
      break;

    case PROP_EXPANDED_ICON_NAME:
      ide_tree_expander_set_expanded_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ICON:
      ide_tree_expander_set_icon (self, g_value_get_object (value));
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

    case PROP_IGNORED:
      ide_tree_expander_set_ignored (self, g_value_get_boolean (value));
      break;

    case PROP_USE_MARKUP:
      ide_tree_expander_set_use_markup (self, g_value_get_boolean (value));
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

  properties [PROP_EXPANDED_ICON] =
    g_param_spec_object ("expanded-icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_EXPANDED_ICON_NAME] =
    g_param_spec_string ("expanded-icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

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

  properties [PROP_IGNORED] =
    g_param_spec_boolean ("ignored", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "treeexpander");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);

  gtk_widget_class_install_action (widget_class, "listitem.toggle-expand", NULL, ide_tree_expander_toggle_expand);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, GDK_CONTROL_MASK, "listitem.toggle-expand", NULL);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TREE_ITEM);
}

static void
ide_tree_expander_init (IdeTreeExpander *self)
{
  GtkEventController *controller;

  self->image = g_object_new (GTK_TYPE_IMAGE, NULL);
  gtk_widget_insert_after (self->image, GTK_WIDGET (self), NULL);

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "halign", GTK_ALIGN_START,
                              "hexpand", TRUE,
                              "ellipsize", PANGO_ELLIPSIZE_END,
                              "margin-start", 3,
                              "margin-end", 3,
                              NULL);
  gtk_widget_insert_after (self->title, GTK_WIDGET (self), self->image);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->title, NULL,
                                  -1);

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

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
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

/**
 * ide_tree_expander_get_icon:
 * @self: a #IdeTreeExpander
 *
 * Gets the icon for the row.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_tree_expander_get_icon (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  return self->icon;
}

/**
 * ide_tree_expander_get_expanded_icon:
 * @self: a #IdeTreeExpander
 *
 * Gets the icon for the row when expanded.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_tree_expander_get_expanded_icon (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), NULL);

  return self->expanded_icon;
}

void
ide_tree_expander_set_icon (IdeTreeExpander *self,
                            GIcon           *icon)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));
  g_return_if_fail (!icon || G_IS_ICON (icon));

  if (g_set_object (&self->icon, icon))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON]);
      ide_tree_expander_update_icon (self);
    }
}

void
ide_tree_expander_set_expanded_icon (IdeTreeExpander *self,
                                     GIcon           *expanded_icon)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));
  g_return_if_fail (!expanded_icon || G_IS_ICON (expanded_icon));

  if (g_set_object (&self->expanded_icon, expanded_icon))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXPANDED_ICON]);
      ide_tree_expander_update_icon (self);
    }
}

void
ide_tree_expander_set_expanded_icon_name (IdeTreeExpander *self,
                                          const char      *expanded_icon_name)
{
  g_autoptr(GIcon) expanded_icon = NULL;

  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));

  if (expanded_icon_name != NULL)
    expanded_icon = g_themed_icon_new (expanded_icon_name);

  ide_tree_expander_set_expanded_icon (self, expanded_icon);
}

void
ide_tree_expander_set_icon_name (IdeTreeExpander *self,
                                 const char      *icon_name)
{
  g_autoptr(GIcon) icon = NULL;

  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));

  if (icon_name != NULL)
    icon = g_themed_icon_new (icon_name);

  ide_tree_expander_set_icon (self, icon);
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

gboolean
ide_tree_expander_get_ignored (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), FALSE);

  return gtk_widget_has_css_class (self->title, "dim-label");
}

void ide_tree_expander_set_ignored (IdeTreeExpander *self,
                                    gboolean         ignored)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));

  if (ide_tree_expander_get_ignored (self) != ignored)
    {
      if (ignored)
        gtk_widget_add_css_class (self->title, "dim-label");
      else
        gtk_widget_remove_css_class (self->title, "dim-label");
      g_object_notify_by_pspec (G_OBJECT(self), properties [PROP_IGNORED]);
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

static void
ide_tree_expander_clear_list_row (IdeTreeExpander *self)
{
  GtkWidget *child;

  g_assert (IDE_IS_TREE_EXPANDER (self));

  if (self->list_row == NULL)
    return;

  g_clear_signal_handler (&self->list_row_notify_expanded, self->list_row);

  g_clear_object (&self->list_row);

  gtk_label_set_label (GTK_LABEL (self->title), NULL);
  gtk_image_set_from_icon_name (GTK_IMAGE (self->image), NULL);

  child = gtk_widget_get_prev_sibling (self->image);

  while (child)
    {
      GtkWidget *prev = gtk_widget_get_prev_sibling (child);
      gtk_widget_unparent (child);
      child = prev;
    }

    gtk_accessible_reset_state (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_STATE_EXPANDED);
}

void
ide_tree_expander_set_list_row (IdeTreeExpander *self,
                                GtkTreeListRow  *list_row)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));
  g_return_if_fail (!list_row || GTK_IS_TREE_LIST_ROW (list_row));

  if (self->list_row == list_row)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  ide_tree_expander_clear_list_row (self);

  if (list_row != NULL)
    {
      self->list_row = g_object_ref (list_row);
      self->list_row_notify_expanded =
        g_signal_connect_object (self->list_row,
                                 "notify::expanded",
                                 G_CALLBACK (ide_tree_expander_notify_expanded_cb),
                                 self,
                                 G_CONNECT_SWAPPED);
      ide_tree_expander_update_depth (self);
      ide_tree_expander_update_expanded_state (self, list_row);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIST_ROW]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPANDED]);

  g_object_thaw_notify (G_OBJECT (self));
}

gboolean
ide_tree_expander_get_use_markup (IdeTreeExpander *self)
{
  g_return_val_if_fail (IDE_IS_TREE_EXPANDER (self), FALSE);

  return gtk_label_get_use_markup (GTK_LABEL (self->title));
}

void
ide_tree_expander_set_use_markup (IdeTreeExpander *self,
                                  gboolean         use_markup)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));

  use_markup = !!use_markup;

  if (use_markup != ide_tree_expander_get_use_markup (self))
    {
      gtk_label_set_use_markup (GTK_LABEL (self->title), use_markup);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_MARKUP]);
    }
}

static gboolean
ide_tree_expander_remove_popover_idle (gpointer user_data)
{
  gpointer *pair = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (pair != NULL);
  g_assert (IDE_IS_TREE_EXPANDER (pair[0]));
  g_assert (GTK_IS_POPOVER (pair[1]));

  if (gtk_widget_get_parent (pair[1]) == pair[0])
    gtk_widget_unparent (pair[1]);

  g_object_unref (pair[0]);
  g_object_unref (pair[1]);
  g_free (pair);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_tree_expander_popover_closed_cb (IdeTreeExpander *self,
                                     GtkPopover      *popover)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_EXPANDER (self));
  g_assert (GTK_IS_POPOVER (popover));

  if (self->popover == popover)
    {
      gpointer *pair = g_new (gpointer, 2);
      pair[0] = g_object_ref (self);
      pair[1] = g_object_ref (popover);
      /* We don't want to unparent the widget immediately because it gets
       * closed _BEFORE_ executing GAction. So removing it will cause the
       * actions to be unavailable.
       *
       * Instead, defer to an idle where we remove the popover.
       */
      g_idle_add (ide_tree_expander_remove_popover_idle, pair);
      self->popover = NULL;
    }
}

void
ide_tree_expander_show_popover (IdeTreeExpander *self,
                                GtkPopover      *popover)
{
  g_return_if_fail (IDE_IS_TREE_EXPANDER (self));
  g_return_if_fail (GTK_IS_POPOVER (popover));

  gtk_widget_set_parent (GTK_WIDGET (popover), GTK_WIDGET (self));

  g_signal_connect_object (popover,
                           "closed",
                           G_CALLBACK (ide_tree_expander_popover_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if (self->popover != NULL)
    gtk_popover_popdown (self->popover);

  self->popover = popover;

  gtk_popover_popup (popover);
}
