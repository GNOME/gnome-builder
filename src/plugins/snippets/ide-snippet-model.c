/* ide-snippet-model.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-snippet-model"

#include "config.h"

#include "ide-snippet-model.h"
#include "ide-snippet-completion-item.h"

struct _IdeSnippetModel
{
  GObject            parent_instance;
  IdeSnippetStorage *storage;
  GPtrArray         *items;
  gchar             *prefix;
  gchar             *language;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeSnippetModel, ide_snippet_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ide_snippet_model_finalize (GObject *object)
{
  IdeSnippetModel *self = (IdeSnippetModel *)object;

  g_clear_pointer (&self->language, g_free);
  g_clear_pointer (&self->prefix, g_free);
  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_object (&self->storage);

  G_OBJECT_CLASS (ide_snippet_model_parent_class)->finalize (object);
}

static void
ide_snippet_model_class_init (IdeSnippetModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_snippet_model_finalize;
}

static void
ide_snippet_model_init (IdeSnippetModel *self)
{
  self->items = g_ptr_array_new ();
}

IdeSnippetModel *
ide_snippet_model_new (IdeSnippetStorage *storage)
{
  IdeSnippetModel *self;

  self = g_object_new (IDE_TYPE_SNIPPET_MODEL, NULL);
  self->storage = g_object_ref (storage);

  return self;
}

static gint
compare_items (gconstpointer a,
               gconstpointer b)
{
  const IdeSnippetInfo *ai = *(const IdeSnippetInfo **)a;
  const IdeSnippetInfo *bi = *(const IdeSnippetInfo **)b;

  /* At this point, everything matches prefix, so we just want
   * to use the shorter string.
   */

  return (gint)strlen (ai->name) - (gint)strlen (bi->name);
}

static void
foreach_cb (IdeSnippetStorage    *storage,
            const IdeSnippetInfo *info,
            gpointer              user_data)
{
  IdeSnippetModel *self = user_data;
  /* You can only add items to storage, and the pointer is
   * guaranteed alive while we own self->storage.
   */
  g_ptr_array_add (self->items, (gpointer)info);
}

static void
ide_snippet_model_update (IdeSnippetModel *self)
{
  guint old_len;

  g_assert (IDE_IS_SNIPPET_MODEL (self));

  old_len = self->items->len;

  if (self->items->len)
    g_ptr_array_remove_range (self->items, 0, self->items->len);

  ide_snippet_storage_query (self->storage, self->language, self->prefix, foreach_cb, self);

  g_ptr_array_sort (self->items, compare_items);

  if (old_len || self->items->len)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->items->len);
}

void
ide_snippet_model_set_prefix (IdeSnippetModel *self,
                              const gchar     *prefix)
{
  g_return_if_fail (IDE_IS_SNIPPET_MODEL (self));

  if (g_strcmp0 (prefix, self->prefix) != 0)
    {
      g_free (self->prefix);
      self->prefix = g_strdup (prefix);
      ide_snippet_model_update (self);
    }
}

void
ide_snippet_model_set_language (IdeSnippetModel *self,
                                const gchar     *language)
{
  g_return_if_fail (IDE_IS_SNIPPET_MODEL (self));

  if (g_strcmp0 (language, self->language) != 0)
    {
      g_free (self->language);
      self->language = g_strdup (language);
      ide_snippet_model_update (self);
    }
}

static GType
ide_snippet_model_get_item_type (GListModel *model)
{
  return IDE_TYPE_SNIPPET_COMPLETION_ITEM;
}

static guint
ide_snippet_model_get_n_items (GListModel *model)
{
  return IDE_SNIPPET_MODEL (model)->items->len;
}

static gpointer
ide_snippet_model_get_item (GListModel *model,
                            guint       position)
{
  IdeSnippetModel *self = IDE_SNIPPET_MODEL (model);
  const IdeSnippetInfo *info = g_ptr_array_index (self->items, position);
  return ide_snippet_completion_item_new (self->storage, info);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_snippet_model_get_item_type;
  iface->get_n_items = ide_snippet_model_get_n_items;
  iface->get_item = ide_snippet_model_get_item;
}
