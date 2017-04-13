/* ide-ctags-builder.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "ide-ctags-builder.h"

struct _IdeCtagsBuilder
{
  IdeObject  parent;
};

typedef struct
{
  GFile *directory;
  GFile *destination;
  gchar *ctags;
  guint  recursive : 1;
} BuildTaskData;

static void tags_builder_iface_init (IdeTagsBuilderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeCtagsBuilder, ide_ctags_builder, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TAGS_BUILDER, tags_builder_iface_init))

static GHashTable *ignored;

static void
build_task_data_free (gpointer data)
{
  BuildTaskData *task_data = data;

  g_clear_object (&task_data->directory);
  g_clear_object (&task_data->destination);
  g_clear_pointer (&task_data->ctags, g_free);

  g_slice_free (BuildTaskData, task_data);
}

static void
ide_ctags_builder_class_init (IdeCtagsBuilderClass *klass)
{
  ignored = g_hash_table_new (g_str_hash, g_str_equal);

  /* TODO: We need a really fast, *THREAD-SAFE* access to determine
   *       if files are ignored via the VCS.
   */

  g_hash_table_insert (ignored, ".git", NULL);
  g_hash_table_insert (ignored, ".bzr", NULL);
  g_hash_table_insert (ignored, ".svn", NULL);
  g_hash_table_insert (ignored, ".flatpak-builder", NULL);
  g_hash_table_insert (ignored, ".libs", NULL);
  g_hash_table_insert (ignored, ".deps", NULL);
  g_hash_table_insert (ignored, "autom4te.cache", NULL);
  g_hash_table_insert (ignored, "build-aux", NULL);
}

static void
ide_ctags_builder_init (IdeCtagsBuilder *self)
{
}

IdeTagsBuilder *
ide_ctags_builder_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_CTAGS_BUILDER,
                       "context", context,
                       NULL);
}

static gboolean
ide_ctags_builder_build (IdeCtagsBuilder *self,
                         const gchar     *ctags,
                         GFile           *directory,
                         GFile           *destination,
                         gboolean         recursive,
                         GCancellable    *cancellable)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GPtrArray) directories = NULL;
  g_autoptr(GPtrArray) dest_directories = NULL;
  g_autoptr(GFile) tags_file = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *cwd = NULL;
  g_autofree gchar *dest_dir = NULL;
  g_autofree gchar *options_path = NULL;
  g_autofree gchar *tags_path = NULL;
  g_autoptr(GString) filenames = NULL;
  GOutputStream *stdin_stream;
  gpointer infoptr;

  g_assert (IDE_IS_CTAGS_BUILDER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_FILE (destination));

  dest_dir = g_file_get_path (destination);
  if (0 != g_mkdir_with_parents (dest_dir, 0750))
    return FALSE;

  tags_file = g_file_get_child (destination, "tags");
  tags_path = g_file_get_path (tags_file);
  cwd = g_file_get_path (directory);
  options_path = g_build_filename (g_get_user_config_dir (),
                                   ide_get_program_name (),
                                   "ctags.conf",
                                   NULL);
  directories = g_ptr_array_new_with_free_func (g_object_unref);
  dest_directories = g_ptr_array_new_with_free_func (g_object_unref);
  filenames = g_string_new (NULL);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);

  ide_subprocess_launcher_set_cwd (launcher, cwd);
  ide_subprocess_launcher_setenv (launcher, "TMPDIR", cwd, TRUE);
  ide_subprocess_launcher_set_stdout_file_path (launcher, tags_path);

  ide_subprocess_launcher_push_argv (launcher, ctags);
  ide_subprocess_launcher_push_argv (launcher, "-f");
  ide_subprocess_launcher_push_argv (launcher, "-");
  ide_subprocess_launcher_push_argv (launcher, "--tag-relative=no");
  ide_subprocess_launcher_push_argv (launcher, "--exclude=.git");
  ide_subprocess_launcher_push_argv (launcher, "--exclude=.bzr");
  ide_subprocess_launcher_push_argv (launcher, "--exclude=.svn");
  ide_subprocess_launcher_push_argv (launcher, "--exclude=.flatpak-builder");
  ide_subprocess_launcher_push_argv (launcher, "--sort=yes");
  ide_subprocess_launcher_push_argv (launcher, "--languages=all");
  ide_subprocess_launcher_push_argv (launcher, "--file-scope=yes");
  ide_subprocess_launcher_push_argv (launcher, "--c-kinds=+defgpstx");

  if (g_file_test (options_path, G_FILE_TEST_IS_REGULAR))
    {
      ide_subprocess_launcher_push_argv (launcher, "--options");
      ide_subprocess_launcher_push_argv (launcher, options_path);
    }

  /* Read filenames from stdin, which we will provided below */
  ide_subprocess_launcher_push_argv (launcher, "-L");
  ide_subprocess_launcher_push_argv (launcher, "-");

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  stdin_stream = ide_subprocess_get_stdin_pipe (subprocess);

  /*
   * We do our own recursive building of ctags instead of --recursive=yes
   * so that we can have smaller files to update. This helps on larger
   * projects where we would have to rescan the whole project after a
   * file is saved.
   *
   * Additionally, while walking the file-system tree, we append files
   * to stdin of our ctags process to tell it to process them.
   */

  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          cancellable,
                                          &error);

  if (enumerator == NULL)
    IDE_GOTO (finish_subprocess);

  while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, cancellable, &error)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      const gchar *name;
      GFileType type;

      name = g_file_info_get_name (info);
      type = g_file_info_get_file_type (info);

      if (g_hash_table_contains (ignored, name))
        continue;

      if (type == G_FILE_TYPE_DIRECTORY)
        {
          if (recursive)
            {
              g_ptr_array_add (directories, g_file_get_child (directory, name));
              g_ptr_array_add (dest_directories, g_file_get_child (destination, name));
            }
        }
      else if (type == G_FILE_TYPE_REGULAR)
        {
          g_string_append_printf (filenames, "%s\n", name);
        }
    }

  g_output_stream_write_all (stdin_stream, filenames->str, filenames->len, NULL, NULL, NULL);

finish_subprocess:
  g_output_stream_close (stdin_stream, NULL, NULL);

  if (!ide_subprocess_wait_check (subprocess, NULL, &error))
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  for (guint i = 0; i < directories->len; i++)
    {
      GFile *child = g_ptr_array_index (directories, i);
      GFile *dest_child = g_ptr_array_index (dest_directories, i);

      g_assert (G_IS_FILE (child));
      g_assert (G_IS_FILE (dest_child));

      if (!ide_ctags_builder_build (self, ctags, child, dest_child, recursive, cancellable))
        return FALSE;
    }

  return TRUE;
}

static void
ide_ctags_builder_build_worker (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data_ptr,
                                GCancellable *cancellable)
{
  BuildTaskData *task_data = task_data_ptr;
  IdeCtagsBuilder *self = source_object;
  const gchar *ctags;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CTAGS_BUILDER (source_object));
  g_assert (G_IS_FILE (task_data->directory));

  ctags = task_data->ctags;
  if (!g_find_program_in_path (ctags))
    ctags = "ctags";

  ide_ctags_builder_build (self,
                           ctags,
                           task_data->directory,
                           task_data->destination,
                           task_data->recursive,
                           cancellable);

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_ctags_builder_build_async (IdeTagsBuilder      *builder,
                               GFile               *directory_or_file,
                               gboolean             recursive,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  IdeCtagsBuilder *self = (IdeCtagsBuilder *)builder;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autofree gchar *destination_path = NULL;
  g_autofree gchar *relative_path = NULL;
  BuildTaskData *task_data;
  IdeContext *context;
  const gchar *project_id;
  GFile *workdir;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_BUILDER (self));
  g_assert (G_IS_FILE (directory_or_file));

  settings = g_settings_new ("org.gnome.builder.code-insight");

  task_data = g_slice_new0 (BuildTaskData);
  task_data->ctags = g_settings_get_string (settings, "ctags-path");
  task_data->directory = g_object_ref (directory_or_file);
  task_data->recursive = recursive;
  task_data->ctags = g_strdup ("ctags");

  /*
   * The destination directory for the tags should match the hierarchy
   * of the projects source tree, but be based in something like
   * ~/.cache/gnome-builder/tags/$project_id/ so that they can be reused
   * even between configuration changes. Primarily, we want to avoid
   * putting things in the source tree.
   */
  context = ide_object_get_context (IDE_OBJECT (self));
  project_id = ide_project_get_id (ide_context_get_project (context));
  workdir = ide_vcs_get_working_directory (ide_context_get_vcs (context));
  relative_path = g_file_get_relative_path (workdir, directory_or_file);
  destination_path = g_build_filename (g_get_user_cache_dir (),
                                       ide_get_program_name (),
                                       "tags",
                                       project_id,
                                       relative_path,
                                       NULL);
  task_data->destination = g_file_new_for_path (destination_path);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_ctags_builder_build_async);
  g_task_set_task_data (task, task_data, build_task_data_free);
  ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER, task, ide_ctags_builder_build_worker);

  IDE_EXIT;
}

static gboolean
ide_ctags_builder_build_finish (IdeTagsBuilder  *builder,
                                GAsyncResult    *result,
                                GError         **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_CTAGS_BUILDER (builder), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
tags_builder_iface_init (IdeTagsBuilderInterface *iface)
{
  iface->build_async = ide_ctags_builder_build_async;
  iface->build_finish = ide_ctags_builder_build_finish;
}
