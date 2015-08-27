/* gb-shortcuts-window.c
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

#include <glib/gi18n.h>

#include "gb-accel-label.h"
#include "gb-shortcuts-window.h"

struct _GbShortcutsWindow
{
  GtkWindow      parent_instance;

  GtkStack      *stack;
  GtkMenuButton *menu_button;
};

G_DEFINE_TYPE (GbShortcutsWindow, gb_shortcuts_window, GTK_TYPE_WINDOW)

GtkWidget *
gb_shortcuts_window_new (void)
{
  return g_object_new (GB_TYPE_SHORTCUTS_WINDOW, NULL);
}

static void
gb_shortcuts_window_set_view (GbShortcutsWindow *self,
                              const gchar       *name)
{
  GtkWidget *child;

  g_assert (GB_IS_SHORTCUTS_WINDOW (self));

  child = gtk_stack_get_child_by_name (self->stack, name);

  if (child != NULL)
    {
      g_autofree gchar *title = NULL;

      gtk_container_child_get (GTK_CONTAINER (self->stack), child,
                               "title", &title,
                               NULL);
      gtk_button_set_label (GTK_BUTTON (self->menu_button), title);
      gtk_stack_set_visible_child (self->stack, child);
    }
}

static void
adjust_page_buttons (GtkWidget *widget,
                     gpointer   data)
{
  /*
   * TODO: This is a hack to get the GtkStackSwitcher radio
   *       buttons to look how we want. However, it's very
   *       much font size specific.
   */
  gtk_widget_set_size_request (widget, 34, 34);
}

static void
gb_shortcuts_window_build (GbShortcutsWindow *self)
{
  g_assert (GB_IS_SHORTCUTS_WINDOW (self));

#define VIEWS(_views) { _views }
#define VIEW(_ident, _name, _pages) \
  { \
    GtkBox *view; \
    GtkStack *stack; \
    GtkStackSwitcher *switcher; \
    guint page_count = 0; \
    view = g_object_new (GTK_TYPE_BOX, \
                         "border-width", 24, \
                         "spacing", 22, \
                         "orientation", GTK_ORIENTATION_VERTICAL, \
                         "visible", TRUE, \
                         "vexpand", TRUE, \
                         NULL); \
    stack = g_object_new (GTK_TYPE_STACK, \
                          "visible", TRUE, \
                          "vexpand", TRUE, \
                          "transition-type", GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT, \
                          "homogeneous", TRUE, \
                          NULL); \
    gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (stack)); \
    switcher = g_object_new (GTK_TYPE_STACK_SWITCHER, \
                             "halign", GTK_ALIGN_CENTER, \
                             "spacing", 12, \
                             "stack", stack, \
                             NULL); \
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (switcher)), "round"); \
    gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (switcher)); \
    gtk_stack_add_titled (self->stack, GTK_WIDGET (view), _ident, _name); \
    _pages \
    if (page_count > 1) \
      gtk_widget_show (GTK_WIDGET (switcher)); \
    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (switcher)), "linked"); \
    gtk_container_foreach (GTK_CONTAINER (switcher), adjust_page_buttons, NULL); \
  }
#define PAGE(_columns) \
  { \
    g_autofree gchar *title = g_strdup_printf ("%u", ++page_count); \
    GtkBox *page; \
    page = g_object_new (GTK_TYPE_BOX, \
                         "homogeneous", FALSE, \
                         "orientation", GTK_ORIENTATION_HORIZONTAL, \
                         "spacing", 24, \
                         "visible", TRUE, \
                         NULL); \
    _columns \
    gtk_stack_add_titled (stack, GTK_WIDGET (page), title, title); \
  }
#define COLUMN(_groups) \
  { \
    GtkBox *column; \
    GtkSizeGroup *size_group; \
    GtkSizeGroup *mod_key_group; \
    column = g_object_new (GTK_TYPE_BOX, \
                           "orientation", GTK_ORIENTATION_VERTICAL, \
                           "spacing", 22, \
                           "visible", TRUE, \
                           NULL); \
    gtk_container_add (GTK_CONTAINER (page), GTK_WIDGET (column)); \
    size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL); \
    mod_key_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL); \
    _groups \
    g_clear_object (&size_group); \
    g_clear_object (&mod_key_group); \
  }
#define GROUP(_group_name, _shortcuts) \
  { \
    g_autofree gchar *title_markup = g_strdup_printf ("<b>%s</b>", _group_name); \
    GtkBox *group; \
    GtkBox *title_label; \
    group = g_object_new (GTK_TYPE_BOX, \
                          "orientation", GTK_ORIENTATION_VERTICAL, \
                          "spacing", 10, \
                          "visible", TRUE, \
                          NULL); \
    gtk_container_add (GTK_CONTAINER (column), GTK_WIDGET (group)); \
    title_label = g_object_new (GTK_TYPE_LABEL, \
                                "visible", TRUE, \
                                "label", title_markup, \
                                "use-markup", TRUE, \
                                "xalign", 0.0f, \
                                NULL); \
    gtk_container_add (GTK_CONTAINER (group), GTK_WIDGET (title_label)); \
    _shortcuts \
  }
#define SHORTCUT(_accel, _desc) \
  { \
    GtkBox *shortcut; \
    GbAccelLabel *accel; \
    GtkLabel *desc; \
    shortcut = g_object_new (GTK_TYPE_BOX, \
                             "orientation", GTK_ORIENTATION_HORIZONTAL, \
                             "spacing", 10, \
                             "visible", TRUE, \
                             NULL); \
    gtk_container_add (GTK_CONTAINER (group), GTK_WIDGET (shortcut)); \
    accel = g_object_new (GB_TYPE_ACCEL_LABEL, \
                          "accelerator", _accel, \
                          "size-group", mod_key_group, \
                          "halign", GTK_ALIGN_START, \
                          "visible", TRUE, \
                          NULL); \
    gtk_size_group_add_widget (size_group, GTK_WIDGET (accel)); \
    gtk_container_add (GTK_CONTAINER (shortcut), GTK_WIDGET (accel)); \
    desc = g_object_new (GTK_TYPE_LABEL, \
                         "label", _desc, \
                         "visible", TRUE, \
                         "xalign", 0.0f, \
                         "hexpand", TRUE, \
                         NULL); \
    gtk_container_add (GTK_CONTAINER (shortcut), GTK_WIDGET (desc)); \
  }
#define GESTURE(_accel, _title, _subtitle)


#include "gb-shortcuts-window.defs"


#undef VIEWS
#undef VIEW
#undef PAGE
#undef COLUMN
#undef GROUP
#undef SHORTCUT
#undef GESTURE
}

static void
gb_shortcuts_window_constructed (GObject *object)
{
  GbShortcutsWindow *self = (GbShortcutsWindow *)object;

  g_assert (GB_IS_SHORTCUTS_WINDOW (self));

  G_OBJECT_CLASS (gb_shortcuts_window_parent_class)->constructed (object);

  gb_shortcuts_window_set_view (self, "editor");
}

static void
gb_shortcuts_window_class_init (GbShortcutsWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_shortcuts_window_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-shortcuts-window.ui");

  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, stack);
  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, menu_button);
}

static void
gb_shortcuts_window_init (GbShortcutsWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gb_shortcuts_window_build (self);
}
