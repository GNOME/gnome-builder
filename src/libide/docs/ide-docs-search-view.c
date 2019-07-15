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
#include <glib/gi18n.h>
#include <libide-gui.h>
#include <libide-threading.h>

#include "ide-docs-library.h"
#include "ide-docs-search-section.h"
#include "ide-docs-search-view.h"

struct _IdeDocsSearchView
{
  GtkBin             parent_instance;

  /* The most recent full result set, so that we can go back after
   * viewing a specific set and going backwards.
   */
  IdeDocsItem       *full_set;

  GtkScrolledWindow *scroller;
  DzlPriorityBox    *sections;
  DzlPriorityBox    *titles;
};

G_DEFINE_TYPE (IdeDocsSearchView, ide_docs_search_view, GTK_TYPE_BIN)

enum {
  ITEM_ACTIVATED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
on_go_previous_clicked_cb (IdeDocsSearchView *self,
                           GtkButton         *button)
{
  g_assert (IDE_IS_DOCS_SEARCH_VIEW (self));
  g_assert (GTK_IS_BUTTON (button));

  if (self->full_set != NULL)
    ide_docs_search_view_add_sections (self, self->full_set);
}

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
      GtkBox *box;
      GtkBox *vbox;

      gtk_container_add_with_properties (GTK_CONTAINER (self->sections), child,
                                         "priority", priority,
                                         NULL);

      vbox = g_object_new (GTK_TYPE_BOX,
                           "orientation", GTK_ORIENTATION_VERTICAL,
                           "visible", TRUE,
                           NULL);
      gtk_container_add (GTK_CONTAINER (self->titles), GTK_WIDGET (vbox));

      box = g_object_new (GTK_TYPE_BOX,
                          "orientation", GTK_ORIENTATION_HORIZONTAL,
                          "spacing", 6,
                          "visible", TRUE,
                          NULL);

      if (ide_docs_search_section_get_show_all_results (IDE_DOCS_SEARCH_SECTION (child)))
        {
          GtkImage *image;
          GtkButton *button;

          button = g_object_new (GTK_TYPE_BUTTON,
                                 "focus-on-click", FALSE,
                                 "halign", GTK_ALIGN_END,
                                 "valign", GTK_ALIGN_START,
                                 "visible", TRUE,
                                 NULL);
          g_signal_connect_object (button,
                                   "clicked",
                                   G_CALLBACK (on_go_previous_clicked_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
          dzl_gtk_widget_add_style_class (GTK_WIDGET (button), "image-button");
          dzl_gtk_widget_add_style_class (GTK_WIDGET (button), "flat");
          gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (button));
          gtk_container_add (GTK_CONTAINER (button), GTK_WIDGET (box));

          image = g_object_new (GTK_TYPE_IMAGE,
                                "icon-name", "go-previous-symbolic",
                                "pixel-size", 16,
                                "visible", TRUE,
                                NULL);
          gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (image));
        }
      else
        {
          gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (box));
        }

      label = g_object_new (GTK_TYPE_LABEL,
                            "label", title,
                            "visible", TRUE,
                            NULL);
      gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (label));

      group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
      gtk_size_group_add_widget (group, GTK_WIDGET (vbox));
      gtk_size_group_add_widget (group, GTK_WIDGET (child));
      g_object_unref (group);

      gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (self->scroller), 0);

      return;
    }

  GTK_CONTAINER_CLASS (ide_docs_search_view_parent_class)->add (container, child);
}

static void
ide_docs_search_view_finalize (GObject *object)
{
  IdeDocsSearchView *self = (IdeDocsSearchView *)object;

  g_assert (IDE_IS_DOCS_SEARCH_VIEW (self));

  g_clear_object (&self->full_set);

  G_OBJECT_CLASS (ide_docs_search_view_parent_class)->finalize (object);
}

static void
ide_docs_search_view_class_init (IdeDocsSearchViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_docs_search_view_finalize;

  container_class->add = ide_docs_search_view_add;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-docs/ui/ide-docs-search-view.ui");
  gtk_widget_class_set_css_name (widget_class, "IdeDocsSearchView");
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchView, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchView, sections);
  gtk_widget_class_bind_template_child (widget_class, IdeDocsSearchView, titles);

  /**
   * IdeDocsSearchView::item-activated:
   * @self: an #IdeDocsSearchView
   * @item: an #IdeDocsItem
   *
   * The "item-activated" signal is emitted when a documentation item
   * has been activated and should be displayed to the user.
   *
   * Since: 3.34
   */
  signals [ITEM_ACTIVATED] =
    g_signal_new ("item-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DOCS_ITEM);
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

static void
on_item_activated_cb (IdeDocsSearchView    *self,
                      IdeDocsItem          *item,
                      IdeDocsSearchSection *old_section)
{
  g_assert (IDE_IS_DOCS_SEARCH_VIEW (self));
  g_assert (IDE_IS_DOCS_ITEM (item));
  g_assert (IDE_IS_DOCS_SEARCH_SECTION (old_section));

  if (ide_docs_item_has_child (item))
    {
      IdeDocsSearchSection *section;

      ide_docs_search_view_clear (self);

      section = g_object_new (IDE_TYPE_DOCS_SEARCH_SECTION,
                              "show-all-results", TRUE,
                              "title", _("All Search Results"),
                              NULL);
      ide_docs_search_section_add_groups (section, item);
      g_signal_connect_object (section,
                               "item-activated",
                               G_CALLBACK (on_item_activated_cb),
                               self,
                               G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (section));
      gtk_widget_show (GTK_WIDGET (section));
    }
  else
    {
      g_signal_emit (self, signals [ITEM_ACTIVATED], 0, item);
    }
}

void
ide_docs_search_view_add_sections (IdeDocsSearchView *self,
                                   IdeDocsItem       *item)
{
  g_assert (IDE_IS_DOCS_SEARCH_VIEW (self));
  g_assert (!item || IDE_IS_DOCS_ITEM (item));

  ide_docs_search_view_clear (self);

  g_set_object (&self->full_set, item);

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
      g_signal_connect_object (section,
                               "item-activated",
                               G_CALLBACK (on_item_activated_cb),
                               self,
                               G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (section));
      gtk_widget_show (GTK_WIDGET (section));
    }

  gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (self->scroller), 0);
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
