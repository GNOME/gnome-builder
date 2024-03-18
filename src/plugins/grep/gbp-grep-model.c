/* gbp-grep-model.c
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

#define G_LOG_DOMAIN "gbp-grep-model"

#include "config.h"

#include <libide-code.h>
#include <libide-vcs.h>

#include "gbp-grep-model.h"

typedef struct
{
  GBytes    *bytes;
  GPtrArray *rows;
} Index;

struct _GbpGrepModel
{
  GObject parent_instance;

  IdeContext *context;

  /* The root directory to start searching from. */
  GFile *directory;

  /* The query text, which we use to send to grep as well as use with
   * GRegex to extract the match positions from a specific line.
   */
  gchar *query;

  /* We need to do client-side processing to extract the exact message
   * locations after grep gives us the matching lines. This allows us to
   * create IdeTextEdit source ranges later as well as creating the
   * match positions for highlighting in the treeview cell renderers.
   */
  GRegex *message_regex;

  /* Our index of matches, which can be compiled off the main thread
   * and then assigned to the model after it has completed building.
   */
  Index *index;

  /* We store the index of the toggled items here, and use that to
   * reverse their selection from a base "all" or "nothing" mode.
   */
  GHashTable *toggled;

  /* We cache the last line we parsed, because the view will parse
   * the same line repeatedly as it builds the cells for display.
   * This saves us a bunch of repeated work.
   */
  GbpGrepModelLine prev_line;

  guint mode;

  guint has_scanned : 1;
  guint use_regex : 1;
  guint recursive : 1;
  guint case_sensitive : 1;
  guint at_word_boundaries : 1;
  guint was_directory : 1;
};

static void tree_model_iface_init (GtkTreeModelIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGrepModel, gbp_grep_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, tree_model_iface_init))

enum {
  PROP_0,
  PROP_AT_WORD_BOUNDARIES,
  PROP_CASE_SENSITIVE,
  PROP_DIRECTORY,
  PROP_RECURSIVE,
  PROP_USE_REGEX,
  PROP_QUERY,
  N_PROPS
};

enum {
  MODE_NONE,
  MODE_ALL,
};

static GParamSpec *properties [N_PROPS];
static GRegex     *line_regex;

static void
index_free (gpointer data)
{
  Index *idx = data;

  g_clear_pointer (&idx->rows, g_ptr_array_unref);
  g_clear_pointer (&idx->bytes, g_bytes_unref);
  g_slice_free (Index, idx);
}

static void
clear_line (GbpGrepModelLine *cl)
{
  cl->start_of_line = NULL;
  cl->line = 0;
  g_clear_pointer (&cl->path, g_free);
  g_clear_pointer (&cl->matches, g_array_unref);
}

static gboolean
gbp_grep_model_line_parse (GbpGrepModelLine *cl,
                           const gchar      *line,
                           GRegex           *message_regex)
{
  g_autoptr(GMatchInfo) match = NULL;
  gsize line_len;

  g_assert (cl != NULL);
  g_assert (line != NULL);
  g_assert (line_regex != NULL);
  g_assert (message_regex != NULL);

  line_len = strlen (line);

  if (g_regex_match_full (line_regex, line, line_len, 0, 0, &match, NULL))
    {
      g_autofree gchar *pathstr = NULL;
      g_autofree gchar *linestr = NULL;
      gint msg_begin = -1;
      gint msg_end = -1;

      pathstr = g_match_info_fetch (match, 1);
      linestr = g_match_info_fetch (match, 2);

      if (g_match_info_fetch_pos (match, 3, &msg_begin, &msg_end))
        {
          g_autoptr(GMatchInfo) msg_match = NULL;
          gsize msg_len;

          /* Make sure we parsed the message offset */
          if (msg_begin < 0)
            return FALSE;

          cl->start_of_line = line;
          cl->start_of_message = line + msg_begin;
          cl->path = g_steal_pointer (&pathstr);
          cl->matches = g_array_new (FALSE, FALSE, sizeof (GbpGrepModelMatch));
          cl->line = g_ascii_strtoll (linestr, NULL, 10);

          /* Now parse the matches for the line so that we can highlight
           * them in the treeview and also determine the IdeTextEdit
           * source range when editing files.
           */

          msg_len = line_len - msg_begin;

          if (g_regex_match_full (message_regex, cl->start_of_message, msg_len, 0, 0, &msg_match, NULL))
            {
              do
                {
                  gint match_begin = -1;
                  gint match_end = -1;

                  if (g_match_info_fetch_pos (msg_match, 0, &match_begin, &match_end))
                    {
                      GbpGrepModelMatch cm;

                      /*
                       * We need to convert match offsets from bytes into the
                       * number of UTF-8 (unichar) characters) so that we get
                       * proper columns into the target file. Otherwise we risk
                       * corrupting non-ASCII files.
                       */
                      cm.match_begin = g_utf8_strlen (cl->start_of_message, match_begin);
                      cm.match_end = g_utf8_strlen (cl->start_of_message, match_end);
                      cm.match_begin_bytes = match_begin;
                      cm.match_end_bytes = match_end;

                      g_array_append_val (cl->matches, cm);
                    }
                }
              while (g_match_info_next (msg_match, NULL));

              g_clear_pointer (&msg_match, g_match_info_free);
            }
        }

      return TRUE;
    }

  return FALSE;
}

GbpGrepModel *
gbp_grep_model_new (IdeContext *context)
{
  GbpGrepModel *self;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  self = g_object_new (GBP_TYPE_GREP_MODEL, NULL);
  self->context = g_object_ref (context);

  return g_steal_pointer (&self);
}

static void
gbp_grep_model_dispose (GObject *object)
{
  GbpGrepModel *self = (GbpGrepModel *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (gbp_grep_model_parent_class)->dispose (object);
}

static void
gbp_grep_model_finalize (GObject *object)
{
  GbpGrepModel *self = (GbpGrepModel *)object;

  clear_line (&self->prev_line);

  g_clear_object (&self->context);
  g_clear_object (&self->directory);
  g_clear_pointer (&self->index, index_free);
  g_clear_pointer (&self->query, g_free);
  g_clear_pointer (&self->toggled, g_hash_table_unref);
  g_clear_pointer (&self->message_regex, g_regex_unref);

  G_OBJECT_CLASS (gbp_grep_model_parent_class)->finalize (object);
}

static void
gbp_grep_model_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpGrepModel *self = GBP_GREP_MODEL (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, gbp_grep_model_get_directory (self));
      break;

    case PROP_USE_REGEX:
      g_value_set_boolean (value, gbp_grep_model_get_use_regex (self));
      break;

    case PROP_RECURSIVE:
      g_value_set_boolean (value, gbp_grep_model_get_recursive (self));
      break;

    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, gbp_grep_model_get_case_sensitive (self));
      break;

    case PROP_AT_WORD_BOUNDARIES:
      g_value_set_boolean (value, gbp_grep_model_get_at_word_boundaries (self));
      break;

    case PROP_QUERY:
      g_value_set_string (value, gbp_grep_model_get_query (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_grep_model_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpGrepModel *self = GBP_GREP_MODEL (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      gbp_grep_model_set_directory (self, g_value_get_object (value));
      break;

    case PROP_USE_REGEX:
      gbp_grep_model_set_use_regex (self, g_value_get_boolean (value));
      break;

    case PROP_RECURSIVE:
      gbp_grep_model_set_recursive (self, g_value_get_boolean (value));
      break;

    case PROP_CASE_SENSITIVE:
      gbp_grep_model_set_case_sensitive (self, g_value_get_boolean (value));
      break;

    case PROP_AT_WORD_BOUNDARIES:
      gbp_grep_model_set_at_word_boundaries (self, g_value_get_boolean (value));
      break;

    case PROP_QUERY:
      gbp_grep_model_set_query (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_grep_model_class_init (GbpGrepModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_grep_model_dispose;
  object_class->finalize = gbp_grep_model_finalize;
  object_class->get_property = gbp_grep_model_get_property;
  object_class->set_property = gbp_grep_model_set_property;

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_REGEX] =
    g_param_spec_boolean ("use-regex", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RECURSIVE] =
    g_param_spec_boolean ("recursive", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CASE_SENSITIVE] =
    g_param_spec_boolean ("case-sensitive", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_AT_WORD_BOUNDARIES] =
    g_param_spec_boolean ("at-word-boundaries", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_QUERY] =
    g_param_spec_string ("query", NULL, NULL, NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  line_regex = g_regex_new ("([^:]+):(\\d+):(.*)", 0, 0, NULL);
  g_assert (line_regex != NULL);
}

static void
gbp_grep_model_init (GbpGrepModel *self)
{
  self->mode = MODE_ALL;
  self->toggled = g_hash_table_new (NULL, NULL);
}

static void
gbp_grep_model_clear_regex (GbpGrepModel *self)
{
  g_assert (GBP_IS_GREP_MODEL (self));

  g_clear_pointer (&self->message_regex, g_regex_unref);
}

static gboolean
gbp_grep_model_rebuild_regex (GbpGrepModel *self)
{
  GRegexCompileFlags compile_flags = G_REGEX_OPTIMIZE;
  g_autoptr(GRegex) regex = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *escaped = NULL;
  const gchar *query;

  g_assert (GBP_IS_GREP_MODEL (self));
  g_assert (self->message_regex == NULL);

  if (self->use_regex)
    query = self->query;
  else
    query = escaped = g_regex_escape_string (self->query, -1);

  if (!self->case_sensitive)
    compile_flags |= G_REGEX_CASELESS;

  if (!(regex = g_regex_new (query, compile_flags, 0, &error)))
    {
      g_warning ("Failed to compile regex for match: %s", error->message);
      return FALSE;
    }

  self->message_regex = g_steal_pointer (&regex);

  return TRUE;
}

const gchar *
gbp_grep_model_get_query (GbpGrepModel *self)
{
  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), NULL);

  return self->query;
}

void
gbp_grep_model_set_query (GbpGrepModel *self,
                          const gchar  *query)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));

  if (g_set_str (&self->query, query))
    {
      gbp_grep_model_clear_regex (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_QUERY]);
    }
}

/**
 * gbp_grep_model_get_directory:
 * @self: a #GbpGrepModel
 *
 * Returns: (transfer none) (nullable): A #GFile or %NULL
 */
GFile *
gbp_grep_model_get_directory (GbpGrepModel *self)
{
  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), NULL);

  return self->directory;
}

void
gbp_grep_model_set_directory (GbpGrepModel *self,
                              GFile        *directory)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));
  g_return_if_fail (self->has_scanned == FALSE);

  if (g_set_object (&self->directory, directory))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
}

gboolean
gbp_grep_model_get_use_regex (GbpGrepModel *self)
{
  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), FALSE);

  return self->use_regex;
}

void
gbp_grep_model_set_use_regex (GbpGrepModel *self,
                              gboolean      use_regex)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));
  g_return_if_fail (self->has_scanned == FALSE);

  use_regex = !!use_regex;

  if (use_regex != self->use_regex)
    {
      self->use_regex = use_regex;
      gbp_grep_model_clear_regex (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_REGEX]);
    }
}

gboolean
gbp_grep_model_get_recursive (GbpGrepModel *self)
{
  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), FALSE);

  return self->recursive;
}

void
gbp_grep_model_set_recursive (GbpGrepModel *self,
                              gboolean      recursive)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));
  g_return_if_fail (self->has_scanned == FALSE);

  recursive = !!recursive;

  if (recursive != self->recursive)
    {
      self->recursive = recursive;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RECURSIVE]);
    }
}

gboolean
gbp_grep_model_get_case_sensitive (GbpGrepModel *self)
{
  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), FALSE);

  return self->case_sensitive;
}

void
gbp_grep_model_set_case_sensitive (GbpGrepModel *self,
                                   gboolean      case_sensitive)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));
  g_return_if_fail (self->has_scanned == FALSE);

  case_sensitive = !!case_sensitive;

  if (case_sensitive != self->case_sensitive)
    {
      self->case_sensitive = case_sensitive;
      gbp_grep_model_clear_regex (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CASE_SENSITIVE]);
    }
}

gboolean
gbp_grep_model_get_at_word_boundaries (GbpGrepModel *self)
{
  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), FALSE);

  return self->at_word_boundaries;
}

void
gbp_grep_model_set_at_word_boundaries (GbpGrepModel *self,
                                       gboolean      at_word_boundaries)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));
  g_return_if_fail (self->has_scanned == FALSE);

  at_word_boundaries = !!at_word_boundaries;

  if (at_word_boundaries != self->at_word_boundaries)
    {
      self->at_word_boundaries = at_word_boundaries;
      gbp_grep_model_clear_regex (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AT_WORD_BOUNDARIES]);
    }
}

static IdeSubprocessLauncher *
gbp_grep_model_create_launcher (GbpGrepModel *self)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  const gchar *path;
  IdeVcs *vcs;
  GFile *workdir;
  GType git_vcs;
  gboolean use_git_grep = FALSE;

  g_assert (GBP_IS_GREP_MODEL (self));
  g_assert (self->query != NULL);
  g_assert (self->query[0] != '\0');

  vcs = ide_vcs_from_context (self->context);
  workdir = ide_vcs_get_workdir (vcs);
  git_vcs = g_type_from_name ("GbpGitVcs");

  if (self->directory != NULL)
    path = g_file_peek_path (self->directory);
  else
    path = g_file_peek_path (workdir);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  /*
   * Soft runtime check for Git support, so that we can use "git grep"
   * instead of the system "grep".
   */
  if (git_vcs != G_TYPE_INVALID && G_TYPE_CHECK_INSTANCE_TYPE (vcs, git_vcs))
    use_git_grep = TRUE;

  if (use_git_grep)
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

  ide_subprocess_launcher_push_argv (launcher, "-I");
  ide_subprocess_launcher_push_argv (launcher, "-H");
  ide_subprocess_launcher_push_argv (launcher, "-n");

  if (!self->case_sensitive)
    ide_subprocess_launcher_push_argv (launcher, "-i");

  if (self->at_word_boundaries)
    ide_subprocess_launcher_push_argv (launcher, "-w");

  if (!use_git_grep)
    {
      if (self->recursive)
        ide_subprocess_launcher_push_argv (launcher, "-r");
    }
  else
    {
      if (!self->recursive)
        ide_subprocess_launcher_push_argv (launcher, "--max-depth=0");

      /* Allow files that are untracked, but not ignored */
      ide_subprocess_launcher_push_argv (launcher, "--untracked");
      /* untracked argument is incompatible with potential default recurse-submodules */
      ide_subprocess_launcher_push_argv (launcher, "--no-recurse-submodules");
    }

  ide_subprocess_launcher_push_argv (launcher, "-E");

  if (!self->use_regex)
    {
      g_autofree gchar *escaped = NULL;

      escaped = g_regex_escape_string (self->query, -1);
      ide_subprocess_launcher_push_argv (launcher, "-e");
      ide_subprocess_launcher_push_argv (launcher, escaped);
    }
  else
    {
      ide_subprocess_launcher_push_argv (launcher, "-e");
      ide_subprocess_launcher_push_argv (launcher, self->query);
    }

  if (use_git_grep)
    {
      /* Avoid pathological lines up front before reading them into
       * the UI process memory space.
       *
       * Note that we do this *after* our query match because it causes
       * grep to have to look at every line up to it. So to do this in
       * reverse order is incredibly slow.
       */
      ide_subprocess_launcher_push_argv (launcher, "--and");
      ide_subprocess_launcher_push_argv (launcher, "-e");
      ide_subprocess_launcher_push_argv (launcher, "^.{0,1024}$");
    }

  if (g_file_test (path, G_FILE_TEST_IS_DIR))
    {
      ide_subprocess_launcher_set_cwd (launcher, path);
      self->was_directory = TRUE;
    }
  else
    {
      g_autofree gchar *parent = g_path_get_dirname (path);
      g_autofree gchar *name = g_path_get_basename (path);

      self->was_directory = FALSE;

      ide_subprocess_launcher_set_cwd (launcher, parent);
      ide_subprocess_launcher_push_argv (launcher, name);
    }

  return g_steal_pointer (&launcher);
}

static void
gbp_grep_model_build_index (IdeTask      *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  GBytes *bytes = task_data;
  IdeLineReader reader;
  Index *idx;
  gchar *buf;
  gsize len;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GREP_MODEL (source_object));
  g_assert (bytes != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  buf = (gchar *)g_bytes_get_data (bytes, &len);
  ide_line_reader_init (&reader, buf, len);

  idx = g_slice_new0 (Index);
  idx->bytes = g_bytes_ref (bytes);
  idx->rows = g_ptr_array_new ();

  while ((buf = ide_line_reader_next (&reader, &len)))
    {
      g_ptr_array_add (idx->rows, buf);
      buf[len] = 0;
    }

  ide_task_return_pointer (task, idx, index_free);
}

static void
gbp_grep_model_scan_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;
  gsize len;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (stdout_buf == NULL)
    stdout_buf = g_strdup ("");

  len = strlen (stdout_buf);
  bytes = g_bytes_new_take (g_steal_pointer (&stdout_buf), len);
  ide_task_set_task_data (task, g_steal_pointer (&bytes), g_bytes_unref);
  ide_task_run_in_thread (task, gbp_grep_model_build_index);

  IDE_EXIT;
}

void
gbp_grep_model_scan_async (GbpGrepModel        *self,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_GREP_MODEL (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_grep_model_scan_async);

  if (self->has_scanned)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "gbp_grep_model_scan_async() may only be called once");
      IDE_EXIT;
    }

  if (ide_str_empty0 (self->query))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "No query has been set to scan for");
      IDE_EXIT;
    }

  self->has_scanned = TRUE;

  launcher = gbp_grep_model_create_launcher (self);
  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         gbp_grep_model_scan_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
gbp_grep_model_scan_finish (GbpGrepModel  *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);
  g_return_val_if_fail (self->index == NULL, FALSE);

  self->index = ide_task_propagate_pointer (IDE_TASK (result), error);

  /*
   * Normally, we might emit ::row-inserted() for each row. But that
   * is expensive and our normal use case is that we don't attach the
   * model until the search has completed.
   */

  return self->index != NULL;
}

void
gbp_grep_model_select_all (GbpGrepModel *self)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));

  self->mode = MODE_ALL;
  g_hash_table_remove_all (self->toggled);
}

void
gbp_grep_model_select_none (GbpGrepModel *self)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));

  self->mode = MODE_NONE;
  g_hash_table_remove_all (self->toggled);
}

void
gbp_grep_model_toggle_row (GbpGrepModel *self,
                           GtkTreeIter  *iter)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));
  g_return_if_fail (iter != NULL);

  if (g_hash_table_contains (self->toggled, iter->user_data))
    g_hash_table_remove (self->toggled, iter->user_data);
  else
    g_hash_table_add (self->toggled, iter->user_data);
}

void
gbp_grep_model_toggle_mode (GbpGrepModel *self)
{
  g_return_if_fail (GBP_IS_GREP_MODEL (self));

  if (self->mode == MODE_ALL)
    gbp_grep_model_select_none (self);
  else
    gbp_grep_model_select_all (self);
}

static GtkTreeModelFlags
gbp_grep_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gbp_grep_model_get_n_columns (GtkTreeModel *tree_model)
{
  return 2;
}

static GType
gbp_grep_model_get_column_type (GtkTreeModel *tree_model,
                                gint          index_)
{
  switch (index_) {
  case 0:  return G_TYPE_STRING;
  case 1:  return G_TYPE_BOOLEAN;
  default: return G_TYPE_INVALID;
  }
}

static gboolean
gbp_grep_model_get_iter (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter,
                         GtkTreePath  *path)
{
  GbpGrepModel *self = (GbpGrepModel *)tree_model;
  gint *indicies;
  gint depth;

  g_assert (GBP_IS_GREP_MODEL (self));
  g_assert (iter != NULL);
  g_assert (path != NULL);

  if (self->index == NULL || self->index->rows == NULL)
    return FALSE;

  indicies = gtk_tree_path_get_indices_with_depth (path, &depth);

  if (depth != 1)
    return FALSE;

  iter->user_data = GINT_TO_POINTER (indicies[0]);

  return indicies[0] >= 0 && indicies[0] < self->index->rows->len;
}

static GtkTreePath *
gbp_grep_model_get_path (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter)
{
  g_assert (GBP_IS_GREP_MODEL (tree_model));
  g_assert (iter != NULL);

  return gtk_tree_path_new_from_indices (GPOINTER_TO_INT (iter->user_data), -1);
}

static void
gbp_grep_model_get_value (GtkTreeModel *tree_model,
                          GtkTreeIter  *iter,
                          gint          column,
                          GValue       *value)
{
  GbpGrepModel *self = (GbpGrepModel *)tree_model;
  guint index_;

  g_assert (GBP_IS_GREP_MODEL (self));
  g_assert (iter != NULL);
  g_assert (column == 0 || column == 1);
  g_assert (value != NULL);

  index_ = GPOINTER_TO_UINT (iter->user_data);

  if (column == 0)
    {
      g_value_init (value, G_TYPE_STRING);
      /* This is a hack that we can do because we know that our
       * consumer will only use the string for a short time to
       * parse it into the real value without holding the GValue
       * past the lifetime of the model.
       *
       * It saves us a serious amount of string copies.
       */
      g_value_set_static_string (value,
                                 g_ptr_array_index (self->index->rows, index_));
    }
  else if (column == 1)
    {
      gboolean b = self->mode;
      if (g_hash_table_contains (self->toggled, iter->user_data))
        b = !b;
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, b);
    }
}

static gboolean
gbp_grep_model_iter_next (GtkTreeModel *tree_model,
                          GtkTreeIter  *iter)
{
  GbpGrepModel *self = (GbpGrepModel *)tree_model;
  guint index_;

  g_assert (GBP_IS_GREP_MODEL (self));

  if (self->index == NULL)
    return FALSE;

  index_ = GPOINTER_TO_UINT (iter->user_data);
  if (index_ == G_MAXUINT)
    return FALSE;

  index_++;

  iter->user_data = GUINT_TO_POINTER (index_);

  return index_ < self->index->rows->len;
}

static gboolean
gbp_grep_model_iter_children (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter,
                              GtkTreeIter  *parent)
{
  GbpGrepModel *self = (GbpGrepModel *)tree_model;

  g_assert (GBP_IS_GREP_MODEL (self));
  g_assert (iter != NULL);

  iter->user_data = NULL;

  return parent == NULL &&
         self->index != NULL &&
         self->index->rows->len > 0;
}

static gboolean
gbp_grep_model_iter_has_child (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter)
{
  GbpGrepModel *self = (GbpGrepModel *)tree_model;

  g_assert (GBP_IS_GREP_MODEL (self));

  if (iter == NULL)
    return self->index != NULL && self->index->rows->len > 0;

  return FALSE;
}

static gint
gbp_grep_model_iter_n_children (GtkTreeModel *tree_model,
                                GtkTreeIter  *iter)
{
  GbpGrepModel *self = (GbpGrepModel *)tree_model;

  g_assert (GBP_IS_GREP_MODEL (self));

  if (iter == NULL)
    {
      if (self->index != NULL)
        return self->index->rows->len;
    }

  return 0;
}

static gboolean
gbp_grep_model_iter_nth_child (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *parent,
                               gint          n)
{
  GbpGrepModel *self = (GbpGrepModel *)tree_model;

  g_assert (GBP_IS_GREP_MODEL (self));
  g_assert (iter != NULL);

  if (parent == NULL && self->index != NULL)
    {
      iter->user_data = GUINT_TO_POINTER (n);
      return n < self->index->rows->len;
    }

  return FALSE;
}

static gboolean
gbp_grep_model_iter_parent (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter,
                            GtkTreeIter  *parent)
{
  return FALSE;
}

static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gbp_grep_model_get_flags;
  iface->get_n_columns = gbp_grep_model_get_n_columns;
  iface->get_column_type = gbp_grep_model_get_column_type;
  iface->get_iter = gbp_grep_model_get_iter;
  iface->get_path = gbp_grep_model_get_path;
  iface->get_value = gbp_grep_model_get_value;
  iface->iter_next = gbp_grep_model_iter_next;
  iface->iter_children = gbp_grep_model_iter_children;
  iface->iter_has_child = gbp_grep_model_iter_has_child;
  iface->iter_n_children = gbp_grep_model_iter_n_children;
  iface->iter_nth_child = gbp_grep_model_iter_nth_child;
  iface->iter_parent = gbp_grep_model_iter_parent;
}

static void
gbp_grep_model_foreach_selected (GbpGrepModel *self,
                                 void        (*callback) (GbpGrepModel *self, guint index_, gpointer user_data),
                                 gpointer      user_data)
{
  g_assert (GBP_IS_GREP_MODEL (self));
  g_assert (callback != NULL);

  if (self->index == NULL)
    return;

  if (self->mode == MODE_NONE)
    {
      GHashTableIter iter;
      gpointer key;

      g_hash_table_iter_init (&iter, self->toggled);
      while (g_hash_table_iter_next (&iter, &key, NULL))
        callback (self, GPOINTER_TO_UINT (key), user_data);
    }
  else if (self->mode == MODE_ALL)
    {
      for (guint i = 0; i < self->index->rows->len; i++)
        {
          if (!g_hash_table_contains (self->toggled, GINT_TO_POINTER (i)))
            callback (self, i, user_data);
        }
    }
  else
    g_assert_not_reached ();
}

static void
create_edits_cb (GbpGrepModel *self,
                 guint         index_,
                 gpointer      user_data)
{
  GPtrArray *edits = user_data;
  GbpGrepModelLine line = {0};
  const gchar *row;

  g_assert (GBP_IS_GREP_MODEL (self));
  g_assert (self->message_regex != NULL);
  g_assert (edits != NULL);

  row = g_ptr_array_index (self->index->rows, index_);

  if (gbp_grep_model_line_parse (&line, row, self->message_regex))
    {
      g_autoptr(GFile) file = NULL;
      guint lineno;

      file = gbp_grep_model_get_file (self, line.path);
      g_assert (G_IS_FILE (file));

      lineno = line.line ? line.line - 1 : 0;

      for (guint i = 0; i < line.matches->len; i++)
        {
          const GbpGrepModelMatch *match = &g_array_index (line.matches, GbpGrepModelMatch, i);
          g_autoptr(IdeTextEdit) edit = NULL;
          g_autoptr(IdeRange) range = NULL;
          g_autoptr(IdeLocation) begin = NULL;
          g_autoptr(IdeLocation) end = NULL;

          begin = ide_location_new (file, lineno, match->match_begin);
          end = ide_location_new (file, lineno, match->match_end);
          range = ide_range_new (begin, end);

          edit = ide_text_edit_new (range, NULL);

          g_ptr_array_add (edits, g_steal_pointer (&edit));
        }
    }

  clear_line (&line);
}

/**
 * gbp_grep_model_create_edits:
 * @self: a #GbpGrepModel
 *
 * Returns: (transfer container): a #GPtrArray of IdeTextEdit
 */
GPtrArray *
gbp_grep_model_create_edits (GbpGrepModel *self)
{
  g_autoptr(GPtrArray) edits = NULL;

  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), NULL);

  if (self->message_regex == NULL)
    {
      if (!gbp_grep_model_rebuild_regex (self))
        return NULL;
    }

  edits = g_ptr_array_new_with_free_func (g_object_unref);
  gbp_grep_model_foreach_selected (self, create_edits_cb, edits);

  return g_steal_pointer (&edits);
}

/**
 * gbp_grep_model_get_line:
 * @self: a #GbpGrepModel
 * @iter: a #GtkTextIter
 * @line: (out): a location for the line info
 *
 * Gets information about the line that @iter points at.
 */
void
gbp_grep_model_get_line (GbpGrepModel            *self,
                         GtkTreeIter             *iter,
                         const GbpGrepModelLine **line)
{
  const gchar *str;
  guint index_;

  g_return_if_fail (GBP_IS_GREP_MODEL (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (line != NULL);
  g_return_if_fail (self->index != NULL);
  g_return_if_fail (self->index->rows != NULL);

  *line = NULL;

  index_ = GPOINTER_TO_UINT (iter->user_data);
  g_return_if_fail (index_ < self->index->rows->len);

  str = g_ptr_array_index (self->index->rows, index_);

  if (str != self->prev_line.start_of_line)
    {
      clear_line (&self->prev_line);

      if (self->message_regex == NULL)
        {
          if (!gbp_grep_model_rebuild_regex (self))
            return;
        }

      gbp_grep_model_line_parse (&self->prev_line, str, self->message_regex);
    }

  *line = &self->prev_line;
}

/**
 * gbp_grep_model_get_file:
 *
 * Returns: (transfer full): a #GFile
 */
GFile *
gbp_grep_model_get_file (GbpGrepModel *self,
                         const gchar  *path)
{
  g_autoptr(GFile) directory = NULL;

  g_return_val_if_fail (GBP_IS_GREP_MODEL (self), NULL);

  directory = self->directory ? g_object_ref (self->directory) : ide_context_ref_workdir (self->context);

  if (!path || !*path || g_strcmp0 (path, ".") == 0)
    return g_file_dup (directory);

  if (self->was_directory)
    return g_file_get_child (directory, path);
  else
    return g_file_dup (directory);
}
