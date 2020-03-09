/* gbp-todo-model.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-todo-model"

#include <libide-code.h>
#include <libide-gui.h>
#include <string.h>

#include "gbp-todo-model.h"
#include "gbp-todo-item.h"

/*
 * If you feel like optimizing this, I would go the route of creating a custom
 * GtkTreeModelIface. Unfortunately GtkTreeDataList always copies strings from
 * the caller (even when using gtk_list_store_set_value() w/ static string) so
 * that prevents us from being clever about using GStringChunk/etc.
 *
 * My preference would be a 2-level tree, with the first level being the index
 * of files, and the second level being the items, with string pointers into
 * a GStringChunk. Most things wont change often, so that space for strings,
 * even when deleted, is more than fine.
 */

struct _GbpTodoModel {
  GtkListStore  parent_instance;
  IdeVcs       *vcs;
};

typedef struct
{
  GFile *file;
  GFile *workdir;
  guint use_git_grep : 1;
} Mine;

typedef struct
{
  GbpTodoModel *self;
  GPtrArray    *items;
} ResultInfo;

G_DEFINE_TYPE (GbpTodoModel, gbp_todo_model, GTK_TYPE_LIST_STORE)

enum {
  PROP_0,
  PROP_VCS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static GRegex *line1;
static GRegex *line2;

static const gchar *exclude_dirs[] = {
  ".bzr",
  ".flatpak-builder",
  ".git",
  ".svn",
  "node_modules",
};

/* This is an optimization to avoid reading files in from disk that
 * we know we'll discard, rather than wait until we query the IdeVcs
 * for that information.
 */
static const gchar *exclude_files[] = {
  "*~",
  "*.swp",
  "*.m4",
  "*.po",
  "*.min.js.*",
  "*.min.js",
  "configure",
  "Makecache",
};

static const gchar *keywords[] = {
  "FIXME",
  "XXX",
  "TODO",
  "HACK",
};

static void
mine_free (Mine *m)
{
  g_clear_object (&m->file);
  g_clear_object (&m->workdir);
  g_slice_free (Mine, m);
}

static void
result_info_free (gpointer data)
{
  ResultInfo *info = data;

  g_clear_object (&info->self);
  g_clear_pointer (&info->items, g_ptr_array_unref);
  g_slice_free (ResultInfo, info);
}

static void
gbp_todo_model_clear (GbpTodoModel *self,
                      const gchar  *path)
{
  GtkTreeIter iter;
  gboolean matched = FALSE;

  g_assert (GBP_IS_TODO_MODEL (self));
  g_assert (path != NULL);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self), &iter))
    {
      do
        {
          g_autoptr(GbpTodoItem) item = NULL;
          const gchar *item_path;

        again:

          gtk_tree_model_get (GTK_TREE_MODEL (self), &iter, 0, &item, -1);
          item_path = gbp_todo_item_get_path (item);

          if (g_strcmp0 (path, item_path) == 0)
            {
              if (!gtk_list_store_remove (GTK_LIST_STORE (self), &iter))
                break;
              matched = TRUE;
              g_clear_object (&item);
              goto again;
            }
          else if (matched)
            {
              /* We skipped past our last match, so we might as well
               * short-circuit and avoid looking at more rows.
               */
              break;
            }
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self), &iter));
    }
}

static void
gbp_todo_model_merge (GbpTodoModel *self,
                      GbpTodoItem  *item)
{
  GtkTreeIter iter;

  g_assert (GBP_IS_TODO_MODEL (self));
  g_assert (GBP_IS_TODO_ITEM (item));

  gtk_list_store_prepend (GTK_LIST_STORE (self), &iter);
  gtk_list_store_set (GTK_LIST_STORE (self), &iter, 0, item, -1);
}

static gboolean
gbp_todo_model_merge_results (gpointer user_data)
{
  ResultInfo *info = user_data;
  const gchar *last_path = NULL;
  gboolean needs_clear = FALSE;

  g_assert (info != NULL);
  g_assert (GBP_IS_TODO_MODEL (info->self));
  g_assert (info->items != NULL);

  /* Try to avoid clearing on the initial build of the model */
  if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (info->self), NULL) > 0)
    needs_clear = TRUE;

  /* Walk backwards to preserve ordering, as merging will always prepend
   * the item to the store.
   */
  for (guint i = info->items->len; i > 0; i--)
    {
      GbpTodoItem *item = g_ptr_array_index (info->items, i - 1);
      const gchar *path = gbp_todo_item_get_path (item);

      if (needs_clear && (g_strcmp0 (path, last_path) != 0))
        gbp_todo_model_clear (info->self, path);

      gbp_todo_model_merge (info->self, item);

      last_path = path;
    }

  return G_SOURCE_REMOVE;
}

static void
gbp_todo_model_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpTodoModel *self = GBP_TODO_MODEL (object);

  switch (prop_id)
    {
    case PROP_VCS:
      g_value_set_object (value, self->vcs);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_todo_model_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpTodoModel *self = GBP_TODO_MODEL (object);

  switch (prop_id)
    {
    case PROP_VCS:
      self->vcs = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_todo_model_dispose (GObject *object)
{
  GbpTodoModel *self = (GbpTodoModel *)object;

  g_clear_object (&self->vcs);

  G_OBJECT_CLASS (gbp_todo_model_parent_class)->dispose (object);
}

static void
gbp_todo_model_class_init (GbpTodoModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_autoptr(GError) error = NULL;

  object_class->dispose = gbp_todo_model_dispose;
  object_class->get_property = gbp_todo_model_get_property;
  object_class->set_property = gbp_todo_model_set_property;

  properties [PROP_VCS] =
    g_param_spec_object ("vcs",
                         "Vcs",
                         "The VCS for the current context",
                         IDE_TYPE_VCS,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  line1 = g_regex_new ("([a-zA-Z0-9@\\+\\-\\.\\/_]+):(\\d+):(.*)", 0, 0, &error);
  g_assert_no_error (error);
  g_assert (line1 != NULL);

  line2 = g_regex_new ("([a-zA-Z0-9@\\+\\-\\.\\/_]+)-(\\d+)-(.*)", 0, 0, &error);
  g_assert_no_error (error);
  g_assert (line2 != NULL);
}

static void
gbp_todo_model_init (GbpTodoModel *self)
{
  GType column_types[] = { GBP_TYPE_TODO_ITEM };

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   G_N_ELEMENTS (column_types),
                                   column_types);
}

/**
 * gbp_todo_model_new:
 * @vcs: The Vcs to check for ignored files
 *
 * Creates a new #GbpTodoModel.
 *
 * Returns: (transfer full): A newly created #GbpTodoModel.
 *
 * Since: 3.32
 */
GbpTodoModel *
gbp_todo_model_new (IdeVcs *vcs)
{
  return g_object_new (GBP_TYPE_TODO_MODEL,
                       "vcs", vcs,
                       NULL);
}

static void
gbp_todo_model_mine_worker (IdeTask      *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) items = NULL;
  g_autoptr(GbpTodoItem) item = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GTimer) timer = g_timer_new ();
  g_autofree gchar *workpath = NULL;
  GbpTodoModel *self = source_object;
  Mine *m = task_data;
  IdeLineReader reader;
  ResultInfo *info;
  gchar *stdoutstr = NULL;
  gchar *line;
  gsize pathlen = 0;
  gsize stdoutstr_len;
  gsize len;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_TODO_MODEL (self));
  g_assert (m != NULL);
  g_assert (G_IS_FILE (m->file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  if (!(workpath = g_file_get_path (m->workdir)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Cannot run on non-native file-systems");
      return;
    }

  pathlen = strlen (workpath);
  ide_subprocess_launcher_set_cwd (launcher, workpath);

  if (m->use_git_grep)
    {
      ide_subprocess_launcher_push_argv (launcher, "git");
      ide_subprocess_launcher_push_argv (launcher, "grep");
    }
  else
    {
#ifdef __FreeBSD__
      ide_subprocess_launcher_push_argv (launcher, "bsdgrep");
#else
      ide_subprocess_launcher_push_argv (launcher, "grep");
#endif
    }

  ide_subprocess_launcher_push_argv (launcher, "-A");
  ide_subprocess_launcher_push_argv (launcher, "5");
  ide_subprocess_launcher_push_argv (launcher, "-I");
  ide_subprocess_launcher_push_argv (launcher, "-H");
  ide_subprocess_launcher_push_argv (launcher, "-n");

  if (!m->use_git_grep)
    ide_subprocess_launcher_push_argv (launcher, "-r");

  ide_subprocess_launcher_push_argv (launcher, "-E");

  if (!m->use_git_grep)
    {
      for (guint i = 0; i < G_N_ELEMENTS (exclude_files); i++)
        {
          const gchar *exclude_file = exclude_files[i];
          g_autofree gchar *arg = NULL;

          arg = g_strdup_printf ("--exclude=%s", exclude_file);
          ide_subprocess_launcher_push_argv (launcher, arg);
        }

      for (guint i = 0; i < G_N_ELEMENTS (exclude_dirs); i++)
        {
          const gchar *exclude_dir = exclude_dirs[i];
          g_autofree gchar *arg = NULL;

          arg = g_strdup_printf ("--exclude-dir=%s", exclude_dir);
          ide_subprocess_launcher_push_argv (launcher, arg);
        }
    }

  for (guint i = 0; i < G_N_ELEMENTS (keywords); i++)
    {
      const gchar *keyword = keywords[i];
      g_autofree gchar *arg = NULL;

      arg = g_strdup_printf ("%s(:| )", keyword);
      ide_subprocess_launcher_push_argv (launcher, "-e");
      ide_subprocess_launcher_push_argv (launcher, arg);

      if (m->use_git_grep)
        {
          /* Avoid pathological lines up front before reading them into
           * the UI process memory space.
           *
           * Note that we do this *after* our TODO: match because it causes
           * grep to have to look at every line up to it. So to do this in
           * reverse order is incredibly slow.
           */
          ide_subprocess_launcher_push_argv (launcher, "--and");
          ide_subprocess_launcher_push_argv (launcher, "-e");
          ide_subprocess_launcher_push_argv (launcher, "^.{0,256}$");
        }
    }

  if (g_file_query_file_type (m->file, 0, NULL) != G_FILE_TYPE_DIRECTORY)
    {
      g_autofree gchar *path = NULL;

      path = g_file_get_path (m->workdir);
      ide_subprocess_launcher_push_argv (launcher, path);
    }

  /* Spawn our grep process */
  if (NULL == (subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Read all of the output into a giant string */
  if (!ide_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdoutstr, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /*
   * To avoid lots of string allocations in the model, we instead
   * store GObjects which contain a reference to the whole buffer
   * (the GBytes) and raw pointers into that data. We'll create
   * the buffer up front, but still mutate the contents as we
   * walk through it.
   */
  stdoutstr_len = strlen (stdoutstr);
  bytes = g_bytes_new_take (stdoutstr, stdoutstr_len);

  items = g_ptr_array_new_with_free_func (g_object_unref);

  /*
   * Process the STDOUT string iteratively, while trying to be tidy
   * with additional allocations. If we overwrite the trailing \n to
   * a \0, we can use it as a C string.
   */
  ide_line_reader_init (&reader, stdoutstr, stdoutstr_len);
  while (NULL != (line = ide_line_reader_next (&reader, &len)))
    {
      g_autoptr(GMatchInfo) match_info1 = NULL;
      g_autoptr(GMatchInfo) match_info2 = NULL;

      line[len] = '\0';

      /* We just finished a group of lines, flush this item
       * to the list of items and get started processing the
       * next result.
       */
      if (g_str_has_prefix (line, "--"))
        {
          if (item != NULL)
            {
              if (m->use_git_grep)
                {
                  g_ptr_array_add (items, g_steal_pointer (&item));
                }
              else
                {
                  const gchar *pathstr = gbp_todo_item_get_path (item);

                  /*
                   * self->vcs is only set at construction, so safe to
                   * access via a worker thread. ide_vcs_path_is_ignored()
                   * is expected to be thread-safe as well.
                   */
                  if (!ide_vcs_path_is_ignored (self->vcs, pathstr, NULL))
                    g_ptr_array_add (items, g_steal_pointer (&item));
                  else
                    g_clear_object (&item);
                }
            }

          continue;
        }

      if (dzl_str_empty0 (line) || len > 256)
        {
          /* cancel anything if the line is too long so that we don't get into
           * pathological cases.
           */
          g_clear_object (&item);
          continue;
        }

      /* Try to match the first line */
      if (item == NULL)
        {
          if (g_regex_match_full (line1, line, len, 0, 0, &match_info1, NULL))
            {
              gint begin;
              gint end;

              item = gbp_todo_item_new (bytes);

              /* Get the path */
              if (g_match_info_fetch_pos (match_info1, 1, &begin, &end))
                {
                  const gchar *pathstr;

                  line[end] = '\0';
                  pathstr = &line[begin];

                  if (pathlen == 0 || strncmp (workpath, pathstr, pathlen) == 0)
                    {
                      pathstr += pathlen;

                      while (*pathstr == G_DIR_SEPARATOR)
                        pathstr++;
                    }

                  gbp_todo_item_set_path (item, pathstr);
                }

              /* Get the line number */
              if (g_match_info_fetch_pos (match_info1, 2, &begin, &end))
                {
                  gint64 lineno;

                  line[end] = '\0';
                  lineno = g_ascii_strtoll (&line[begin], NULL, 10);
                  gbp_todo_item_set_lineno (item, lineno);
                }

              /* Get the message */
              if (g_match_info_fetch_pos (match_info1, 3, &begin, &end))
                {
                  line[end] = '\0';
                  gbp_todo_item_add_line (item, &line[begin]);
                }
            }

          continue;
        }

      g_assert (item != NULL);

      if (g_regex_match_full (line2, line, len, 0, 0, &match_info2, NULL))
        {
          gint begin;
          gint end;

          /* Get the message */
          if (g_match_info_fetch_pos (match_info2, 3, &begin, &end))
            {
              line[end] = '\0';
              gbp_todo_item_add_line (item, &line[begin]);
            }
        }
    }

  /* We might have a trailing item w/o final -- */
  if (item != NULL)
    {
      if (m->use_git_grep)
        {
          g_ptr_array_add (items, g_steal_pointer (&item));
        }
      else
        {
          const gchar *pathstr = gbp_todo_item_get_path (item);

          /*
           * self->vcs is only set at construction, so safe to
           * access via a worker thread. ide_vcs_path_is_ignored()
           * is expected to be thread-safe as well.
           */
          if (!ide_vcs_path_is_ignored (self->vcs, pathstr, NULL))
            g_ptr_array_add (items, g_steal_pointer (&item));
          else
            g_clear_object (&item);
        }
    }

  g_debug ("Located %u TODO items in %0.4lf seconds",
           items->len, g_timer_elapsed (timer, NULL));

  info = g_slice_new0 (ResultInfo);
  info->self = g_object_ref (source_object);
  info->items = g_steal_pointer (&items);

  gdk_threads_add_idle_full (G_PRIORITY_LOW + 100,
                             gbp_todo_model_merge_results,
                             info, result_info_free);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
is_typed (IdeVcs      *vcs,
          const gchar *name)
{
  return g_strcmp0 (G_OBJECT_TYPE_NAME (vcs), name) == 0;
}

/**
 * gbp_todo_model_mine_async:
 * @self: a #GbpTodoModel
 * @file: a #GFile to mine
 * @cancellable: (nullable): a #Gancellable or %NULL
 * @callback: (scope async) (closure user_data): An async callback
 * @user_data: user data for @callback
 *
 * Asynchronously mines @file.
 *
 * If @file is a directory, it will be recursively scanned.  @callback
 * will be called after the operation is complete.  Call
 * gbp_todo_model_mine_finish() to get the result of this operation.
 *
 * If @file is not a native file (meaning it is accessable on the
 * normal, mounted, local file-system) this operation will fail.
 *
 * Since: 3.32
 */
void
gbp_todo_model_mine_async (GbpTodoModel        *self,
                           GFile               *file,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GFile *workdir;
  Mine *m;

  g_return_if_fail (GBP_IS_TODO_MODEL (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW + 100);
  ide_task_set_source_tag (task, gbp_todo_model_mine_async);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Only local files are supported");
      return;
    }

  workdir = ide_vcs_get_workdir (self->vcs);

  m = g_slice_new0 (Mine);
  m->file = g_object_ref (file);
  m->workdir = g_object_ref (workdir);
  m->use_git_grep = is_typed (self->vcs, "IdeGitVcs");
  ide_task_set_task_data (task, m, mine_free);

  ide_task_run_in_thread (task, gbp_todo_model_mine_worker);
}

/**
 * gbp_todo_model_mine_finish:
 * @self: a #GbpTodoModel
 * @result: a #GAsyncResult
 * @error: A location for a #GError or %NULL
 *
 * Completes an asynchronous request to gbp_todo_model_mine_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.32
 */
gboolean
gbp_todo_model_mine_finish (GbpTodoModel  *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (GBP_IS_TODO_MODEL (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
