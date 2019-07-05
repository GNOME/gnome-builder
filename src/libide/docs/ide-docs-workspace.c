/* ide-docs-workspace.c
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

#define G_LOG_DOMAIN "ide-docs-workspace"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-plugins.h>
#include <libide-threading.h>

#include "ide-docs-item.h"
#include "ide-docs-library.h"
#include "ide-docs-provider.h"
#include "ide-docs-search-view.h"
#include "ide-docs-workspace.h"

struct _IdeDocsWorkspace
{
  IdeWorkspace            parent_instance;

  /* Template Widgets */
  IdeDocsSearchView      *search_view;
  GtkEntry               *entry;
};

G_DEFINE_TYPE (IdeDocsWorkspace, ide_docs_workspace, IDE_TYPE_WORKSPACE)

/**
 * ide_docs_workspace_new:
 *
 * Create a new #IdeDocsWorkspace.
 *
 * Returns: (transfer full): a newly created #IdeDocsWorkspace
 */
IdeWorkspace *
ide_docs_workspace_new (IdeApplication *application)
{
  return g_object_new (IDE_TYPE_DOCS_WORKSPACE,
                       "application", application,
                       "default-width", 800,
                       "default-height", 600,
                       NULL);
}

static void
on_search_entry_changed_cb (IdeDocsWorkspace *self,
                            GtkEntry         *entry)
{
  g_autoptr(IdeDocsQuery) query = NULL;
  const gchar *text;

  g_assert (IDE_IS_DOCS_WORKSPACE (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);

  if (ide_str_empty0 (text))
    return;

  query = ide_docs_query_new ();
  ide_docs_query_set_keyword (query, text);

  ide_docs_search_view_search_async (self->search_view,
                                     query,
                                     NULL,
                                     NULL,
                                     NULL);
}

static void
on_search_view_item_activated_cb (IdeDocsWorkspace  *self,
                                  IdeDocsItem       *item,
                                  IdeDocsSearchView *view)
{
  g_assert (IDE_IS_DOCS_WORKSPACE (self));
  g_assert (IDE_IS_DOCS_ITEM (item));
  g_assert (IDE_IS_DOCS_SEARCH_VIEW (view));

  g_print ("Activate view for %s at %s\n",
           ide_docs_item_get_title (item),
           ide_docs_item_get_url (item));
}

static void
ide_docs_workspace_class_init (IdeDocsWorkspaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-docs/ui/ide-docs-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDocsWorkspace, entry);
  gtk_widget_class_bind_template_child (widget_class, IdeDocsWorkspace, search_view);

  ide_workspace_class_set_kind (workspace_class, "docs");

  g_type_ensure (IDE_TYPE_DOCS_SEARCH_VIEW);
}

static void
ide_docs_workspace_init (IdeDocsWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->search_view,
                           "item-activated",
                           G_CALLBACK (on_search_view_item_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (on_search_entry_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
