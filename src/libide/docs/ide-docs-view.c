/* ide-docs-view.c
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

#define G_LOG_DOMAIN "ide-docs-view"

#include "config.h"

#include <string.h>
#include <webkit2/webkit2.h>

#include "ide-docs-view.h"

struct _IdeDocsView
{
  GtkBin parent_instance;

  WebKitWebView *web_view;
};

G_DEFINE_TYPE (IdeDocsView, ide_docs_view, GTK_TYPE_BIN)

static void
ide_docs_view_class_init (IdeDocsViewClass *klass)
{
}

static void
ide_docs_view_init (IdeDocsView *self)
{
  self->web_view = g_object_new (WEBKIT_TYPE_WEB_VIEW,
                                 "expand", TRUE,
                                 "visible", TRUE,
                                 NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->web_view));
}

GtkWidget *
ide_docs_view_new (void)
{
  return g_object_new (IDE_TYPE_DOCS_VIEW, NULL);
}

void
ide_docs_view_set_item (IdeDocsView *self,
                        IdeDocsItem *item)
{
  const gchar *url;

  g_return_if_fail (IDE_IS_DOCS_VIEW (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (item));

  if ((url = ide_docs_item_get_url (item)))
    {
      g_autofree gchar *generated = NULL;

      if (strstr (url, "://") == NULL)
        {
          IdeDocsItem *parent;

          if ((parent = ide_docs_item_get_parent (item)))
            {
              const gchar *purl = ide_docs_item_get_url (parent);

              if (purl != NULL)
                url = generated = g_strdup_printf ("file://%s/%s", purl, url);
            }
        }

      g_print ("%s\n", url);

      webkit_web_view_load_uri (self->web_view, url);
    }
}
