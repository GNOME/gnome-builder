/* cpack-completion-results.c
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

#define G_LOG_DOMAIN "cpack-completion-results"

#include "config.h"

#include <string.h>

#include "cpack-completion-item.h"
#include "cpack-completion-results.h"

struct _CpackCompletionResults
{
  GObject       parent_instance;
  GStringChunk *strings;
  GHashTable   *words;
  GPtrArray    *unfiltered;
  GArray       *items;
};

typedef struct
{
  const gchar *word;
  guint        priority;
} Item;

typedef struct
{
  GPtrArray *dirs;
} Populate;

static void list_model_iface_init             (GListModelInterface *iface);
static void cpack_completion_results_populate (IdeTask             *task);

G_DEFINE_TYPE_WITH_CODE (CpackCompletionResults, cpack_completion_results, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
populate_free (Populate *p)
{
  g_clear_pointer (&p->dirs, g_ptr_array_unref);
  g_slice_free (Populate, p);
}

static void
cpack_completion_results_finalize (GObject *object)
{
  CpackCompletionResults *self = (CpackCompletionResults *)object;

  g_clear_pointer (&self->strings, g_string_chunk_free);
  g_clear_pointer (&self->words, g_hash_table_unref);
  g_clear_pointer (&self->unfiltered, g_ptr_array_unref);
  g_clear_pointer (&self->items, g_array_unref);

  G_OBJECT_CLASS (cpack_completion_results_parent_class)->finalize (object);
}

static void
cpack_completion_results_class_init (CpackCompletionResultsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cpack_completion_results_finalize;
}

static void
cpack_completion_results_init (CpackCompletionResults *self)
{
}

static GType
cpack_completion_results_get_item_type (GListModel *model)
{
  return CPACK_TYPE_COMPLETION_ITEM;
}

static guint
cpack_completion_results_get_n_items (GListModel *model)
{
  CpackCompletionResults *self = (CpackCompletionResults *)model;

  g_assert (CPACK_IS_COMPLETION_RESULTS (self));

  return self->items ? self->items->len : 0;
}

static gpointer
cpack_completion_results_get_item (GListModel *model,
                                   guint       position)
{
  CpackCompletionResults *self = (CpackCompletionResults *)model;
  const Item *item;

  g_assert (CPACK_IS_COMPLETION_RESULTS (self));
  g_assert (position < self->items->len);

  item = &g_array_index (self->items, Item, position);

  return cpack_completion_item_new (item->word);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = cpack_completion_results_get_item_type;
  iface->get_n_items = cpack_completion_results_get_n_items;
  iface->get_item = cpack_completion_results_get_item;
}

static gint
compare_item (gconstpointer a,
              gconstpointer b)
{
  const Item *ia = a;
  const Item *ib = b;
  gint ret;

  if (!(ret = (gint)ia->priority - (gint)ib->priority))
    ret = g_strcmp0 (ia->word, ib->word);

  return ret;
}

void
cpack_completion_results_refilter (CpackCompletionResults *self,
                                   const gchar            *word)
{
  guint old_len;

  g_return_if_fail (CPACK_IS_COMPLETION_RESULTS (self));

  if (self->unfiltered == NULL || self->unfiltered->len == 0)
    return;

  old_len = self->items ? self->items->len : 0;

  if (self->items == NULL)
    self->items = g_array_new (FALSE, FALSE, sizeof (Item));
  else if (self->items->len)
    g_array_remove_range (self->items, 0, self->items->len);

  if (word == NULL)
    {
      for (guint i = 0; i < self->unfiltered->len; i++)
        {
          const gchar *ele = g_ptr_array_index (self->unfiltered, i);
          Item item = { .word = ele };

          g_array_append_val (self->items, item);
        }
    }
  else
    {
      g_autofree gchar *casefold = g_utf8_strdown (word, -1);

      for (guint i = 0; i < self->unfiltered->len; i++)
        {
          const gchar *ele = g_ptr_array_index (self->unfiltered, i);
          guint priority;

          if (ide_completion_fuzzy_match (ele, casefold, &priority))
            {
              Item item = { .word = ele, .priority = priority };

              g_array_append_val (self->items, item);
            }
        }
    }

  g_array_sort (self->items, compare_item);

  if (old_len || self->items->len)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->items->len);
}

static gboolean
is_headerish (const gchar *name)
{
  const gchar *dot = strrchr (name, '.');

  return dot &&
         (strcmp (dot, ".h") == 0 ||
          strcmp (dot, ".hh") == 0 ||
          strcmp (dot, ".hpp") == 0 ||
          strcmp (dot, ".hxx") == 0 ||
          strcmp (dot, ".defs") == 0);
}

static void
cpack_completion_results_populate_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GFile *file = (GFile *)object;
  CpackCompletionResults *self;
  g_autoptr(GPtrArray) children = NULL;
  g_autoptr(IdeTask) task = user_data;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (CPACK_IS_COMPLETION_RESULTS (self));

  children = ide_g_file_get_children_finish (file, result, NULL);
  IDE_PTR_ARRAY_SET_FREE_FUNC (children, g_object_unref);

  if (children && children->len)
    {
      if (self->unfiltered == NULL)
        self->unfiltered = g_ptr_array_new ();

      if (self->strings == NULL)
        self->strings = g_string_chunk_new (4096);

      if (self->words == NULL)
        self->words = g_hash_table_new (g_str_hash, g_str_equal);

      for (guint i = 0; i < children->len; i++)
        {
          g_autofree gchar *as_dir = NULL;
          GFileInfo *info = g_ptr_array_index (children, i);
          const gchar *name = g_file_info_get_name (info);
          const gchar *word;

          g_assert (G_IS_FILE_INFO (info));

          if (name == NULL)
            continue;

          if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
            name = as_dir = g_strdup_printf ("%s/", name);
          else if (!is_headerish (name))
            continue;

          if (!(word = g_hash_table_lookup (self->words, name)))
            {
              word = g_string_chunk_insert (self->strings, name);
              g_hash_table_add (self->words, (gchar *)word);
              g_ptr_array_add (self->unfiltered, (gchar *)word);
            }
        }
    }

  cpack_completion_results_populate (task);
}

static void
cpack_completion_results_populate (IdeTask *task)
{
  CpackCompletionResults *self;
  g_autoptr(GFile) dir = NULL;
  Populate *p;

  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  p = ide_task_get_task_data (task);

  g_assert (CPACK_IS_COMPLETION_RESULTS (self));
  g_assert (p != NULL);
  g_assert (p->dirs != NULL);

  if (p->dirs->len == 0)
    {
      if (self->unfiltered != NULL)
        cpack_completion_results_refilter (self, NULL);
      ide_task_return_boolean (task, TRUE);
      return;
    }

  dir = g_steal_pointer (&g_ptr_array_index (p->dirs, p->dirs->len - 1));
  p->dirs->len--;

  ide_g_file_get_children_async (dir,
                                 G_FILE_ATTRIBUTE_STANDARD_NAME","
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 ide_task_get_cancellable (task),
                                 cpack_completion_results_populate_cb,
                                 g_object_ref (task));
}

void
cpack_completion_results_populate_async (CpackCompletionResults *self,
                                         const gchar * const    *build_flags,
                                         const gchar            *prefix,
                                         GCancellable           *cancellable,
                                         GAsyncReadyCallback     callback,
                                         gpointer                user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Populate *p;
  guint old_len = 0;

  g_return_if_fail (CPACK_IS_COMPLETION_RESULTS (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (prefix && !*prefix)
    prefix = NULL;

  /*
   * We only want to deal with the base path, no trailing input for
   * anything additional typed.
   */
  if (prefix != NULL && !g_str_has_suffix (prefix, "/"))
    prefix = NULL;

  p = g_slice_new0 (Populate);
  p->dirs = g_ptr_array_new_with_free_func (g_object_unref);

  if (self->words)
    g_hash_table_remove_all (self->words);

  if (self->strings)
    g_string_chunk_clear (self->strings);

  if (self->unfiltered && self->unfiltered->len)
    g_ptr_array_remove_range (self->unfiltered, 0, self->unfiltered->len);

  if (self->items && self->items->len)
    {
      old_len = self->items->len;
      g_array_remove_range (self->items, 0, self->items->len);
    }

  if (build_flags)
    {
      for (guint i = 0; build_flags[i]; i++)
        {
          const gchar *arg = build_flags[i];

          /*
           * All of our CFLAGS should be changed -Ipath at this point. Due to
           * translations for cross-container paths.
           */

          if (g_str_has_prefix (arg, "-I") && arg[2])
            {
              const gchar *path = &arg[2];
              g_autofree gchar *subdir = g_build_filename (path, prefix, NULL);

              g_ptr_array_add (p->dirs, g_file_new_for_path (subdir));
            }
        }
    }

  if (old_len)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, 0);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, cpack_completion_results_populate_async);
  ide_task_set_task_data (task, p, populate_free);

  cpack_completion_results_populate (task);
}

gboolean
cpack_completion_results_populate_finish (CpackCompletionResults  *self,
                                          GAsyncResult            *result,
                                          GError                 **error)
{
  g_return_val_if_fail (CPACK_IS_COMPLETION_RESULTS (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
