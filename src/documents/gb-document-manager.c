/* gb-document-manager.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "document-manager"

#include <gtksourceview/gtksource.h>

#include "gb-document-manager.h"
#include "gb-editor-document.h"

struct _GbDocumentManagerPrivate
{
  GPtrArray *documents;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDocumentManager, gb_document_manager,
                            G_TYPE_OBJECT)

enum {
  DOCUMENT_ADDED,
  DOCUMENT_REMOVED,
  DOCUMENT_MODIFIED_CHANGED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

GbDocumentManager *
gb_document_manager_new (void)
{
  return g_object_new (GB_TYPE_DOCUMENT_MANAGER, NULL);
}

GbDocumentManager *
gb_document_manager_get_default (void)
{
  static GbDocumentManager *instance;

  if (!instance)
    instance = gb_document_manager_new ();

  return instance;
}

guint
gb_document_manager_get_count (GbDocumentManager *manager)
{
  g_return_val_if_fail (GB_IS_DOCUMENT_MANAGER (manager), 0);

  return manager->priv->documents->len;
}

GbDocument *
gb_document_manager_find_with_file (GbDocumentManager *manager,
                                    GFile             *file)
{
  guint i;

  g_return_val_if_fail (GB_IS_DOCUMENT_MANAGER (manager), NULL);

  for (i = 0; i < manager->priv->documents->len; i++)
    {
      GbDocument *document;

      document = g_ptr_array_index (manager->priv->documents, i);

      if (GB_IS_EDITOR_DOCUMENT (document))
        {
          GtkSourceFile *sfile;
          GFile *location;

          sfile = gb_editor_document_get_file (GB_EDITOR_DOCUMENT (document));
          location = gtk_source_file_get_location (sfile);

          if (g_file_equal (location, file))
            return document;
        }
    }

  return NULL;
}

/**
 * gb_document_manager_get_documents:
 *
 * Fetches a #GList of all the documents loaded by #GbDocumentManager.
 *
 * Returns: (transfer container) (element-type GbDocument*): #GList of
 *   #GbDocument. Free list with g_list_free().
 */
GList *
gb_document_manager_get_documents (GbDocumentManager *manager)
{
  GList *list = NULL;
  guint i;

  g_return_val_if_fail (GB_IS_DOCUMENT_MANAGER (manager), NULL);

  for (i = 0; i < manager->priv->documents->len; i++)
    {
      GbDocument *document;

      document = g_ptr_array_index (manager->priv->documents, i);
      list = g_list_prepend (list, document);
    }

  return list;
}

static void
gb_document_manager_document_modified (GbDocumentManager *manager,
                                       GParamSpec        *pspec,
                                       GbDocument        *document)
{
  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (manager));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  g_signal_emit (manager, gSignals [DOCUMENT_MODIFIED_CHANGED], 0, document);
}

void
gb_document_manager_add (GbDocumentManager *manager,
                         GbDocument        *document)
{
  guint i;

  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (manager));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  for (i = 0; i < manager->priv->documents->len; i++)
    {
      GbDocument *item;

      item = g_ptr_array_index (manager->priv->documents, i);

      if (item == document)
        {
          g_warning ("GbDocumentManager already contains document \"%s\"",
                     gb_document_get_title (document));
          return;
        }
    }

  g_signal_connect_object (document,
                           "notify::modified",
                           G_CALLBACK (gb_document_manager_document_modified),
                           manager,
                           G_CONNECT_SWAPPED);

  g_ptr_array_add (manager->priv->documents, g_object_ref (document));

  g_signal_emit (manager, gSignals [DOCUMENT_ADDED], 0, document);
}

void
gb_document_manager_remove (GbDocumentManager *manager,
                            GbDocument        *document)
{
  guint i;

  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (manager));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  for (i = 0; i < manager->priv->documents->len; i++)
    {
      GbDocument *item;

      item = g_ptr_array_index (manager->priv->documents, i);

      if (item == document)
        {
          g_signal_handlers_disconnect_by_func (item,
                                                gb_document_manager_document_modified,
                                                manager);
          g_ptr_array_remove_index_fast (manager->priv->documents, i);
          g_signal_emit (manager, gSignals [DOCUMENT_REMOVED], 0, item);
          g_object_unref (item);
          break;
        }
    }
}

static void
gb_document_manager_finalize (GObject *object)
{
  GbDocumentManagerPrivate *priv = GB_DOCUMENT_MANAGER (object)->priv;
  GbDocumentManager *manager = (GbDocumentManager *)object;

  while (priv->documents->len)
    {
      GbDocument *document;

      document = GB_DOCUMENT (g_ptr_array_index (priv->documents, 0));
      gb_document_manager_remove (manager, document);
    }

  g_clear_pointer (&priv->documents, g_ptr_array_unref);

  G_OBJECT_CLASS (gb_document_manager_parent_class)->finalize (object);
}

static void
gb_document_manager_class_init (GbDocumentManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_document_manager_finalize;

  gSignals [DOCUMENT_ADDED] =
    g_signal_new ("document-added",
                  GB_TYPE_DOCUMENT_MANAGER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GbDocumentManagerClass, document_added),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_DOCUMENT);

  gSignals [DOCUMENT_MODIFIED_CHANGED] =
    g_signal_new ("document-modified-changed",
                  GB_TYPE_DOCUMENT_MANAGER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GbDocumentManagerClass,
                                   document_modified_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_DOCUMENT);

  gSignals [DOCUMENT_REMOVED] =
    g_signal_new ("document-removed",
                  GB_TYPE_DOCUMENT_MANAGER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GbDocumentManagerClass, document_removed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_DOCUMENT);
}

static void
gb_document_manager_init (GbDocumentManager *self)
{
  self->priv = gb_document_manager_get_instance_private (self);
  self->priv->documents = g_ptr_array_new ();
}
