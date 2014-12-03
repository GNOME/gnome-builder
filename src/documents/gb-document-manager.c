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

#include "gb-document-manager.h"

G_DEFINE_TYPE (GbDocumentManager, gb_document_manager, GTK_TYPE_LIST_STORE)

enum {
  COLUMN_DOCUMENT,
  LAST_COLUMN
};

GbDocumentManager *
gb_document_manager_new (void)
{
  return g_object_new (GB_TYPE_DOCUMENT_MANAGER, NULL);
}

gboolean
gb_document_manager_find_document (GbDocumentManager *manager,
                                   GbEditorDocument  *document,
                                   GtkTreeIter       *iter)
{
  g_return_val_if_fail (GB_IS_DOCUMENT_MANAGER (manager), FALSE);
  g_return_val_if_fail (GB_IS_EDITOR_DOCUMENT (document), FALSE);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (manager), iter))
    {
      do
        {
          GValue value = { 0 };

          gtk_tree_model_get_value (GTK_TREE_MODEL (manager), iter,
                                    COLUMN_DOCUMENT, &value);

          if (G_VALUE_HOLDS_OBJECT (&value) &&
              (g_value_get_object (&value) == (void *)document))
            {
              g_value_unset (&value);
              return TRUE;
            }

          g_value_unset (&value);
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (manager), iter));
    }

  return FALSE;
}

void
gb_document_manager_document_changed (GbDocumentManager *manager,
                                      GbEditorDocument  *document)
{
  GtkTreeIter iter;

  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (manager));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  if (gb_document_manager_find_document (manager, document, &iter))
    {
      GtkTreePath *tree_path;

      tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (manager), &iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (manager), tree_path, &iter);
      gtk_tree_path_free (tree_path);
    }
}

static void
gb_document_manager_on_notify_dirty (GbDocumentManager *manager,
                                     GParamSpec        *pspec,
                                     GbEditorDocument  *document)
{
  gb_document_manager_document_changed (manager, document);
}

static void
gb_document_manager_on_notify_title (GbDocumentManager *manager,
                                     GParamSpec        *pspec,
                                     GbEditorDocument  *document)
{
  gb_document_manager_document_changed (manager, document);
}

/**
 * gb_document_manager_add_document:
 * @manager: A #GbDocumentManager.
 * @document: A #GbEditorDocument.
 *
 * Adds A #GbEditorDocument to the collection of buffers managed by the
 * #GbDocumentManager instance.
 */
void
gb_document_manager_add_document (GbDocumentManager *manager,
                                  GbEditorDocument  *document)
{
  GtkTreeIter iter;

  g_return_if_fail (GB_IS_DOCUMENT_MANAGER (manager));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  g_signal_connect_object (document,
                           "notify::title",
                           G_CALLBACK (gb_document_manager_on_notify_title),
                           manager,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (document,
                           "notify::dirty",
                           G_CALLBACK (gb_document_manager_on_notify_dirty),
                           manager,
                           G_CONNECT_SWAPPED);

  gtk_list_store_append (GTK_LIST_STORE (manager), &iter);
  gtk_list_store_set (GTK_LIST_STORE (manager), &iter,
                      COLUMN_DOCUMENT, document,
                      -1);
}

/**
 * gb_document_manager_remove_document:
 *
 * Removes the #GbEditorDocument instance @document from being managed by
 * the #GbDocumentManager instance.
 *
 * Returns: %TRUE if the document was removed.
 */
gboolean
gb_document_manager_remove_document (GbDocumentManager *manager,
                                     GbEditorDocument  *document)
{
  GtkTreeIter iter;

  g_return_val_if_fail (GB_IS_DOCUMENT_MANAGER (manager), FALSE);
  g_return_val_if_fail (GB_IS_EDITOR_DOCUMENT (document), FALSE);

  if (gb_document_manager_find_document (manager, document, &iter))
    {
      gtk_list_store_remove (GTK_LIST_STORE (manager), &iter);
      return TRUE;
    }

  return FALSE;
}

static void
gb_document_manager_class_init (GbDocumentManagerClass *klass)
{
}

static void
gb_document_manager_init (GbDocumentManager *self)
{
}
