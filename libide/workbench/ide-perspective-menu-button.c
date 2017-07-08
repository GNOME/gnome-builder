/* ide-perspective-menu-button.c
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

#define G_LOG_DOMAIN "ide-perspective-menu-button"

#include <dazzle.h>

#include "ide-macros.h"

#include "workbench/ide-perspective.h"
#include "workbench/ide-perspective-menu-button.h"
#include "workbench/ide-workbench.h"

struct _IdePerspectiveMenuButton
{
  GtkMenuButton  parent_instance;

  /* Weak references */
  GtkWidget     *stack;

  /* Template children */
  GtkSizeGroup  *accel_size_group;
  GtkListBox    *list_box;
  GtkPopover    *popover;
  GtkImage      *image;
};

enum {
  PROP_0,
  PROP_STACK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

G_DEFINE_TYPE (IdePerspectiveMenuButton, ide_perspective_menu_button, GTK_TYPE_MENU_BUTTON)

static GtkWidget *
ide_perspective_menu_button_create_row (IdePerspectiveMenuButton *self,
                                        IdePerspective           *perspective)
{
  g_autofree gchar *title = NULL;
  g_autofree gchar *icon_name = NULL;
  g_autofree gchar *accel = NULL;
  GtkListBoxRow *row;
  GtkLabel *label;
  GtkImage *image;
  GtkBox *box;

  g_assert (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));
  g_assert (IDE_IS_PERSPECTIVE (perspective));

  title = ide_perspective_get_title (perspective);
  icon_name = ide_perspective_get_icon_name (perspective);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "selectable", FALSE,
                      "visible", TRUE,
                      NULL);

  g_object_set_data_full (G_OBJECT (row),
                          "IDE_PERSPECTIVE_ID",
                          ide_perspective_get_id (perspective),
                          g_free);

  g_object_set_data (G_OBJECT (row),
                     "IDE_PERSPECTIVE_PRIORITY",
                     GINT_TO_POINTER (ide_perspective_get_priority (perspective)));

  box = g_object_new (GTK_TYPE_BOX,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));

  image = g_object_new (GTK_TYPE_IMAGE,
                        "hexpand", FALSE,
                        "icon-name", icon_name,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", title,
                        "hexpand", TRUE,
                        "xalign", 0.0f,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));

  accel = ide_perspective_get_accelerator (perspective);

  if (accel != NULL)
    {
      g_autofree gchar *xaccel = NULL;
      guint accel_key = 0;
      GdkModifierType accel_mod = 0;

      gtk_accelerator_parse (accel, &accel_key, &accel_mod);
      xaccel = gtk_accelerator_get_label (accel_key, accel_mod);
      label = g_object_new (GTK_TYPE_LABEL,
                            "label", xaccel,
                            "visible", TRUE,
                            "xalign", 0.0f,
                            NULL);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (label), "dim-label");
      dzl_gtk_widget_add_style_class (GTK_WIDGET (label), "accel");
      gtk_container_add_with_properties (GTK_CONTAINER (box), GTK_WIDGET (label),
                                         "pack-type", GTK_PACK_END,
                                         NULL);
      gtk_size_group_add_widget (self->accel_size_group, GTK_WIDGET (label));
    }

  return GTK_WIDGET (row);
}

static void
ide_perspective_menu_button_do_add_child (GtkWidget *widget,
                                          gpointer   user_data)
{
  IdePerspectiveMenuButton *self = user_data;
  GtkWidget *row;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));

  row = ide_perspective_menu_button_create_row (self, IDE_PERSPECTIVE (widget));
  gtk_container_add (GTK_CONTAINER (self->list_box), row);
  gtk_list_box_invalidate_sort (self->list_box);
}

static void
ide_perspective_menu_button_add_child (IdePerspectiveMenuButton *self,
                                       GtkWidget                *child,
                                       GtkStack                 *stack)
{
  g_assert (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));
  g_assert (GTK_IS_WIDGET (child));
  g_assert (GTK_IS_STACK (stack));

  if (!IDE_IS_PERSPECTIVE (child))
    {
      g_warning ("Attempt to add something other than an IdePerspective to %s",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  if (ide_perspective_is_early (IDE_PERSPECTIVE (child)))
    return;

  ide_perspective_menu_button_do_add_child (child, self);
}

static void
ide_perspective_menu_button_do_remove_child (GtkWidget *widget,
                                             gpointer   user_data)
{
  const gchar *id = user_data;
  const gchar *widget_id;

  g_assert (GTK_IS_LIST_BOX_ROW (widget));

  widget_id = g_object_get_data (G_OBJECT (widget), "IDE_PERSPECTIVE_ID");

  if (g_strcmp0 (widget_id, id) == 0)
    gtk_widget_destroy (widget);
}

static void
ide_perspective_menu_button_remove_child (IdePerspectiveMenuButton *self,
                                          GtkWidget                *child,
                                          GtkStack                 *stack)
{
  g_autofree gchar *id = NULL;

  g_assert (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));
  g_assert (GTK_IS_WIDGET (child));
  g_assert (GTK_IS_STACK (stack));

  /* warn on addition, silent on removal */
  if (!IDE_IS_PERSPECTIVE (child))
    return;

  id = ide_perspective_get_id (IDE_PERSPECTIVE (child));
  if (id != NULL)
    gtk_container_foreach (GTK_CONTAINER (self->list_box),
                           ide_perspective_menu_button_do_remove_child,
                           id);
}

static void
ide_perspective_menu_button_notify_visible_child (IdePerspectiveMenuButton *self,
                                                  GParamSpec               *pspec,
                                                  GtkStack                 *stack)
{
  GtkWidget *child;

  g_assert (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));
  g_assert (GTK_IS_STACK (stack));

  child = gtk_stack_get_visible_child (stack);

  if (IDE_IS_PERSPECTIVE (child))
    {
      g_autofree gchar *icon_name = NULL;

      icon_name = ide_perspective_get_icon_name (IDE_PERSPECTIVE (child));

      g_object_set (self->image,
                    "icon-name", icon_name,
                    NULL);
    }
}

static void
ide_perspective_menu_button_disconnect (IdePerspectiveMenuButton *self)
{
  g_assert (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));
  g_assert (GTK_IS_STACK (self->stack));

  g_signal_handlers_disconnect_by_func (self->stack,
                                        G_CALLBACK (ide_perspective_menu_button_add_child),
                                        self);
  g_signal_handlers_disconnect_by_func (self->stack,
                                        G_CALLBACK (ide_perspective_menu_button_remove_child),
                                        self);
  g_signal_handlers_disconnect_by_func (self->stack,
                                        G_CALLBACK (ide_perspective_menu_button_notify_visible_child),
                                        self);

  ide_clear_weak_pointer (&self->stack);
}

static void
ide_perspective_menu_button_connect (IdePerspectiveMenuButton *self,
                                     GtkWidget                *stack)
{
  g_assert (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));
  g_assert (GTK_IS_STACK (stack));

  ide_set_weak_pointer (&self->stack, stack);

  g_signal_connect_object (stack,
                           "add",
                           G_CALLBACK (ide_perspective_menu_button_add_child),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack,
                           "remove",
                           G_CALLBACK (ide_perspective_menu_button_remove_child),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_perspective_menu_button_notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_container_foreach (GTK_CONTAINER (stack),
                         ide_perspective_menu_button_do_add_child,
                         self);
}

static void
ide_perspective_menu_button_row_activated (IdePerspectiveMenuButton *self,
                                           GtkListBoxRow            *row,
                                           GtkListBox               *list_box)
{
  const gchar *id;
  GtkWidget *workbench;

  g_assert (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  workbench = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_WORKBENCH);
  id = g_object_get_data (G_OBJECT (row), "IDE_PERSPECTIVE_ID");

  /*
   * We use the workbench to set the perspective name rather than the stack
   * so that it can have a simpler implementation of handling changes between
   * perspectives. Otherwise, we have to be much more careful about
   * re-entrancy issues.
   */

  if (id != NULL && IDE_IS_WORKBENCH (workbench))
    {
      ide_workbench_set_visible_perspective_name (IDE_WORKBENCH (workbench), id);
      gtk_popover_popdown (self->popover);
    }
}

static gint
list_box_sort (GtkListBoxRow *row1,
               GtkListBoxRow *row2,
               gpointer       user_data)
{
  gpointer priority1;
  gpointer priority2;

  priority1 = g_object_get_data (G_OBJECT (row1), "IDE_PERSPECTIVE_PRIORITY");
  priority2 = g_object_get_data (G_OBJECT (row2), "IDE_PERSPECTIVE_PRIORITY");

  return GPOINTER_TO_INT (priority1) - GPOINTER_TO_INT (priority2);
}

static void
ide_perspective_menu_button_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdePerspectiveMenuButton *self = IDE_PERSPECTIVE_MENU_BUTTON (object);

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, ide_perspective_menu_button_get_stack (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_perspective_menu_button_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdePerspectiveMenuButton *self = IDE_PERSPECTIVE_MENU_BUTTON (object);

  switch (prop_id)
    {
    case PROP_STACK:
      ide_perspective_menu_button_set_stack (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_perspective_menu_button_class_init (IdePerspectiveMenuButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_perspective_menu_button_get_property;
  object_class->set_property = ide_perspective_menu_button_set_property;

  properties [PROP_STACK] =
    g_param_spec_object ("stack",
                         "Stack",
                         "The perspectives stack",
                         GTK_TYPE_STACK,

                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-perspective-menu-button.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePerspectiveMenuButton, accel_size_group);
  gtk_widget_class_bind_template_child (widget_class, IdePerspectiveMenuButton, image);
  gtk_widget_class_bind_template_child (widget_class, IdePerspectiveMenuButton, list_box);
  gtk_widget_class_bind_template_child (widget_class, IdePerspectiveMenuButton, popover);
}

static void
ide_perspective_menu_button_init (IdePerspectiveMenuButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->list_box, list_box_sort, NULL, NULL);

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (ide_perspective_menu_button_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
}

/**
 * ide_perspective_menu_button_get_stack:
 *
 * Returns: (nullable) (transfer none): A #GtkStack or %NULL.
 */
GtkWidget *
ide_perspective_menu_button_get_stack (IdePerspectiveMenuButton *self)
{
  g_return_val_if_fail (IDE_IS_PERSPECTIVE_MENU_BUTTON (self), NULL);

  return self->stack;
}

void
ide_perspective_menu_button_set_stack (IdePerspectiveMenuButton *self,
                                       GtkWidget                *stack)
{
  g_return_if_fail (IDE_IS_PERSPECTIVE_MENU_BUTTON (self));
  g_return_if_fail (!stack || GTK_IS_STACK (stack));

  if (stack != self->stack)
    {
      if (self->stack != NULL)
        ide_perspective_menu_button_disconnect (self);

      if (stack != NULL)
        ide_perspective_menu_button_connect (self, stack);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STACK]);
    }
}
