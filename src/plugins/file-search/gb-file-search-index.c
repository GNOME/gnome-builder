/* gb-file-search-index.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-file-search-index"

#include <glib/gi18n.h>
#include <ide.h>
#include <string.h>

#include "gb-file-search-index.h"
#include "gb-file-search-result.h"

struct _GbFileSearchIndex
{
  IdeObject             parent_instance;

  GFile                *root_directory;
  DzlFuzzyMutableIndex *fuzzy;
};

G_DEFINE_TYPE (GbFileSearchIndex, gb_file_search_index, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ROOT_DIRECTORY,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
gb_file_search_index_set_root_directory (GbFileSearchIndex *self,
                                         GFile             *root_directory)
{
  g_return_if_fail (GB_IS_FILE_SEARCH_INDEX (self));
  g_return_if_fail (!root_directory || G_IS_FILE (root_directory));

  if (g_set_object (&self->root_directory, root_directory))
    {
      g_clear_pointer (&self->fuzzy, dzl_fuzzy_mutable_index_unref);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ROOT_DIRECTORY]);
    }
}

static void
gb_file_search_index_finalize (GObject *object)
{
  GbFileSearchIndex *self = (GbFileSearchIndex *)object;

  g_clear_object (&self->root_directory);
  g_clear_pointer (&self->fuzzy, dzl_fuzzy_mutable_index_unref);

  G_OBJECT_CLASS (gb_file_search_index_parent_class)->finalize (object);
}

static void
gb_file_search_index_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbFileSearchIndex *self = GB_FILE_SEARCH_INDEX (object);

  switch (prop_id)
    {
    case PROP_ROOT_DIRECTORY:
      g_value_set_object (value, self->root_directory);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_file_search_index_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbFileSearchIndex *self = GB_FILE_SEARCH_INDEX (object);

  switch (prop_id)
    {
    case PROP_ROOT_DIRECTORY:
      gb_file_search_index_set_root_directory (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_file_search_index_class_init (GbFileSearchIndexClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_file_search_index_finalize;
  object_class->get_property = gb_file_search_index_get_property;
  object_class->set_property = gb_file_search_index_set_property;

  properties [PROP_ROOT_DIRECTORY] =
    g_param_spec_object ("root-directory",
                         "Root Directory",
                         "Root Directory",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_file_search_index_init (GbFileSearchIndex *self)
{
}

static void
populate_from_dir (DzlFuzzyMutableIndex *fuzzy,
                   IdeVcs               *vcs,
                   const gchar          *relpath,
                   GFile                *directory,
                   GCancellable         *cancellable)
{
  GFileEnumerator *enumerator;
  GPtrArray *children = NULL;
  gpointer file_info_ptr;

  g_assert (fuzzy != NULL);
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (ide_vcs_is_ignored (vcs, directory, NULL))
    return;

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE,
                                          cancellable,
                                          NULL);

  if (enumerator == NULL)
    return;

  while ((file_info_ptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) file_info = file_info_ptr;
      g_autofree gchar *path = NULL;
      g_autoptr(GFile) file = NULL;
      GFileType file_type;
      const gchar *name;

      if (g_file_info_get_is_symlink (file_info))
        continue;

      name = g_file_info_get_display_name (file_info);
      file = g_file_get_child (directory, name);

      file_type = g_file_info_get_file_type (file_info);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        {
          if (children == NULL)
            children = g_ptr_array_new_with_free_func (g_object_unref);
          g_ptr_array_add (children, g_object_ref (file));
          continue;
        }

      /* We only want to index regular files, and ignore symlinks.  If the
       * symlink points to something else in-tree, we'll index it in the
       * rightful place.
       */
      if (file_type != G_FILE_TYPE_REGULAR)
        continue;

      if (ide_vcs_is_ignored (vcs, file, NULL))
        continue;

      if (relpath != NULL)
        name = path = g_build_filename (relpath, name, NULL);

      dzl_fuzzy_mutable_index_insert (fuzzy, name, NULL);
    }

  g_clear_object (&enumerator);

  if (children != NULL)
    {
      gsize i;

      for (i = 0; i < children->len; i++)
        {
          g_autofree gchar *path = NULL;
          g_autofree gchar *name = NULL;
          GFile *child;

          child = g_ptr_array_index (children, i);
          name = g_file_get_basename (child);

          if (relpath != NULL)
            path = g_build_filename (relpath, name, NULL);

          populate_from_dir (fuzzy, vcs, path ? path : name, child, cancellable);
        }
    }

  g_clear_pointer (&children, g_ptr_array_unref);
}

static void
gb_file_search_index_builder (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
  GbFileSearchIndex *self = source_object;
  g_autoptr(GTimer) timer = NULL;
  GFile *directory = task_data;
  IdeContext *context;
  IdeVcs *vcs;
  DzlFuzzyMutableIndex *fuzzy;
  gdouble elapsed;

  g_assert (G_IS_TASK (task));
  g_assert (GB_IS_FILE_SEARCH_INDEX (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (G_IS_FILE (directory));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);

  timer = g_timer_new ();

  fuzzy = dzl_fuzzy_mutable_index_new (FALSE);
  dzl_fuzzy_mutable_index_begin_bulk_insert (fuzzy);
  populate_from_dir (fuzzy, vcs, NULL, directory, cancellable);
  dzl_fuzzy_mutable_index_end_bulk_insert (fuzzy);

  self->fuzzy = fuzzy;

  g_timer_stop (timer);
  elapsed = g_timer_elapsed (timer, NULL);

  g_message ("File index built in %lf seconds.", elapsed);

  g_task_return_boolean (task, TRUE);
}

void
gb_file_search_index_build_async (GbFileSearchIndex   *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (GB_IS_FILE_SEARCH_INDEX (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gb_file_search_index_build_async);
  g_task_set_priority (task, G_PRIORITY_LOW);

  if (self->root_directory == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "Root directory has not been set.");
      return;
    }

  g_task_set_task_data (task, g_object_ref (self->root_directory), g_object_unref);
  g_task_run_in_thread (task, gb_file_search_index_builder);
}

gboolean
gb_file_search_index_build_finish (GbFileSearchIndex  *self,
                                   GAsyncResult       *result,
                                   GError            **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (GB_IS_FILE_SEARCH_INDEX (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

GPtrArray *
gb_file_search_index_populate (GbFileSearchIndex *self,
                               const gchar       *query,
                               gsize              max_results)
{
  g_auto(IdeSearchReducer) reducer = { 0 };
  g_autoptr(GString) delimited = NULL;
  g_autoptr(GArray) ar = NULL;
  const gchar *iter = query;
  IdeContext *context;
  gsize i;

  g_return_val_if_fail (GB_IS_FILE_SEARCH_INDEX (self), NULL);
  g_return_val_if_fail (query != NULL, NULL);

  if (self->fuzzy == NULL)
    return g_ptr_array_new_with_free_func (g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (self));

  ide_search_reducer_init (&reducer, max_results);

  delimited = g_string_new (NULL);

  for (; *iter; iter = g_utf8_next_char (iter))
    {
      gunichar ch = g_utf8_get_char (iter);

      if (!g_unichar_isspace (ch))
        g_string_append_unichar (delimited, ch);
    }

  ar = dzl_fuzzy_mutable_index_match (self->fuzzy, delimited->str, max_results);

  for (i = 0; i < ar->len; i++)
    {
      const DzlFuzzyMutableIndexMatch *match = &g_array_index (ar, DzlFuzzyMutableIndexMatch, i);

      if (ide_search_reducer_accepts (&reducer, match->score))
        {
          g_autoptr(GbFileSearchResult) result = NULL;
          g_autofree gchar *escaped = NULL;
          g_autofree gchar *markup = NULL;
          g_autofree gchar *free_me = NULL;
          g_autofree gchar *free_me_sym = NULL;
          const gchar *filename = match->key;
          const gchar *icon_name = "text-x-generic-symbolic";
          g_autofree gchar *content_type;

          escaped = g_markup_escape_text (match->key, -1);
          markup = dzl_fuzzy_highlight (escaped, delimited->str, FALSE);

          /*
           * Try to get a more appropriate icon, but by filename only.
           * Sniffing would be way too slow here.
           */
          content_type = g_content_type_guess (filename, NULL, 0, NULL);
          if (content_type != NULL)
            icon_name = free_me = g_content_type_get_generic_icon_name (content_type);

          /* Cheat and simply append -symbolic to the name.  This isn't
           * strictly correct, but seems to work and doesn't require that we
           * create lots of GIcon instances and pass them around.
           */
          if (!g_str_has_suffix (icon_name, "-symbolic"))
            icon_name = free_me_sym = g_strdup_printf ("%s-symbolic", icon_name);

          result = g_object_new (GB_TYPE_FILE_SEARCH_RESULT,
                                 "context", context,
                                 "icon-name", icon_name,
                                 "score", match->score,
                                 "title", markup,
                                 "path", filename,
                                 NULL);
          ide_search_reducer_take (&reducer, g_steal_pointer (&result));
        }
    }

  return ide_search_reducer_free (&reducer, FALSE);
}

gboolean
gb_file_search_index_contains (GbFileSearchIndex *self,
                               const gchar       *relative_path)
{
  g_return_val_if_fail (GB_IS_FILE_SEARCH_INDEX (self), FALSE);
  g_return_val_if_fail (relative_path != NULL, FALSE);
  g_return_val_if_fail (self->fuzzy != NULL, FALSE);

  return dzl_fuzzy_mutable_index_contains (self->fuzzy, relative_path);
}

void
gb_file_search_index_insert (GbFileSearchIndex *self,
                             const gchar       *relative_path)
{
  g_return_if_fail (GB_IS_FILE_SEARCH_INDEX (self));
  g_return_if_fail (relative_path != NULL);
  g_return_if_fail (self->fuzzy != NULL);

  dzl_fuzzy_mutable_index_insert (self->fuzzy, relative_path, NULL);
}

void
gb_file_search_index_remove (GbFileSearchIndex *self,
                             const gchar       *relative_path)
{
  g_return_if_fail (GB_IS_FILE_SEARCH_INDEX (self));
  g_return_if_fail (relative_path != NULL);
  g_return_if_fail (self->fuzzy != NULL);

  dzl_fuzzy_mutable_index_remove (self->fuzzy, relative_path);
}
