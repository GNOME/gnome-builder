/* gb-file-search-index.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <fuzzy.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "gb-file-search-index.h"
#include "gb-file-search-result.h"

struct _GbFileSearchIndex
{
  IdeObject     parent_instance;

  GFile        *root_directory;
  GFileMonitor *file_monitor;
  Fuzzy        *fuzzy;
};

G_DEFINE_TYPE (GbFileSearchIndex, gb_file_search_index, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ROOT_DIRECTORY,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_file_search_index_set_root_directory (GbFileSearchIndex *self,
                                         GFile             *root_directory)
{
  g_return_if_fail (GB_IS_FILE_SEARCH_INDEX (self));
  g_return_if_fail (!root_directory || G_IS_FILE (root_directory));

  if (g_set_object (&self->root_directory, root_directory))
    {
      g_clear_pointer (&self->fuzzy, fuzzy_unref);

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ROOT_DIRECTORY]);
    }
}

static void
gb_file_search_index_finalize (GObject *object)
{
  GbFileSearchIndex *self = (GbFileSearchIndex *)object;

  g_clear_object (&self->root_directory);
  g_clear_object (&self->file_monitor);
  g_clear_pointer (&self->fuzzy, fuzzy_unref);

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

  gParamSpecs [PROP_ROOT_DIRECTORY] =
    g_param_spec_object ("root-directory",
                         _("Root Directory"),
                         _("Root Directory"),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_file_search_index_init (GbFileSearchIndex *self)
{
}

static void
populate_from_dir (Fuzzy        *fuzzy,
                   IdeVcs       *vcs,
                   const gchar  *relpath,
                   GFile        *directory,
                   GCancellable *cancellable)
{
  GFileEnumerator *enumerator;
  GPtrArray *children = NULL;
  gpointer file_info_ptr;

  g_assert (fuzzy != NULL);
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  enumerator = g_file_enumerate_children (directory,
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
      const gchar *name;

      name = g_file_info_get_display_name (file_info);
      file = g_file_get_child (directory, name);

      if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
        {
          if (children == NULL)
            children = g_ptr_array_new_with_free_func (g_object_unref);
          g_ptr_array_add (children, g_object_ref (file));
          continue;
        }

      if (ide_vcs_is_ignored (vcs, file, NULL))
        continue;

      if (relpath != NULL)
        name = path = g_build_filename (relpath, name, NULL);

      fuzzy_insert (fuzzy, name, NULL);
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
  GFile *directory = task_data;
  IdeContext *context;
  IdeVcs *vcs;
  Fuzzy *fuzzy;
  GTimer *timer;
  gdouble elapsed;

  g_assert (G_IS_TASK (task));
  g_assert (GB_IS_FILE_SEARCH_INDEX (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (G_IS_FILE (directory));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);

  timer = g_timer_new ();

  fuzzy = fuzzy_new (FALSE);
  fuzzy_begin_bulk_insert (fuzzy);
  populate_from_dir (fuzzy, vcs, NULL, directory, cancellable);
  fuzzy_end_bulk_insert (fuzzy);

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

static gchar *
str_highlight (const gchar *str,
               const gchar *match)
{
  GString *ret;
  gunichar str_ch;
  gunichar match_ch;

  ret = g_string_new (NULL);

  for (; *str; str = g_utf8_next_char (str))
    {
      str_ch = g_utf8_get_char (str);
      match_ch = g_utf8_get_char (match);

      if (str_ch == match_ch)
        {
          g_string_append (ret, "<u>");
          g_string_append_unichar (ret, str_ch);
          g_string_append (ret, "</u>");

          match = g_utf8_next_char (match);
        }
      else
        {
          g_string_append_unichar (ret, str_ch);
        }
    }

  return g_string_free (ret, FALSE);
}

void
gb_file_search_index_populate (GbFileSearchIndex *self,
                               IdeSearchContext  *context,
                               IdeSearchProvider *provider,
                               const gchar       *query)
{
  g_autoptr(GArray) ar = NULL;
  g_auto(IdeSearchReducer) reducer = { 0 };
  IdeContext *icontext;
  gsize max_matches;
  gsize i;

  g_return_if_fail (GB_IS_FILE_SEARCH_INDEX (self));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (query != NULL);

  if (self->fuzzy == NULL)
    return;

  icontext = ide_object_get_context (IDE_OBJECT (provider));
  max_matches = ide_search_context_get_max_results (context);
  ide_search_reducer_init (&reducer, context, provider, max_matches);

  ar = fuzzy_match (self->fuzzy, query, max_matches);

  for (i = 0; i < ar->len; i++)
    {
      FuzzyMatch *match;

      match = &g_array_index (ar, FuzzyMatch, i);

      if (ide_search_reducer_accepts (&reducer, match->score))
        {
          g_autoptr(GbFileSearchResult) result = NULL;
          g_autofree gchar *markup = NULL;

          markup = str_highlight (match->key, query);
          result = g_object_new (GB_TYPE_FILE_SEARCH_RESULT,
                                 "context", icontext,
                                 "provider", provider,
                                 "score", match->score,
                                 "title", markup,
                                 "path", match->key,
                                 NULL);
          ide_search_reducer_push (&reducer, IDE_SEARCH_RESULT (result));
        }
    }
}
