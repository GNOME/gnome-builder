/* ide-ctags-builder.c
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

#define G_LOG_DOMAIN "ide-ctags-builder"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "egg-counter.h"

#include "ide-buffer.h"
#include "ide-buffer-manager.h"
#include "ide-context.h"
#include "ide-ctags-builder.h"
#include "ide-debug.h"
#include "ide-global.h"
#include "ide-project.h"
#include "ide-thread-pool.h"
#include "ide-vcs.h"

#define BUILD_CTAGS_DELAY_SECONDS 10

EGG_DEFINE_COUNTER (instances, "IdeCtagsBuilder", "Instances", "Number of IdeCtagsBuilder instances.")
EGG_DEFINE_COUNTER (parse_count, "IdeCtagsBuilder", "Build Count", "Number of build attempts.");

struct _IdeCtagsBuilder
{
  IdeObject  parent_instance;

  GSettings *settings;

  GQuark     ctags_path;

  guint      build_timeout;

  guint      is_building : 1;
};

enum {
  TAGS_BUILT,
  LAST_SIGNAL
};

G_DEFINE_DYNAMIC_TYPE (IdeCtagsBuilder, ide_ctags_builder, IDE_TYPE_OBJECT)

static guint gSignals [LAST_SIGNAL];

IdeCtagsBuilder *
ide_ctags_builder_new (void)
{
  return g_object_new (IDE_TYPE_CTAGS_BUILDER, NULL);
}

static void
ide_ctags_builder_build_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeCtagsBuilder *self = (IdeCtagsBuilder *)object;
  GTask *task = (GTask *)result;
  GFile *file;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_BUILDER (self));
  g_assert (G_IS_TASK (task));

  if (g_task_propagate_boolean (task, &error))
    {
      file = g_task_get_task_data (task);
      g_assert (G_IS_FILE (file));
      g_signal_emit (self, gSignals [TAGS_BUILT], 0, file);
    }
  else
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  self->is_building = FALSE;

  IDE_EXIT;
}

static void
ide_ctags_builder_process_wait_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GSubprocess *process = (GSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_SUBPROCESS (process));
  g_assert (G_IS_TASK (task));

  if (!g_subprocess_wait_finish (process, result, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_ctags_builder_build_worker (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  IdeCtagsBuilder *self = source_object;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) process = NULL;
  g_autoptr(GPtrArray) argv = NULL;
  g_autofree gchar *tags_file = NULL;
  g_autofree gchar *workpath = NULL;
  g_autofree gchar *options_path = NULL;
  g_autofree gchar *tagsdir = NULL;
  IdeContext *context;
  IdeProject *project;
  GError *error = NULL;
  IdeVcs *vcs;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CTAGS_BUILDER (self));
  g_assert (task_data == NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * Get our necessary components, and then release the context hold
   * which we acquired before passing work to this thread.
   */
  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  vcs = ide_context_get_vcs (context);
  workdir = g_object_ref (ide_vcs_get_working_directory (vcs));
  tags_file = g_build_filename (g_get_user_cache_dir (),
                                ide_get_program_name (),
                                ide_project_get_id (project),
                                "tags",
                                NULL);
  options_path = g_build_filename (g_get_user_config_dir (),
                                   ide_get_program_name (),
                                   "ctags.conf",
                                   NULL);
  ide_object_release (IDE_OBJECT (self));

  /*
   * If the file is not native, ctags can't generate anything for us.
   */
  if (!(workpath = g_file_get_path (workdir)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "ctags can only operate on local files.");
      return;
    }

  /* create the directory if necessary */
  tagsdir = g_path_get_dirname (tags_file);
  if (!g_file_test (tagsdir, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents (tagsdir, 0750);

  /* remove the existing tags file (we already have it in memory anyway) */
  if (g_file_test (tags_file, G_FILE_TEST_EXISTS))
    g_unlink (tags_file);

  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_strdup (g_quark_to_string (self->ctags_path)));
  g_ptr_array_add (argv, g_strdup ("-f"));
  g_ptr_array_add (argv, g_strdup ("-"));
  g_ptr_array_add (argv, g_strdup ("--recurse=yes"));
  g_ptr_array_add (argv, g_strdup ("--tag-relative=no"));
  g_ptr_array_add (argv, g_strdup ("--exclude=.git"));
  g_ptr_array_add (argv, g_strdup ("--exclude=.bzr"));
  g_ptr_array_add (argv, g_strdup ("--exclude=.svn"));
  g_ptr_array_add (argv, g_strdup ("--sort=yes"));
  g_ptr_array_add (argv, g_strdup ("--languages=all"));
  g_ptr_array_add (argv, g_strdup ("--file-scope=yes"));
  g_ptr_array_add (argv, g_strdup ("--c-kinds=+defgpstx"));
  if (g_file_test (options_path, G_FILE_TEST_IS_REGULAR))
    g_ptr_array_add (argv, g_strdup_printf ("--options=%s", options_path));
  g_ptr_array_add (argv, g_strdup ("."));
  g_ptr_array_add (argv, NULL);

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *msg = g_strjoinv (" ", (gchar **)argv->pdata);
    IDE_TRACE_MSG ("%s", msg);
  }
#endif

  /*
   * Create our arguments to launch the ctags generation process.
   */
  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_set_cwd (launcher, workpath);
  g_subprocess_launcher_set_stdout_file_path (launcher, tags_file);
  process = g_subprocess_launcher_spawnv (launcher, (const gchar * const *)argv->pdata, &error);

  EGG_COUNTER_INC (parse_count);

  if (process == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_set_task_data (task, g_file_new_for_path (tags_file), g_object_unref);

  g_subprocess_wait_async (process,
                           cancellable,
                           ide_ctags_builder_process_wait_cb,
                           g_object_ref (task));
}

static void
ide_ctags_builder_do_build (IdeCtagsBuilder *self)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_CTAGS_BUILDER (self));

  /* Make sure we aren't already in shutdown. */
  if (!ide_object_hold (IDE_OBJECT (self)))
    return;

  task = g_task_new (self, NULL, ide_ctags_builder_build_cb, NULL);
  ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER, task, ide_ctags_builder_build_worker);
}

static gboolean
ide_ctags_builder_build_timeout (gpointer data)
{
  IdeCtagsBuilder *self = data;

  g_assert (IDE_IS_CTAGS_BUILDER (self));

  self->build_timeout = 0;

  if (self->is_building == FALSE)
    {
      self->is_building = TRUE;
      ide_ctags_builder_do_build (self);
    }

  return G_SOURCE_REMOVE;
}

static void
ide_ctags_builder__buffer_saved_cb (IdeCtagsBuilder  *self,
                                    IdeBuffer        *buffer,
                                    IdeBufferManager *buffer_manager)
{
  g_assert (IDE_IS_CTAGS_BUILDER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (self->build_timeout != 0)
    {
      g_source_remove (self->build_timeout);
      self->build_timeout = 0;
    }

  /*
   * TODO: We will need to make ctags code insight check a few keys,
   *       such as symbol resolving, autocompletion, highlight, etc.
   */
  if (!g_settings_get_boolean (self->settings, "ctags-autocompletion"))
    return;

  self->build_timeout = g_timeout_add_seconds (BUILD_CTAGS_DELAY_SECONDS,
                                               ide_ctags_builder_build_timeout,
                                               self);
}

static void
ide_ctags_builder__ctags_path_changed (IdeCtagsBuilder *self,
                                       const gchar     *key,
                                       GSettings       *settings)
{
  g_autofree gchar *ctags_path = NULL;

  g_assert (IDE_IS_CTAGS_BUILDER (self));
  g_assert (ide_str_equal0 (key, "ctags-path"));
  g_assert (G_IS_SETTINGS (settings));

  ctags_path = g_settings_get_string (settings, "ctags-path");
  self->ctags_path = g_quark_from_string (ctags_path);
}

static void
ide_ctags_builder_constructed (GObject *object)
{
  IdeCtagsBuilder *self = (IdeCtagsBuilder *)object;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer_manager = ide_context_get_buffer_manager (context);

  g_signal_connect_object (buffer_manager,
                           "buffer-saved",
                           G_CALLBACK (ide_ctags_builder__buffer_saved_cb),
                           self,
                           G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (ide_ctags_builder_parent_class)->constructed (object);
}

static void
ide_ctags_builder_finalize (GObject *object)
{
  IdeCtagsBuilder *self = (IdeCtagsBuilder *)object;

  if (self->build_timeout)
    {
      g_source_remove (self->build_timeout);
      self->build_timeout = 0;
    }

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_ctags_builder_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);
}

static void
ide_ctags_builder_class_init (IdeCtagsBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_ctags_builder_constructed;
  object_class->finalize = ide_ctags_builder_finalize;

  gSignals [TAGS_BUILT] =
    g_signal_new ("tags-built",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_FILE);
}

static void
ide_ctags_builder_class_finalize (IdeCtagsBuilderClass *klass)
{
}

static void
ide_ctags_builder_init (IdeCtagsBuilder *self)
{
  g_autofree gchar *ctags_path = NULL;

  EGG_COUNTER_INC (instances);

  self->settings = g_settings_new ("org.gnome.builder.code-insight");

  g_signal_connect_object (self->settings,
                           "changed::ctags-path",
                           G_CALLBACK (ide_ctags_builder__ctags_path_changed),
                           self,
                           G_CONNECT_SWAPPED);

  ctags_path = g_settings_get_string (self->settings, "ctags-path");
  self->ctags_path = g_quark_from_string (ctags_path);
}

void
_ide_ctags_builder_register_type (GTypeModule *module)
{
  ide_ctags_builder_register_type (module);
}
