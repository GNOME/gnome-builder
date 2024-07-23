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

#include <gtk/gtk.h>
#include <string.h>

#include <libide-code.h>
#include <libide-gui.h>

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

struct _GbpTodoModel
{
  GObject    parent_instance;
  GSequence *items;
  IdeVcs    *vcs;
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
  GSequence    *items;
  GFile        *file;
  GFile        *workdir;
  guint         single_file : 1;
} ResultInfo;

static GType
gbp_todo_model_get_item_type (GListModel *model)
{
  return GBP_TYPE_TODO_ITEM;
}

static guint
gbp_todo_model_get_n_items (GListModel *model)
{
  return g_sequence_get_length (GBP_TODO_MODEL (model)->items);
}

static gpointer
gbp_todo_model_get_item (GListModel *model,
                         guint       position)
{
  GbpTodoModel *self = GBP_TODO_MODEL (model);
  GSequenceIter *iter = g_sequence_get_iter_at_pos (self->items, position);

  if (g_sequence_iter_is_end (iter))
    return NULL;

  return g_object_ref (g_sequence_get (iter));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gbp_todo_model_get_item_type;
  iface->get_n_items = gbp_todo_model_get_n_items;
  iface->get_item = gbp_todo_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTodoModel, gbp_todo_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
  PROP_0,
  PROP_VCS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static GRegex *line1;
static GRegex *line2;

static const char *exclude_dirs[] = {
  ".bzr",
  ".flatpak-builder",
  "_build",
  ".git",
  ".svn",
  "node_modules",
};

/* This is an optimization to avoid reading files in from disk that
 * we know we'll discard, rather than wait until we query the IdeVcs
 * for that information.
 */
static const char *exclude_files[] = {
  "*~",
  "*.swp",
  "*.m4",
  "*.po",
  "*.min.js.*",
  "*.min.js",
  "configure",
  "Makecache",
};

static const char *keywords[] = {
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
  g_clear_object (&info->file);
  g_clear_object (&info->workdir);
  g_clear_pointer (&info->items, g_sequence_free);
  g_slice_free (ResultInfo, info);
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

  g_clear_pointer (&self->items, g_sequence_free);

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
  self->items = g_sequence_new (g_object_unref);
}

/**
 * gbp_todo_model_new:
 * @vcs: The Vcs to check for ignored files
 *
 * Creates a new #GbpTodoModel.
 *
 * Returns: (transfer full): A newly created #GbpTodoModel.
 */
GbpTodoModel *
gbp_todo_model_new (IdeVcs *vcs)
{
  return g_object_new (GBP_TYPE_TODO_MODEL,
                       "vcs", vcs,
                       NULL);
}

static int
gbp_todo_model_compare_func (GbpTodoItem *a,
                             GbpTodoItem *b)
{
  g_assert (GBP_IS_TODO_ITEM (a));
  g_assert (GBP_IS_TODO_ITEM (b));

  return g_strcmp0 (a->path, b->path);
}

static int
gbp_todo_model_compare_file (GbpTodoItem *a,
                             const char  *path)
{
  g_assert (GBP_IS_TODO_ITEM (a));
  g_assert (path != NULL);

  return g_strcmp0 (a->path, path);
}

static gboolean
result_info_merge (gpointer user_data)
{
  ResultInfo *r = user_data;
  guint added;

  g_assert (r != NULL);
  g_assert (GBP_IS_TODO_MODEL (r->self));
  g_assert (G_IS_FILE (r->file));
  g_assert (r->items != NULL);

  added = g_sequence_get_length (r->items);

  if (!r->single_file)
    {
      g_autoptr(GSequence) old_seq = g_steal_pointer (&r->self->items);
      guint old_len = g_sequence_get_length (old_seq);

      /* If we are not in single-file mode, then we just indexed the entire
       * project directory tree. Just swap out our items lists and notify
       * the list model interface of changes.
       */
      r->self->items = g_steal_pointer (&r->items);

      if (old_len || added)
        g_list_model_items_changed (G_LIST_MODEL (r->self), 0, old_len, added);

      return G_SOURCE_REMOVE;
    }
  else
    {
      g_autofree char *path = NULL;
      GSequenceIter *iter;
      GSequenceIter *prev;
      GSequenceIter *next;
      guint position;
      guint removed = 0;

      /* We parsed a single file for TODOs, so we need to remove all of the old
       * items first. We search for a GSequenceIter then walk backwards to the
       * first matching position (since multiple iters returning TRUE from the
       * compare func do not guarantee we're given the first). After the last
       * iter is removed, we can start inserting our sorted result set.
       */

      path = g_file_get_relative_path (r->workdir, r->file);
      iter = g_sequence_search (r->self->items,
                                path,
                                (GCompareDataFunc)gbp_todo_model_compare_file,
                                NULL);

      g_assert (iter != NULL);

      /* Walk backwards to first match. Necessary because GSequence does not
       * guarantee our binary search will find the first matching iter when
       * multiple iters may compare equally.
       */
      while ((prev = g_sequence_iter_prev (iter)) &&
             (prev != iter) &&
             gbp_todo_model_compare_file (g_sequence_get (prev), path) == 0)
        iter = prev;

      position = g_sequence_iter_get_position (iter);

      while ((next = g_sequence_iter_next (iter)) &&
             (next != iter) &&
             gbp_todo_model_compare_file (g_sequence_get (iter), path) == 0)
        {
          g_sequence_remove (iter);
          iter = next;
          removed++;
        }

      if (added > 0)
        g_sequence_move_range (iter,
                               g_sequence_get_begin_iter (r->items),
                               g_sequence_get_end_iter (r->items));

      g_list_model_items_changed (G_LIST_MODEL (r->self), position, removed, added);

      return G_SOURCE_REMOVE;
    }

  g_assert_not_reached ();
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
  g_autoptr(GSequence) items = NULL;
  g_autoptr(GbpTodoItem) item = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GTimer) timer = g_timer_new ();
  g_autofree char *workpath = NULL;
  GbpTodoModel *self = source_object;
  Mine *m = task_data;
  IdeLineReader reader;
  ResultInfo *info;
  gboolean single_file = FALSE;
  char *stdoutstr = NULL;
  char *line;
  gsize pathlen = 0;
  gsize stdoutstr_len;
  gsize len;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_TODO_MODEL (self));
  g_assert (m != NULL);
  g_assert (G_IS_FILE (m->file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  if (!g_file_is_native (m->workdir) ||
      !(workpath = g_file_get_path (m->workdir)))
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
  ide_subprocess_launcher_push_argv (launcher, "-w");

  if (!m->use_git_grep)
    ide_subprocess_launcher_push_argv (launcher, "-r");

  ide_subprocess_launcher_push_argv (launcher, "-E");

  if (!m->use_git_grep)
    {
      for (guint i = 0; i < G_N_ELEMENTS (exclude_files); i++)
        {
          const char *exclude_file = exclude_files[i];
          g_autofree char *arg = NULL;

          arg = g_strdup_printf ("--exclude=%s", exclude_file);
          ide_subprocess_launcher_push_argv (launcher, arg);
        }

      for (guint i = 0; i < G_N_ELEMENTS (exclude_dirs); i++)
        {
          const char *exclude_dir = exclude_dirs[i];
          g_autofree char *arg = NULL;

          arg = g_strdup_printf ("--exclude-dir=%s", exclude_dir);
          ide_subprocess_launcher_push_argv (launcher, arg);
        }
    }

  for (guint i = 0; i < G_N_ELEMENTS (keywords); i++)
    {
      const char *keyword = keywords[i];

      ide_subprocess_launcher_push_argv (launcher, "-e");
      ide_subprocess_launcher_push_argv (launcher, keyword);

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
      ide_subprocess_launcher_push_argv (launcher, g_file_peek_path (m->file));
      single_file = TRUE;
    }

  /* Spawn our grep process */
  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
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

  items = g_sequence_new (g_object_unref);

  /*
   * Process the STDOUT string iteratively, while trying to be tidy
   * with additional allocations. If we overwrite the trailing \n to
   * a \0, we can use it as a C string.
   */
  ide_line_reader_init (&reader, stdoutstr, stdoutstr_len);
  while ((line = ide_line_reader_next (&reader, &len)))
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
                  g_sequence_append (items, g_steal_pointer (&item));
                }
              else
                {
                  const char *pathstr = gbp_todo_item_get_path (item);

                  /*
                   * self->vcs is only set at construction, so safe to
                   * access via a worker thread. ide_vcs_path_is_ignored()
                   * is expected to be thread-safe as well.
                   */
                  if (!ide_vcs_path_is_ignored (self->vcs, pathstr, NULL))
                    g_sequence_append (items, g_steal_pointer (&item));
                  else
                    g_clear_object (&item);
                }
            }

          continue;
        }

      if (ide_str_empty0 (line) || len > 256)
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
                  const char *pathstr;

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
          g_sequence_append (items, g_steal_pointer (&item));
        }
      else
        {
          const char *pathstr = gbp_todo_item_get_path (item);

          /*
           * self->vcs is only set at construction, so safe to
           * access via a worker thread. ide_vcs_path_is_ignored()
           * is expected to be thread-safe as well.
           */
          if (!ide_vcs_path_is_ignored (self->vcs, pathstr, NULL))
            g_sequence_append (items, g_steal_pointer (&item));
          else
            g_clear_object (&item);
        }
    }

  g_debug ("Located %d TODO items in %0.4lf seconds",
           g_sequence_get_length (items),
           g_timer_elapsed (timer, NULL));

  info = g_slice_new0 (ResultInfo);
  info->self = g_object_ref (source_object);
  info->items = g_steal_pointer (&items);
  info->file = g_object_ref (m->file);
  info->single_file = single_file;
  info->workdir = g_object_ref (m->workdir);

  /* Sort our result set to help reduce how much sorting
   * needs to be done on the main thread later.
   */
  g_sequence_sort (info->items, (GCompareDataFunc)gbp_todo_model_compare_func, NULL);

  g_idle_add_full (G_PRIORITY_LOW + 100,
                   result_info_merge, info, result_info_free);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
is_typed (IdeVcs     *vcs,
          const char *name)
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
