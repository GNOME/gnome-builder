/* ide-preferences-window.c
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-preferences-window"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-preferences-window.h"

struct _IdePreferencesWindow
{
  AdwApplicationWindow parent_window;

  GtkToggleButton    *search_button;
  GtkButton          *back_button;
  GtkStack           *page_stack;
  AdwWindowTitle     *page_title;
  GtkStack           *pages_stack;
  AdwWindowTitle     *pages_title;

  const IdePreferencePageEntry *current_page;

  guint rebuild_source;

  struct {
    GPtrArray *pages;
    GPtrArray *groups;
    GPtrArray *items;
    GArray *data;
  } info;
};

typedef struct
{
  gpointer data;
  GDestroyNotify notify;
} DataDestroy;

typedef struct
{
  GtkStack  *stack;
  GtkWidget *child;
} DropPage;

typedef struct
{
  GtkBox *box;
  GtkSearchBar *search_bar;
  GtkSearchEntry *search_entry;
  GtkScrolledWindow *scroller;
  GtkListBox *list_box;
} Page;

G_DEFINE_FINAL_TYPE (IdePreferencesWindow, ide_preferences_window, ADW_TYPE_APPLICATION_WINDOW)

static gboolean
drop_page_cb (gpointer data)
{
  DropPage *drop = data;
  gtk_stack_remove (drop->stack, drop->child);
  return G_SOURCE_REMOVE;
}

static DropPage *
drop_page_new (GtkStack  *stack,
               GtkWidget *child)
{
  DropPage *p = g_new0 (DropPage, 1);
  p->stack = g_object_ref (stack);
  p->child = g_object_ref (child);
  return p;
}

static void
drop_page_free (gpointer data)
{
  DropPage *drop = data;
  g_object_unref (drop->child);
  g_object_unref (drop->stack);
  g_free (drop);
}

static gboolean
entry_matches (const char *request,
               const char *current)
{
  if (g_strcmp0 (request, current) == 0)
    return TRUE;

  if (g_str_has_suffix (request, "/*") &&
      strncmp (request, current, strlen (request) - 2) == 0)
    return TRUE;

  return FALSE;
}

static const IdePreferencePageEntry *
get_page (IdePreferencesWindow *self,
          const char           *name)
{
  if (name == NULL)
    return NULL;

  for (guint i = 0; i < self->info.pages->len; i++)
    {
      const IdePreferencePageEntry *page = g_ptr_array_index (self->info.pages, i);

      if (entry_matches (page->name, name))
        return page;
    }

  return NULL;
}

static void
go_back_cb (IdePreferencesWindow *self,
            GtkButton            *button)
{
  const IdePreferencePageEntry *page;
  const char *pages_name;

  g_assert (IDE_IS_PREFERENCES_WINDOW (self));
  g_assert (GTK_IS_BUTTON (button));

  pages_name = gtk_stack_get_visible_child_name (self->pages_stack);
  page = get_page (self, pages_name);
  if (page == NULL)
    return;

  if (!(page = get_page (self, page->parent)))
    {
      gtk_stack_set_visible_child_name (self->pages_stack, "default");

      gtk_widget_hide (GTK_WIDGET (self->back_button));
      gtk_widget_show (GTK_WIDGET (self->search_button));
    }
  else
    {
      gtk_stack_set_visible_child_name (self->pages_stack, page->name);
    }
}

static gboolean
filter_rows_cb (GtkListBoxRow *row,
                gpointer       user_data)
{
  const IdePreferencePageEntry *page;
  const char *text = user_data;

  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (text != NULL);

  page = g_object_get_data (G_OBJECT (row), "ENTRY");

  /* TODO: Precalculate keywords/etc for pages */

  if (strstr (page->name, text) != NULL ||
      strstr (page->title, text) != NULL)
    return TRUE;

  return FALSE;
}

static void
search_changed_cb (IdePreferencesWindow *self,
                   GtkSearchEntry       *entry)
{
  const char *text;
  GtkBox *box;
  Page *page;

  g_assert (IDE_IS_PREFERENCES_WINDOW (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  box = GTK_BOX (gtk_widget_get_ancestor (GTK_WIDGET (entry), GTK_TYPE_BOX));
  page = g_object_get_data (G_OBJECT (box), "PAGE");
  g_assert (box == page->box);

  text = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (text == NULL || text[0] == 0)
    gtk_list_box_set_filter_func (page->list_box, NULL, NULL, NULL);
  else
    gtk_list_box_set_filter_func (page->list_box, filter_rows_cb, g_strdup (text), g_free);
}

static void
ide_preferences_window_dispose (GObject *object)
{
  IdePreferencesWindow *self = (IdePreferencesWindow *)object;

  g_clear_handle_id (&self->rebuild_source, g_source_remove);

  if (self->info.data != NULL)
    {
      for (guint i = self->info.data->len; i > 0; i--)
        {
          DataDestroy data = g_array_index (self->info.data, DataDestroy, i-1);
          g_array_remove_index (self->info.data, i-1);
          data.notify (data.data);
        }
    }

  g_clear_pointer (&self->info.pages, g_ptr_array_unref);
  g_clear_pointer (&self->info.groups, g_ptr_array_unref);
  g_clear_pointer (&self->info.items, g_ptr_array_unref);
  g_clear_pointer (&self->info.data, g_array_unref);

  G_OBJECT_CLASS (ide_preferences_window_parent_class)->dispose (object);
}

static void
ide_preferences_window_class_init (IdePreferencesWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_preferences_window_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-preferences-window.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesWindow, page_stack);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesWindow, pages_stack);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesWindow, page_title);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesWindow, pages_title);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesWindow, search_button);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesWindow, back_button);
  gtk_widget_class_bind_template_callback (widget_class, go_back_cb);
}

static void
ide_preferences_window_init (IdePreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->info.pages = g_ptr_array_new_with_free_func (g_free);
  self->info.groups = g_ptr_array_new_with_free_func (g_free);
  self->info.items = g_ptr_array_new_with_free_func (g_free);
  self->info.data = g_array_new (FALSE, FALSE, sizeof (DataDestroy));
}

GtkWidget *
ide_preferences_window_new (void)
{
  return g_object_new (IDE_TYPE_PREFERENCES_WINDOW, NULL);
}

static int
sort_pages_by_priority (gconstpointer a,
                        gconstpointer b)
{
  const IdePreferencePageEntry * const *a_entry = a;
  const IdePreferencePageEntry * const *b_entry = b;

  if ((*a_entry)->priority < (*b_entry)->priority)
    return -1;
  else if ((*a_entry)->priority > (*b_entry)->priority)
    return 1;
  else
    return 0;
}

static int
sort_groups_by_priority (gconstpointer a,
                         gconstpointer b)
{
  const IdePreferenceGroupEntry * const *a_entry = a;
  const IdePreferenceGroupEntry * const *b_entry = b;

  if ((*a_entry)->priority < (*b_entry)->priority)
    return -1;
  else if ((*a_entry)->priority > (*b_entry)->priority)
    return 1;
  else
    return 0;
}

static int
sort_items_by_priority (gconstpointer a,
                        gconstpointer b)
{
  const IdePreferenceItemEntry * const *a_entry = a;
  const IdePreferenceItemEntry * const *b_entry = b;

  if ((*a_entry)->priority < (*b_entry)->priority)
    return -1;
  else if ((*a_entry)->priority > (*b_entry)->priority)
    return 1;
  else
    return 0;
}

static gboolean
has_children (GPtrArray  *pages,
              const char *page)
{
  for (guint i = 0; i < pages->len; i++)
    {
      IdePreferencePageEntry *entry = g_ptr_array_index (pages, i);

      if (g_strcmp0 (entry->parent, page) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
pages_header_func (GtkListBoxRow *row,
                   GtkListBoxRow *before,
                   gpointer       user_data)
{
  if (before != NULL &&
      g_object_get_data (G_OBJECT (row), "SECTION") != g_object_get_data (G_OBJECT (before), "SECTION"))
    gtk_list_box_row_set_header (row, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  else
    gtk_list_box_row_set_header (row, NULL);
}

static GtkListBoxRow *
add_page (IdePreferencesWindow         *self,
          GtkListBox                   *list_box,
          GPtrArray                    *pages,
          const IdePreferencePageEntry *entry)
{
  GtkListBoxRow *row;
  GtkImage *icon;
  GtkLabel *title;
  GtkBox *box;

  g_assert (IDE_IS_PREFERENCES_WINDOW (self));
  g_assert (entry != NULL);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      NULL);
  box = g_object_new (GTK_TYPE_BOX,
                      "spacing", 12,
                      "margin-top", 12,
                      "margin-bottom", 12,
                      "margin-start", 12,
                      "margin-end", 12,
                      NULL);
  icon = g_object_new (GTK_TYPE_IMAGE,
                       "icon-name", entry->icon_name,
                       NULL);
  title = g_object_new (GTK_TYPE_LABEL,
                        "label", entry->title,
                        "xalign", 0.0f,
                        "hexpand", TRUE,
                        NULL);
  gtk_box_append (box, GTK_WIDGET (icon));
  gtk_box_append (box, GTK_WIDGET (title));
  gtk_list_box_row_set_child (row, GTK_WIDGET (box));

  if (has_children (pages, entry->name))
    {
      GtkImage *more;

      more = g_object_new (GTK_TYPE_IMAGE,
                           "icon-name", "go-next-symbolic",
                           NULL);
      gtk_box_append (box, GTK_WIDGET (more));
    }

  g_object_set_data (G_OBJECT (row), "ENTRY", (gpointer)entry);
  g_object_set_data (G_OBJECT (row), "SECTION", (gpointer)entry->section);

  gtk_list_box_append (list_box, GTK_WIDGET (row));

  return row;
}

static void
ide_preferences_window_page_activated_cb (IdePreferencesWindow *self,
                                          GtkListBoxRow        *row,
                                          GtkListBox           *list_box)
{
  const IdePreferencePageEntry *entry;
  const IdePreferencePageEntry *parent;
  AdwPreferencesPage *page;
  GtkWidget *visible_child;

  g_assert (IDE_IS_PREFERENCES_WINDOW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  entry = g_object_get_data (G_OBJECT (row), "ENTRY");
  if (entry == self->current_page)
    return;

  self->current_page = entry;
  parent = get_page (self, entry->parent);

  visible_child = gtk_stack_get_visible_child (self->page_stack);

  adw_window_title_set_title (self->page_title, entry->title);

  if (parent != NULL)
    adw_window_title_set_title (self->pages_title, parent->title);
  else
    adw_window_title_set_title (self->pages_title, _("Preferences"));

  if (has_children (self->info.pages, entry->name))
    {
      GtkListBoxRow *subrow;
      Page *info;

      gtk_stack_set_visible_child_name (self->pages_stack, entry->name);

      info = g_object_get_data (G_OBJECT (gtk_stack_get_visible_child (self->pages_stack)), "PAGE");
      subrow = gtk_list_box_get_row_at_index (info->list_box, 0);

      gtk_widget_hide (GTK_WIDGET (self->search_button));
      gtk_widget_show (GTK_WIDGET (self->back_button));

      /* Now select the first row of the child and bail out as we'll reenter
       * this function with that row selected.
       */
      gtk_list_box_select_row (info->list_box, subrow);

      return;
    }
  else if (entry->parent == NULL)
    {
      gtk_stack_set_visible_child_name (self->pages_stack, "default");
    }

  /* First create the new page so we can transition to it. Then
   * remove the old page from a callback after the transition has
   * completed.
   */
  page = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
  adw_preferences_page_set_title (page, entry->title);
  adw_preferences_page_set_name (page, entry->name);

  /* XXX: this could be optimized with sort/binary search */
  for (guint i = 0; i < self->info.groups->len; i++)
    {
      const IdePreferenceGroupEntry *group = g_ptr_array_index (self->info.groups, i);

      if (entry_matches (group->page, entry->name))
        {
          AdwPreferencesGroup *pref_group;

          pref_group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
          adw_preferences_group_set_title (pref_group, group->title);

          /* XXX: this could be optimized with sort/binary search */
          for (guint j = 0; j < self->info.items->len; j++)
            {
              const IdePreferenceItemEntry *item = g_ptr_array_index (self->info.items, j);

              if (entry_matches (item->page, entry->name) &&
                  entry_matches (item->group, group->name))
                item->callback (entry->name, item, pref_group, (gpointer)item->user_data);
            }

          adw_preferences_page_add (page, pref_group);
        }
    }

  /* Now add the new child and transition to it */
  gtk_stack_add_child (self->page_stack, GTK_WIDGET (page));
  gtk_stack_set_visible_child (self->page_stack, GTK_WIDGET (page));

  if (visible_child != NULL)
    g_timeout_add_full (G_PRIORITY_LOW,
                        gtk_stack_get_transition_duration (self->page_stack) + 100,
                        drop_page_cb,
                        drop_page_new (self->page_stack, visible_child),
                        drop_page_free);
}

static void
create_navigation_page (IdePreferencesWindow  *self,
                        Page                 **out_page)
{
  Page *page;

  g_assert (IDE_IS_PREFERENCES_WINDOW (self));
  g_assert (out_page != NULL);

  page = g_new0 (Page, 1);
  page->box = g_object_new (GTK_TYPE_BOX,
                            "orientation", GTK_ORIENTATION_VERTICAL,
                            NULL);
  page->search_entry = g_object_new (GTK_TYPE_SEARCH_ENTRY,
                                     "hexpand", TRUE,
                                     NULL);
  g_signal_connect_object (page->search_entry,
                           "changed",
                           G_CALLBACK (search_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  page->search_bar = g_object_new (GTK_TYPE_SEARCH_BAR,
                                   "child", page->search_entry,
                                   NULL);
  page->scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                 "hscrollbar-policy", GTK_POLICY_NEVER,
                                 "vexpand", TRUE,
                                 NULL);
  page->list_box = g_object_new (GTK_TYPE_LIST_BOX,
                                 "activate-on-single-click", TRUE,
                                 "selection-mode", GTK_SELECTION_SINGLE,
                                 NULL);
  g_signal_connect_object (page->list_box,
                           "row-activated",
                           G_CALLBACK (ide_preferences_window_page_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_list_box_set_header_func (page->list_box, pages_header_func, NULL, NULL);
  gtk_widget_add_css_class (GTK_WIDGET (page->list_box), "navigation-sidebar");
  gtk_box_append (page->box, GTK_WIDGET (page->search_bar));
  gtk_box_append (page->box, GTK_WIDGET (page->scroller));
  gtk_scrolled_window_set_child (page->scroller, GTK_WIDGET (page->list_box));

  g_object_set_data_full (G_OBJECT (page->box), "PAGE", page, g_free);

  *out_page = page;
}

static void
ide_preferences_window_rebuild (IdePreferencesWindow *self)
{
  GtkListBoxRow *select_row = NULL;
  GHashTable *pages;
  Page *page;

  g_assert (IDE_IS_PREFERENCES_WINDOW (self));

  /* Remove old widgets */
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->pages_stack));
       child != NULL;
       child = gtk_widget_get_first_child (GTK_WIDGET (self->pages_stack)))
    gtk_stack_remove (self->pages_stack, child);
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->page_stack));
       child != NULL;
       child = gtk_widget_get_first_child (GTK_WIDGET (self->page_stack)))
    gtk_stack_remove (self->page_stack, child);

  /* Clear titles */
  adw_window_title_set_title (self->page_title, NULL);
  adw_window_title_set_title (self->pages_title, _("Preferences"));

  if (self->info.pages->len == 0)
    return;

  pages = g_hash_table_new (NULL, NULL);

  /* Add new pages */
  g_ptr_array_sort (self->info.pages, sort_pages_by_priority);
  g_ptr_array_sort (self->info.groups, sort_groups_by_priority);
  g_ptr_array_sort (self->info.items, sort_items_by_priority);

  create_navigation_page (self, &page);
  g_object_bind_property (self->search_button, "active",
                          page->search_bar, "search-mode-enabled",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_stack_add_named (self->pages_stack, GTK_WIDGET (page->box), "default");
  gtk_stack_set_visible_child (self->pages_stack, GTK_WIDGET (page->box));

  for (guint i = 0; i < self->info.pages->len; i++)
    {
      const IdePreferencePageEntry *entry = g_ptr_array_index (self->info.pages, i);
      GtkListBox *parent = page->list_box;
      GtkListBoxRow *row;

      if (entry->parent != NULL)
        {
          parent = g_hash_table_lookup (pages, entry->parent);

          if (parent == NULL)
            {
              Page *subpage;
              create_navigation_page (self, &subpage);
              gtk_search_bar_set_search_mode (subpage->search_bar, TRUE);
              gtk_stack_add_named (self->pages_stack, GTK_WIDGET (subpage->box), entry->parent);
              parent = subpage->list_box;
              g_hash_table_insert (pages, (gpointer)entry->parent, parent);
            }
        }

      row = add_page (self, parent, self->info.pages, entry);
      if (select_row == NULL)
        select_row = row;
    }

  /* Now select the first row */
  gtk_widget_activate (GTK_WIDGET (select_row));

  g_hash_table_unref (pages);
}

static gboolean
ide_preferences_window_rebuild_cb (gpointer data)
{
  IdePreferencesWindow *self = data;
  self->rebuild_source = 0;
  ide_preferences_window_rebuild (self);
  return G_SOURCE_REMOVE;
}

static void
ide_preferences_window_queue_rebuild (IdePreferencesWindow *self)
{
  g_assert (IDE_IS_PREFERENCES_WINDOW (self));

  if (self->rebuild_source == 0)
    self->rebuild_source = g_idle_add (ide_preferences_window_rebuild_cb, self);
}

void
ide_preferences_window_add_pages (IdePreferencesWindow         *self,
                                  const IdePreferencePageEntry *pages,
                                  gsize                         n_pages,
                                  const char                   *translation_domain)
{
  g_return_if_fail (IDE_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (pages != NULL || n_pages == 0);

  for (gsize i = 0; i < n_pages; i++)
    {
      IdePreferencePageEntry entry = pages[i];

      entry.parent = g_intern_string (entry.parent);
      entry.section = g_intern_string (entry.section);
      entry.name = g_intern_string (entry.name);
      entry.icon_name = g_intern_string (entry.icon_name);
      entry.title = g_intern_string (g_dgettext (translation_domain, entry.title));

      g_ptr_array_add (self->info.pages, g_memdup2 (&entry, sizeof entry));
    }

  ide_preferences_window_queue_rebuild (self);
}

void
ide_preferences_window_add_groups (IdePreferencesWindow          *self,
                                   const IdePreferenceGroupEntry *groups,
                                   gsize                          n_groups,
                                   const char                    *translation_domain)
{
  g_return_if_fail (IDE_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (groups != NULL || n_groups == 0);

  for (gsize i = 0; i < n_groups; i++)
    {
      IdePreferenceGroupEntry entry = groups[i];

      entry.page = g_intern_string (entry.page);
      entry.name = g_intern_string (entry.name);
      entry.title = g_intern_string (g_dgettext (translation_domain, entry.title));

      g_ptr_array_add (self->info.groups, g_memdup2 (&entry, sizeof entry));
    }

  ide_preferences_window_queue_rebuild (self);
}

static gboolean noop (gpointer data) { return G_SOURCE_REMOVE; };

void
ide_preferences_window_add_items (IdePreferencesWindow         *self,
                                  const IdePreferenceItemEntry *items,
                                  gsize                         n_items,
                                  gpointer                      user_data,
                                  GDestroyNotify                user_data_destroy)
{
  g_return_if_fail (IDE_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (items != NULL || n_items == 0);

  if (n_items == 0)
    {
      if (user_data_destroy)
        g_idle_add_full (G_PRIORITY_DEFAULT, noop, user_data, user_data_destroy);
      return;
    }

  for (gsize i = 0; i < n_items; i++)
    {
      IdePreferenceItemEntry entry = items[i];

      if (entry.callback == NULL)
        continue;

      entry.page = g_intern_string (entry.page);
      entry.group = g_intern_string (entry.group);
      entry.name = g_intern_string (entry.name);
      entry.title = g_intern_string (entry.title);
      entry.subtitle = g_intern_string (entry.subtitle);
      entry.schema_id = g_intern_string (entry.schema_id);
      entry.path = g_intern_string (entry.path);
      entry.key = g_intern_string (entry.key);
      entry.user_data = user_data;

      g_ptr_array_add (self->info.items, g_memdup2 (&entry, sizeof entry));
    }

  if (user_data_destroy)
    {
      DataDestroy data;

      data.data = user_data;
      data.notify = user_data_destroy;

      g_array_append_val (self->info.data, data);
    }

  ide_preferences_window_queue_rebuild (self);
}

void
ide_preferences_window_add_item (IdePreferencesWindow  *self,
                                 const char            *page,
                                 const char            *group,
                                 const char            *name,
                                 int                    priority,
                                 IdePreferenceCallback  callback,
                                 gpointer               user_data,
                                 GDestroyNotify         user_data_destroy)
{
  IdePreferenceItemEntry entry = {0};

  g_return_if_fail (IDE_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (page != NULL);
  g_return_if_fail (group != NULL);
  g_return_if_fail (callback != NULL);

  entry.page = g_intern_string (page);
  entry.group = g_intern_string (group);
  entry.name = g_intern_string (name);
  entry.user_data = user_data;

  g_ptr_array_add (self->info.items, g_memdup2 (&entry, sizeof entry));

  if (user_data_destroy)
    {
      DataDestroy data;

      data.data = user_data;
      data.notify = user_data_destroy;

      g_array_append_val (self->info.data, data);
    }

  ide_preferences_window_queue_rebuild (self);
}
