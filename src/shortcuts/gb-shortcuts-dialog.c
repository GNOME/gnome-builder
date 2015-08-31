/* gb-shortcuts-dialog.c
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

#include "egg-search-bar.h"

#include "gb-scrolled-window.h"
#include "gb-shortcuts-column.h"
#include "gb-shortcuts-dialog.h"
#include "gb-shortcuts-gesture.h"
#include "gb-shortcuts-group.h"
#include "gb-shortcuts-page.h"
#include "gb-shortcuts-shortcut.h"
#include "gb-shortcuts-view.h"

typedef struct
{
  GtkStack       *stack;
  GtkMenuButton  *menu_button;
  GtkLabel       *menu_label;
  EggSearchBar   *search_bar;
  GtkSearchEntry *search_entry;
  GtkHeaderBar   *header_bar;
  GtkPopover     *popover;
  GtkListBox     *list_box;
} GbShortcutsDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GbShortcutsDialog, gb_shortcuts_dialog, GTK_TYPE_WINDOW)

enum {
  CLOSE,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

static void
gb_shortcuts_dialog_add_view (GbShortcutsDialog *self,
                              GbShortcutsView   *view)
{
  GbShortcutsDialogPrivate *priv = gb_shortcuts_dialog_get_instance_private (self);
  GtkListBoxRow *row;
  const gchar *title;
  const gchar *name;
  GtkWidget *label;

  g_assert (GB_IS_SHORTCUTS_DIALOG (self));
  g_assert (GB_IS_SHORTCUTS_VIEW (view));

  name = gb_shortcuts_view_get_view_name (view);
  title = gb_shortcuts_view_get_title (view);

  gtk_stack_add_titled (priv->stack, GTK_WIDGET (view), name, title);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data_full (G_OBJECT (row), "GB_SHORTCUTS_VIEW_NAME", g_strdup (name), g_free);
  label = g_object_new (GTK_TYPE_LABEL,
                        "margin", 6,
                        "label", title,
                        "xalign", 0.0f,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (label));
  gtk_container_add (GTK_CONTAINER (priv->list_box), GTK_WIDGET (row));
}

static void
gb_shortcuts_dialog_add (GtkContainer *container,
                         GtkWidget    *widget)
{
  GbShortcutsDialog *self = (GbShortcutsDialog *)container;

  g_assert (GB_IS_SHORTCUTS_DIALOG (self));

  if (GB_IS_SHORTCUTS_VIEW (widget))
    gb_shortcuts_dialog_add_view (self, GB_SHORTCUTS_VIEW (widget));
  else
    GTK_CONTAINER_CLASS (gb_shortcuts_dialog_parent_class)->add (container, widget);
}

static void
gb_shortcuts_dialog__stack__notify_visible_child (GbShortcutsDialog *self,
                                                  GParamSpec        *pspec,
                                                  GtkStack          *stack)
{
  GbShortcutsDialogPrivate *priv = gb_shortcuts_dialog_get_instance_private (self);
  GtkWidget *visible_child;

  visible_child = gtk_stack_get_visible_child (stack);

  if (GB_IS_SHORTCUTS_VIEW (visible_child))
    {
      const gchar *title;

      title = gb_shortcuts_view_get_title (GB_SHORTCUTS_VIEW (visible_child));
      gtk_label_set_label (priv->menu_label, title);
    }
}

static void
gb_shortcuts_dialog__list_box__row_activated (GbShortcutsDialog *self,
                                              GtkListBoxRow     *row,
                                              GtkListBox        *list_box)
{
  GbShortcutsDialogPrivate *priv = gb_shortcuts_dialog_get_instance_private (self);
  const gchar *name;

  g_assert (GB_IS_SHORTCUTS_DIALOG (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  name = g_object_get_data (G_OBJECT (row), "GB_SHORTCUTS_VIEW_NAME");
  gtk_stack_set_visible_child_name (priv->stack, name);
  gtk_widget_hide (GTK_WIDGET (priv->popover));
}

static void
gb_shortcuts_dialog_class_init (GbShortcutsDialogClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkBindingSet *binding_set = gtk_binding_set_by_class (klass);

  container_class->add = gb_shortcuts_dialog_add;

  gSignals [CLOSE] = g_signal_new ("close",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                   G_STRUCT_OFFSET (GbShortcutsDialogClass, close),
                                   NULL, NULL, NULL,
                                   G_TYPE_NONE,
                                   0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  g_type_ensure (GB_TYPE_SHORTCUTS_PAGE);
  g_type_ensure (GB_TYPE_SHORTCUTS_COLUMN);
  g_type_ensure (GB_TYPE_SHORTCUTS_GROUP);
  g_type_ensure (GB_TYPE_SHORTCUTS_GESTURE);
  g_type_ensure (GB_TYPE_SHORTCUTS_SHORTCUT);
}

static void
gb_shortcuts_dialog_init (GbShortcutsDialog *self)
{
  GbShortcutsDialogPrivate *priv = gb_shortcuts_dialog_get_instance_private (self);
  GtkToggleButton *search_button;
  GtkScrolledWindow *scroller;
  GtkBox *main_box;
  GtkBox *menu_box;
  GtkArrow *arrow;
  GtkSearchEntry *entry;

  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);

  priv->header_bar = g_object_new (GTK_TYPE_HEADER_BAR,
                                   "show-close-button", TRUE,
                                   "visible", TRUE,
                                   NULL);
  gtk_window_set_titlebar (GTK_WINDOW (self), GTK_WIDGET (priv->header_bar));

  search_button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                                "child", g_object_new (GTK_TYPE_IMAGE,
                                                       "visible", TRUE,
                                                       "icon-name", "edit-find-symbolic",
                                                       NULL),
                                "visible", TRUE,
                                NULL);
  gtk_container_add (GTK_CONTAINER (priv->header_bar), GTK_WIDGET (search_button));

  main_box = g_object_new (GTK_TYPE_BOX,
                           "orientation", GTK_ORIENTATION_VERTICAL,
                           "visible", TRUE,
                           NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (main_box));

  priv->search_bar = g_object_new (EGG_TYPE_SEARCH_BAR,
                                   "visible", TRUE,
                                   NULL);
  g_object_bind_property (priv->search_bar, "search-mode-enabled",
                          search_button, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_container_add (GTK_CONTAINER (main_box), GTK_WIDGET (priv->search_bar));

  priv->stack = g_object_new (GTK_TYPE_STACK,
                              "expand", TRUE,
                              "homogeneous", TRUE,
                              "visible", TRUE,
                              NULL);
  gtk_container_add (GTK_CONTAINER (main_box), GTK_WIDGET (priv->stack));

  priv->menu_button = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "focus-on-click", FALSE,
                                    "visible", TRUE,
                                    NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (priv->menu_button)),
                               "flat");
  gtk_header_bar_set_custom_title (priv->header_bar, GTK_WIDGET (priv->menu_button));

  menu_box = g_object_new (GTK_TYPE_BOX,
                           "orientation", GTK_ORIENTATION_HORIZONTAL,
                           "spacing", 3,
                           "visible", TRUE,
                           NULL);
  gtk_container_add (GTK_CONTAINER (priv->menu_button), GTK_WIDGET (menu_box));

  priv->menu_label = g_object_new (GTK_TYPE_LABEL,
                                   "visible", TRUE,
                                   NULL);
  gtk_container_add (GTK_CONTAINER (menu_box), GTK_WIDGET (priv->menu_label));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  arrow = g_object_new (GTK_TYPE_ARROW,
                        "arrow-type", GTK_ARROW_DOWN,
                        "visible", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (menu_box), GTK_WIDGET (arrow));
  G_GNUC_END_IGNORE_DEPRECATIONS;

  priv->popover = g_object_new (GTK_TYPE_POPOVER,
                                "border-width", 6,
                                "relative-to", priv->menu_button,
                                "position", GTK_POS_BOTTOM,
                                NULL);
  gtk_menu_button_set_popover (priv->menu_button, GTK_WIDGET (priv->popover));

  scroller = g_object_new (GB_TYPE_SCROLLED_WINDOW,
                           "min-content-width", 150,
                           "max-content-width", 300,
                           "min-content-height", 10,
                           "max-content-height", 300,
                           "shadow-type", GTK_SHADOW_IN,
                           "visible", TRUE,
                           NULL);
  gtk_container_add (GTK_CONTAINER (priv->popover), GTK_WIDGET (scroller));

  priv->list_box = g_object_new (GTK_TYPE_LIST_BOX,
                                 "selection-mode", GTK_SELECTION_NONE,
                                 "visible", TRUE,
                                 NULL);
  g_signal_connect_object (priv->list_box,
                           "row-activated",
                           G_CALLBACK (gb_shortcuts_dialog__list_box__row_activated),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (priv->list_box));

  entry = GTK_SEARCH_ENTRY (egg_search_bar_get_entry (priv->search_bar));
  g_object_set (entry,
                "placeholder-text", _("Search Shortcuts"),
                "width-chars", 40,
                NULL);

  g_signal_connect_object (priv->stack,
                           "notify::visible-child",
                           G_CALLBACK (gb_shortcuts_dialog__stack__notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);
}
