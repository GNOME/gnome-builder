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

#include <egg-counter.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <ide.h>

#include "ide-ctags-builder.h"

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

static guint signals [LAST_SIGNAL];

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
      g_signal_emit (self, signals [TAGS_BUILT], 0, file);
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
  IdeSubprocess *process = (IdeSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (process));
  g_assert (G_IS_TASK (task));

  if (!ide_subprocess_wait_finish (process, result, &error))
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
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) process = NULL;
  g_autofree gchar *tags_file = NULL;
  g_autofree gchar *tags_filename = NULL;
  g_autofree gchar *workpath = NULL;
  g_autofree gchar *options_path = NULL;
  g_autofree gchar *tagsdir = NULL;
  IdeContext *context;
  IdeProject *project;
  GError *error = NULL;
  IdeVcs *vcs;

  IDE_ENTRY;

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
  tags_filename = g_strconcat (ide_project_get_id (project), ".tags", NULL);
  tags_file = g_build_filename (g_get_user_cache_dir (),
                                ide_get_program_name (),
                                "tags",
                                tags_filename,
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
      IDE_EXIT;
    }

  /* create the directory if necessary */
  tagsdir = g_path_get_dirname (tags_file);
  if (!g_file_test (tagsdir, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents (tagsdir, 0750);

  /* remove the existing tags file (we already have it in memory anyway) */
  if (g_file_test (tags_file, G_FILE_TEST_EXISTS))
    g_unlink (tags_file);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);

  ide_subprocess_launcher_push_argv (launcher, g_quark_to_string (self->ctags_path));
  ide_subprocess_launcher_push_argv (launcher, "-f");
  ide_subprocess_launcher_push_argv (launcher, "-");
  ide_subprocess_launcher_push_argv (launcher, "--recurse=yes");
  ide_subprocess_launcher_push_argv (launcher, "--tag-relative=no");
  ide_subprocess_launcher_push_argv (launcher, "--exclude=.git");
  ide_subprocess_launcher_push_argv (launcher, "--exclude=.bzr");
  ide_subprocess_launcher_push_argv (launcher, "--exclude=.svn");
  ide_subprocess_launcher_push_argv (launcher, "--sort=yes");
  ide_subprocess_launcher_push_argv (launcher, "--languages=all");
  ide_subprocess_launcher_push_argv (launcher, "--file-scope=yes");
  ide_subprocess_launcher_push_argv (launcher, "--c-kinds=+defgpstx");
  if (g_file_test (options_path, G_FILE_TEST_IS_REGULAR))
    {
      ide_subprocess_launcher_push_argv (launcher, "--options");
      ide_subprocess_launcher_push_argv (launcher, options_path);
    }
  ide_subprocess_launcher_push_argv (launcher, ".");

  /*
   * Create our arguments to launch the ctags generation process.
   */
  ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
  ide_subprocess_launcher_set_cwd (launcher, workpath);
  ide_subprocess_launcher_set_stdout_file_path (launcher, tags_file);
  /*
   * ctags can sometimes write to TMPDIR for incremental writes so that it
   * can sort internally. On large files this can cause us to run out of
   * tmpfs. Instead, just use the home dir which should map to something
   * that is persistent.
   */
  ide_subprocess_launcher_setenv (launcher, "TMPDIR", tagsdir, TRUE);
  process = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  EGG_COUNTER_INC (parse_count);

  if (process == NULL)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  g_task_set_task_data (task, g_file_new_for_path (tags_file), g_object_unref);

  ide_subprocess_wait_async (process,
                             cancellable,
                             ide_ctags_builder_process_wait_cb,
                             g_object_ref (task));

  IDE_EXIT;
}

void
ide_ctags_builder_rebuild (IdeCtagsBuilder *self)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CTAGS_BUILDER (self));

  /* Make sure we aren't already in shutdown. */
  if (!ide_object_hold (IDE_OBJECT (self)))
    return;

  task = g_task_new (self, NULL, ide_ctags_builder_build_cb, NULL);
  ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER, task, ide_ctags_builder_build_worker);
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
ide_ctags_builder_finalize (GObject *object)
{
  IdeCtagsBuilder *self = (IdeCtagsBuilder *)object;

  ide_clear_source (&self->build_timeout);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_ctags_builder_parent_class)->finalize (object);

  EGG_COUNTER_DEC (instances);
}

static void
ide_ctags_builder_class_init (IdeCtagsBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ctags_builder_finalize;

  signals [TAGS_BUILT] =
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
