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

/*
 * NOTE:
 *
 * Thar be dragons!
 *
 * This code uses XMACROS. It's a bit insane, and does lots of
 * duplication that could be cleaned up next cycle.
 *
 * If you want this code for your module, I suggest you refactor
 * this to use GtkBuilder with custom_tag_start().
 *
 * I'd like to see something like:
 *
 *  <views>
 *    <view>
 *      <page>
 *        <column>
 *          <group>
 *            <shortcut />
 *            <shortcut />
 *            <gesture />
 *
 * Or whatever else you come up with and are willing to maintain.
 *
 * -- Christian
 */

#include <glib/gi18n.h>
#include <ide.h>

#include "egg-search-bar.h"

#include "gb-accel-label.h"
#include "gb-scrolled-window.h"
#include "gb-shortcuts-window.h"
#include "gb-widget.h"

struct _GbShortcutsWindow
{
  GtkWindow       parent_instance;

  GHashTable     *widget_keywords;

  GtkListBox     *list_box;
  GtkStack       *stack;
  GtkMenuButton  *menu_button;
  GtkLabel       *menu_label;
  GtkPopover     *popover;
  EggSearchBar   *search_bar;
  GtkSearchEntry *search_entry;

  GtkWidget      *previous_view;
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
      gtk_label_set_label (self->menu_label, title);
      gtk_stack_set_visible_child (self->stack, child);
    }
}

static void
register_keywords (GbShortcutsWindow *self,
                   GtkWidget         *widget,
                   const gchar       *keywords)
{
  gchar *casefold;

  g_assert (GB_IS_SHORTCUTS_WINDOW (self));

  casefold = g_utf8_casefold (keywords, -1);
  g_hash_table_insert (self->widget_keywords, widget, casefold);
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
  GtkSizeGroup *desc_group = NULL;
  gboolean do_keywords = FALSE;

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
    { \
      GtkListBoxRow *row; \
      row = g_object_new (GTK_TYPE_LIST_BOX_ROW, \
                          "visible", TRUE, \
                          "child", g_object_new (GTK_TYPE_LABEL, \
                                                 "label", _name, \
                                                 "visible", TRUE, \
                                                 "xalign", 0.5f, \
                                                 NULL), \
                          NULL); \
      g_object_set_data (G_OBJECT (row), "view", _ident); \
      gtk_container_add (GTK_CONTAINER (self->list_box), GTK_WIDGET (row)); \
    } \
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
    g_autofree gchar *keywords = NULL; \
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
    if (desc_group != NULL) \
      gtk_size_group_add_widget (desc_group, GTK_WIDGET (desc)); \
    if (do_keywords) \
      { \
        keywords = g_strdup_printf ("%s %s", _accel, _desc); \
        register_keywords (self, GTK_WIDGET (shortcut), keywords); \
      } \
  }
#define GESTURE(_accel, _title, _subtitle) \
  { \
    g_autofree gchar *keywords = NULL; \
    GtkBox *gesture; \
    GtkLabel *primary; \
    GtkLabel *subtitle; \
    GtkImage *image; \
    gesture = g_object_new (GTK_TYPE_GRID, \
                            "column-spacing", 12, \
                            "visible", TRUE, \
                            NULL); \
    gtk_container_add (GTK_CONTAINER (group), GTK_WIDGET (gesture)); \
    image = g_object_new (GTK_TYPE_IMAGE, \
                          "resource", "/org/gnome/builder/icons/scalable/actions/gesture-"_accel".svg", \
                          "xalign", 0.0f, \
                          "valign", GTK_ALIGN_CENTER, \
                          "visible", TRUE, \
                          NULL); \
    gtk_container_add_with_properties (GTK_CONTAINER (gesture), GTK_WIDGET (image), \
                                       "height", 2, \
                                       NULL); \
    primary = g_object_new (GTK_TYPE_LABEL, \
                            "label", _title, \
                            "visible", TRUE, \
                            "valign", GTK_ALIGN_END, \
                            "xalign", 0.0f, \
                            "hexpand", TRUE, \
                            NULL); \
    gtk_container_add_with_properties (GTK_CONTAINER (gesture), GTK_WIDGET (primary), \
                                       "left-attach", 1, \
                                       NULL); \
    subtitle = g_object_new (GTK_TYPE_LABEL, \
                             "label", _subtitle, \
                             "visible", TRUE, \
                             "valign", GTK_ALIGN_START, \
                             "xalign", 0.0f, \
                             "hexpand", TRUE, \
                             NULL); \
    if (desc_group != NULL) \
      gtk_size_group_add_widget (desc_group, GTK_WIDGET (subtitle)); \
    gb_widget_add_style_class (GTK_WIDGET (subtitle), "dim-label"); \
    gtk_container_add_with_properties (GTK_CONTAINER (gesture), GTK_WIDGET (subtitle), \
                                       "top-attach", 1, \
                                       "left-attach", 1, \
                                       NULL); \
    if (do_keywords) \
      { \
        keywords = g_strdup_printf ("%s %s %s", _accel, _title, _subtitle); \
        register_keywords (self, GTK_WIDGET (gesture), keywords); \
        gtk_size_group_add_widget (size_group, GTK_WIDGET (image)); \
      } \
  }

#include "gb-shortcuts-window.defs"

#undef VIEWS
#undef VIEW
#undef PAGE
#undef COLUMN
#undef GROUP

  /*
   * Now add the page where we show search results.
   */
  {
    GtkScrolledWindow *page;
    GtkSizeGroup *mod_key_group;
    GtkSizeGroup *size_group;
    GtkBox *group;

    page = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                         "visible", TRUE,
                         NULL);
    gtk_stack_add_titled (self->stack,
                          GTK_WIDGET (page),
                          "internal-search",
                          _("Search Results"));

    group = g_object_new (GTK_TYPE_BOX,
                          "border-width", 24,
                          "halign", GTK_ALIGN_CENTER,
                          "orientation", GTK_ORIENTATION_VERTICAL,
                          "spacing", 12,
                          "visible", TRUE,
                          NULL);
    gtk_container_add (GTK_CONTAINER (page), GTK_WIDGET (group));

    mod_key_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    desc_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    do_keywords = TRUE;

#define VIEWS(_views) {_views}
#define VIEW(_ident, _title, _pages) {_pages}
#define PAGE(_columns) {_columns}
#define COLUMN(_groups) {_groups}
#define GROUP(_title, _items) {_items}

#include "gb-shortcuts-window.defs"

#undef VIEWS
#undef VIEW
#undef PAGE
#undef COLUMN
#undef GROUP
#undef SHORTCUT
#undef GESTURE

    g_clear_object (&mod_key_group);
    g_clear_object (&size_group);
    g_clear_object (&desc_group);
  }
}

static void
gb_shortcuts_window__search_entry__changed (GbShortcutsWindow *self,
                                            GtkSearchEntry    *search_entry)
{
  g_autoptr(IdePatternSpec) spec = NULL;
  GHashTableIter iter;
  g_autofree gchar *query = NULL;
  g_autofree gchar *name = NULL;
  const gchar *text;
  GtkWidget *visible_child;
  gpointer key;
  gpointer value;

  g_assert (GB_IS_SHORTCUTS_WINDOW (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  text = gtk_entry_get_text (GTK_ENTRY (search_entry));

  if (ide_str_empty0 (text))
    {
      if (self->previous_view != NULL)
        gtk_stack_set_visible_child (self->stack, self->previous_view);
      g_hash_table_foreach (self->widget_keywords,
                            (GHFunc)gtk_widget_show,
                            NULL);
      return;
    }

  visible_child = gtk_stack_get_visible_child (self->stack);
  gtk_container_child_get (GTK_CONTAINER (self->stack), visible_child,
                           "name", &name,
                           NULL);

  if (!ide_str_equal0 (name, "internal-search"))
    {
      self->previous_view = visible_child;
      gtk_stack_set_visible_child_name (self->stack, "internal-search");
    }

  query = g_utf8_casefold (text, -1);
  spec = ide_pattern_spec_new (query);

  g_hash_table_iter_init (&iter, self->widget_keywords);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *keywords = value;

      if (ide_pattern_spec_match (spec, keywords))
        gtk_widget_show (key);
      else
        gtk_widget_hide (key);
    }
}

static void
gb_shortcuts_window__stack__notify_visible_child (GbShortcutsWindow *self,
                                                  GParamSpec        *pspec,
                                                  GtkStack          *stack)
{
  g_autofree gchar *title = NULL;
  GtkWidget *visible_child;

  g_assert (GB_IS_SHORTCUTS_WINDOW (self));
  g_assert (GTK_IS_STACK (stack));

  visible_child = gtk_stack_get_visible_child (stack);
  gtk_container_child_get (GTK_CONTAINER (stack), visible_child,
                           "title", &title,
                           NULL);
  gtk_label_set_label (self->menu_label, title);
}

static void
gb_shortcuts_window__list_box__row_activated (GbShortcutsWindow *self,
                                              GtkListBoxRow     *row,
                                              GtkListBox        *list_box)
{
  const gchar *view;

  g_assert (GB_IS_SHORTCUTS_WINDOW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  view = g_object_get_data (G_OBJECT (row), "view");
  if (view != NULL)
    gtk_stack_set_visible_child_name (self->stack, view);

  /*
   * Ensure search is now hidden.
   */
  gtk_entry_set_text (GTK_ENTRY (self->search_entry), "");
  egg_search_bar_set_search_mode_enabled (self->search_bar, FALSE);

  /*
   * Apparently we need to hide the popover manually.
   */
  gtk_widget_hide (GTK_WIDGET (self->popover));
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
gb_shortcuts_window_finalize (GObject *object)
{
  GbShortcutsWindow *self = (GbShortcutsWindow *)object;

  g_clear_pointer (&self->widget_keywords, g_hash_table_unref);

  G_OBJECT_CLASS (gb_shortcuts_window_parent_class)->finalize (object);
}

static void
gb_shortcuts_window_class_init (GbShortcutsWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_shortcuts_window_constructed;
  object_class->finalize = gb_shortcuts_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-shortcuts-window.ui");

  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, list_box);
  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, menu_button);
  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, menu_label);
  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, popover);
  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, search_bar);
  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GbShortcutsWindow, stack);

  g_type_ensure (EGG_TYPE_SEARCH_BAR);
  g_type_ensure (GB_TYPE_SCROLLED_WINDOW);
}

static void
gb_shortcuts_window_init (GbShortcutsWindow *self)
{
  self->widget_keywords = g_hash_table_new_full (NULL, NULL, NULL, g_free);

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_shortcuts_window_build (self);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (gb_shortcuts_window__search_entry__changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->stack,
                           "notify::visible-child",
                           G_CALLBACK (gb_shortcuts_window__stack__notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (gb_shortcuts_window__list_box__row_activated),
                           self,
                           G_CONNECT_SWAPPED);
}
