/* ide-docs-search-view.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-docs-search-view"

#include "config.h"

#include <dazzle.h>
#include <libide-gui.h>
#include <libide-threading.h>

#include "ide-docs-library.h"
#include "ide-docs-search-section.h"
#include "ide-docs-search-view.h"

struct _IdeDocsSearchView
{
  GtkBin          parent_instance;

  DzlPriorityBox *sections;
  DzlPriorityBox *titles;
};

G_DEFINE_TYPE (IdeDocsSearchView, ide_docs_search_view, GTK_TYPE_BIN)

static void
ide_docs_search_view_add (GtkContainer *container,
                          GtkWidget    *child)
{
  IdeDocsSearchView *self = (IdeDocsSearchView *)container;

  g_assert (IDE_IS_DOCS_SEARCH_VIEW (self));
  g_assert (GTK_IS_WIDGET (child));

  if (IDE_IS_DOCS_SEARCH_SECTION (child))
    {
      const gchar *title = ide_docs_search_section_get_title (IDE_DOCS_SEARCH_SECTION (child));
      gint priority = ide_docs_search_section_get_priority (IDE_DOCS_SEARCH_SECTION (child));
      GtkSizeGroup *group;
      GtkLabel *label;

      label = g_object_new (GTK_TYPE_LABEL,
                            "label", title,
                            "xalign", 1.0f,
                            "yalign", 0.0f,
                            "visible", TRUE,
                            NULL);
      gtk_container_add (GTK_CONTAINER (self->titles), GTK_WIDGET (label));
      gtk_container_add_with_properties (GTK_CONTAINER (self->sections), child,
                                         "priority", priority,
                                         NULL);
      group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
      gtk_size_group_add_widget (group, GTK_WIDGET (label));
      gtk_size_group_add_widget (group, GTK_WIDGET (child));
      g_object_unref (group);
      return;
    }

  GTK_CONTAINER_CLASS (ide_docs_search_view_parent_class)->add (container, child);
}

static void
ide_docs_search_view_class_init (IdeDocsSearchViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  container_class->add = ide_docs_search_view_add;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-docs/ui/ide-docs-search-view.ui");
  gtk_widget_class_set_css_name (widget_class, "IdeDocsSearchView");
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchView, sections);
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchView, titles);
}

static void
ide_docs_search_view_init (IdeDocsSearchView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ide_docs_search_view_clear (IdeDocsSearchView *self)
{
  g_assert (IDE_IS_DOCS_SEARCH_VIEW (self));

  gtk_container_foreach (GTK_CONTAINER (self->sections),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);
  gtk_container_foreach (GTK_CONTAINER (self->titles),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);
}

void
ide_docs_search_view_add_sections (IdeDocsSearchView *self,
                                   IdeDocsItem       *item)
{
  g_assert (IDE_IS_DOCS_SEARCH_VIEW (self));
  g_assert (!item || IDE_IS_DOCS_ITEM (item));

  ide_docs_search_view_clear (self);

  if (item == NULL)
    return;

  /* The root IdeDocsItem contains the children which are groups,
   * each containing the children within that category.
   *
   * For each group, we create a new searchgroup to contain the
   * children items. If there are too many items to display, we
   * let the user know how many items are in the group and provide
   * a button to click to show the additional items.
   */

  for (const GList *iter = ide_docs_item_get_children (item);
       iter != NULL;
       iter = iter->next)
    {
      IdeDocsItem *child = iter->data;
      IdeDocsSearchSection *section;
      const gchar *title;
      gint priority;

      g_assert (IDE_IS_DOCS_ITEM (child));

      /* Ignore children that have no items */
      if (ide_docs_item_get_n_children (child) == 0)
        continue;

      /* Create a new group with the title */
      title = ide_docs_item_get_title (child);
      priority = ide_docs_item_get_priority (child);
      section = g_object_new (IDE_TYPE_DOCS_SEARCH_SECTION,
                              "title", title,
                              "priority", priority,
                              NULL);
      ide_docs_search_section_add_groups (section, child);
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (section));
      gtk_widget_show (GTK_WIDGET (section));
    }
}

GtkWidget *
ide_docs_search_view_new (void)
{
  return g_object_new (IDE_TYPE_DOCS_SEARCH_VIEW, NULL);
}

static void
ide_docs_search_view_search_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeDocsLibrary *library = (IdeDocsLibrary *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeDocsItem) res = NULL;
  g_autoptr(GError) error = NULL;
  IdeDocsSearchView *self;
  IdeDocsItem *results;

  g_assert (IDE_IS_DOCS_LIBRARY (library));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_docs_library_search_finish (library, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  self = ide_task_get_source_object (task);
  results = ide_task_get_task_data (task);

  ide_docs_search_view_add_sections (self, results);
}

void
ide_docs_search_view_search_async (IdeDocsSearchView   *self,
                                   IdeDocsQuery        *query,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeDocsItem) results = NULL;
  IdeDocsLibrary *library;
  IdeContext *context;

  g_return_if_fail (IDE_IS_DOCS_SEARCH_VIEW (self));
  g_return_if_fail (IDE_IS_DOCS_QUERY (query));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_widget_get_context (GTK_WIDGET (self));
  library = ide_docs_library_from_context (context);
  results = ide_docs_item_new ();

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_docs_search_view_search_async);
  ide_task_set_task_data (task, g_object_ref (results), g_object_unref);

  ide_docs_library_search_async (library,
                                 query,
                                 results,
                                 cancellable,
                                 ide_docs_search_view_search_cb,
                                 g_steal_pointer (&task));
}

gboolean
ide_docs_search_view_search_finish (IdeDocsSearchView  *self,
                                    GAsyncResult       *result,
                                    GError            **error)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_VIEW (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
