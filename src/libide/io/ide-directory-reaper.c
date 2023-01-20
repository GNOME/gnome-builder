/* ide-directory-reaper.c
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

#define G_LOG_DOMAIN "ide-directory-reaper"

#include "config.h"

#include "ide-marshal.h"

#include "ide-directory-reaper.h"

typedef enum
{
  PATTERN_FILE,
  PATTERN_GLOB,
} PatternType;

typedef struct
{
  PatternType type;
  GTimeSpan   min_age;
  union {
    struct {
      GFile *directory;
      gchar *glob;
    } glob;
    struct {
      GFile *file;
    } file;
  };
} Pattern;

struct _IdeDirectoryReaper
{
  GObject  parent_instance;
  GArray  *patterns;
};

G_DEFINE_TYPE (IdeDirectoryReaper, ide_directory_reaper, G_TYPE_OBJECT)

enum {
  REMOVE_FILE,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static gboolean
emit_remove_file_from_main_cb (gpointer data)
{
  gpointer *pair = data;

  g_signal_emit (pair[0], signals [REMOVE_FILE], 0, pair[1]);
  g_object_unref (pair[0]);
  g_object_unref (pair[1]);
  g_slice_free1 (sizeof (gpointer) * 2, pair);

  return G_SOURCE_REMOVE;
}

static gboolean
file_delete (IdeDirectoryReaper  *self,
             GFile               *file,
             GCancellable        *cancellable,
             GError             **error)
{
  gpointer *data = g_slice_alloc (sizeof (gpointer) * 2);

  data[0] = g_object_ref (self);
  data[1] = g_object_ref (file);

  /* XXX:
   *
   * It would be awesome if we didn't round-trip to the main
   * thread for every one of these files. At least group some
   * together occasionally.
   */

  g_idle_add_full (G_PRIORITY_LOW + 1000,
                   emit_remove_file_from_main_cb,
                   data, NULL);

  return g_file_delete (file, cancellable, error);
}

static void
clear_pattern (gpointer data)
{
  Pattern *p = data;

  switch (p->type)
    {
    case PATTERN_GLOB:
      g_clear_object (&p->glob.directory);
      g_clear_pointer (&p->glob.glob, g_free);
      break;

    case PATTERN_FILE:
      g_clear_object (&p->file.file);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
ide_directory_reaper_finalize (GObject *object)
{
  IdeDirectoryReaper *self = (IdeDirectoryReaper *)object;

  g_clear_pointer (&self->patterns, g_array_unref);

  G_OBJECT_CLASS (ide_directory_reaper_parent_class)->finalize (object);
}

static void
ide_directory_reaper_class_init (IdeDirectoryReaperClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_directory_reaper_finalize;

  /**
   * IdeDirectoryReaper::remove-file:
   * @self: a #IdeDirectoryReaper
   * @file: a #GFile
   *
   * The "remove-file" signal is emitted for each file that is removed by the
   * #IdeDirectoryReaper instance. This may be useful if you want to show the
   * user what was processed by the reaper.
   */
  signals [REMOVE_FILE] =
    g_signal_new_class_handler ("remove-file",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                NULL, NULL,
                                ide_marshal_VOID__OBJECT,
                                G_TYPE_NONE,
                                1,
                                G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [REMOVE_FILE],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECTv);
}

static void
ide_directory_reaper_init (IdeDirectoryReaper *self)
{
  self->patterns = g_array_new (FALSE, FALSE, sizeof (Pattern));
  g_array_set_clear_func (self->patterns, clear_pattern);
}

void
ide_directory_reaper_add_directory (IdeDirectoryReaper *self,
                                    GFile              *directory,
                                    GTimeSpan           min_age)
{
  g_return_if_fail (IDE_IS_DIRECTORY_REAPER (self));
  g_return_if_fail (G_IS_FILE (directory));

  ide_directory_reaper_add_glob (self, directory, NULL, min_age);
}

void
ide_directory_reaper_add_glob (IdeDirectoryReaper *self,
                               GFile              *directory,
                               const gchar        *glob,
                               GTimeSpan           min_age)
{
  Pattern p = { 0 };

  g_return_if_fail (IDE_IS_DIRECTORY_REAPER (self));
  g_return_if_fail (G_IS_FILE (directory));

  if (glob == NULL)
    glob = "*";

  p.type = PATTERN_GLOB;
  p.min_age = ABS (min_age);
  p.glob.directory = g_object_ref (directory);
  p.glob.glob = g_strdup (glob);

  g_array_append_val (self->patterns, p);
}

void
ide_directory_reaper_add_file (IdeDirectoryReaper *self,
                               GFile              *file,
                               GTimeSpan           min_age)
{
  Pattern p = { 0 };

  g_return_if_fail (IDE_IS_DIRECTORY_REAPER (self));
  g_return_if_fail (G_IS_FILE (file));

  p.type = PATTERN_FILE;
  p.min_age = ABS (min_age);
  p.file.file = g_object_ref (file);

  g_array_append_val (self->patterns, p);
}

IdeDirectoryReaper *
ide_directory_reaper_new (void)
{
  return g_object_new (IDE_TYPE_DIRECTORY_REAPER, NULL);
}

static gboolean
remove_directory_with_children (IdeDirectoryReaper  *self,
                                GFile               *file,
                                GCancellable        *cancellable,
                                GError             **error)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) enum_error = NULL;
  g_autofree gchar *uri = NULL;
  gpointer infoptr;

  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  uri = g_file_get_uri (file);
  g_debug ("Removing uri recursively \"%s\"", uri);

  enumerator = g_file_enumerate_children (file,
                                          G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                          G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          cancellable,
                                          &enum_error);


  if (enumerator == NULL)
    {
      /* If the directory does not exist, nothing to do */
      if (g_error_matches (enum_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        return TRUE;
      g_propagate_error (error, g_steal_pointer (&enum_error));
      return FALSE;
    }

  g_assert (enum_error == NULL);

  while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, cancellable, &enum_error)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      g_autoptr(GFile) child = g_file_enumerator_get_child (enumerator, info);
      GFileType file_type = g_file_info_get_file_type (info);

      if (!g_file_info_get_is_symlink (info) && file_type == G_FILE_TYPE_DIRECTORY)
        {
          if (!remove_directory_with_children (self, child, cancellable, error))
            return FALSE;
        }

      if (!file_delete (self, child, cancellable, error))
        return FALSE;
    }

  if (enum_error != NULL)
    {
      g_propagate_error (error, g_steal_pointer (&enum_error));
      return FALSE;
    }

  if (!g_file_enumerator_close (enumerator, cancellable, error))
    return FALSE;

  return TRUE;
}

static void
ide_directory_reaper_execute_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  IdeDirectoryReaper *self;
  GArray *patterns = task_data;
  gint64 now = g_get_real_time ();

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_DIRECTORY_REAPER (source_object));
  g_assert (patterns != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self = g_task_get_source_object (task);

  for (guint i = 0; i < patterns->len; i++)
    {
      const Pattern *p = &g_array_index (patterns, Pattern, i);
      g_autoptr(GFileInfo) info = NULL;
      g_autoptr(GFileInfo) dir_info = NULL;
      g_autoptr(GPatternSpec) spec = NULL;
      g_autoptr(GFileEnumerator) enumerator = NULL;
      g_autoptr(GError) error = NULL;
      guint64 v64;

      switch (p->type)
        {
        case PATTERN_FILE:

          info = g_file_query_info (p->file.file,
                                    G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    cancellable,
                                    &error);

          if (info == NULL)
            {
              if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
                g_warning ("%s", error->message);
              break;
            }

          v64 = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

          /* mtime is in seconds */
          v64 *= G_USEC_PER_SEC;

          if (v64 < now - p->min_age)
            {
              if (!file_delete (self, p->file.file, cancellable, &error))
                g_warning ("%s", error->message);
            }

          break;

        case PATTERN_GLOB:

          spec = g_pattern_spec_new (p->glob.glob);

          if (spec == NULL)
            {
              g_warning ("Invalid pattern spec \"%s\"", p->glob.glob);
              break;
            }

          dir_info = g_file_query_info (p->glob.directory,
                                        G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                        G_FILE_ATTRIBUTE_STANDARD_TYPE",",
                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                        cancellable,
                                        &error);

          if (dir_info == NULL)
            {
              if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
                g_warning ("%s", error->message);
              break;
            }

          /* Do not follow through symlinks. */
          if (g_file_info_get_is_symlink (dir_info) ||
              g_file_info_get_file_type (dir_info) != G_FILE_TYPE_DIRECTORY)
            break;

          enumerator = g_file_enumerate_children (p->glob.directory,
                                                  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                                  G_FILE_ATTRIBUTE_STANDARD_NAME","
                                                  G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                  cancellable,
                                                  &error);

          if (enumerator == NULL)
            {
              if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
                g_warning ("%s", error->message);
              break;
            }

          while (NULL != (info = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
            {
              v64 = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

              /* mtime is in seconds */
              v64 *= G_USEC_PER_SEC;

              if (v64 < now - p->min_age)
                {
                  g_autoptr(GFile) file = g_file_enumerator_get_child (enumerator, info);
                  GFileType file_type = g_file_info_get_file_type (info);

                  if (g_file_info_get_is_symlink (info) || file_type != G_FILE_TYPE_DIRECTORY)
                    {
                      if (!file_delete (self, file, cancellable, &error))
                        {
                          g_warning ("%s", error->message);
                          g_clear_error (&error);
                        }
                    }
                  else
                    {
                      g_assert (file_type == G_FILE_TYPE_DIRECTORY);

                      if (!remove_directory_with_children (self, file, cancellable, &error) ||
                          !file_delete (self, file, cancellable, &error))
                        {
                          g_warning ("%s", error->message);
                          g_clear_error (&error);
                        }
                    }
                }

              g_clear_object (&info);
            }

          break;

        default:
          g_assert_not_reached ();
        }
    }

  g_task_return_boolean (task, TRUE);
}

static GArray *
ide_directory_reaper_copy_state (IdeDirectoryReaper *self)
{
  g_autoptr(GArray) copy = NULL;

  g_assert (IDE_IS_DIRECTORY_REAPER (self));
  g_assert (self->patterns != NULL);

  copy = g_array_new (FALSE, FALSE, sizeof (Pattern));
  g_array_set_clear_func (copy, clear_pattern);

  for (guint i = 0; i < self->patterns->len; i++)
    {
      Pattern p = g_array_index (self->patterns, Pattern, i);

      switch (p.type)
        {
        case PATTERN_GLOB:
          p.glob.directory = g_object_ref (p.glob.directory);
          p.glob.glob = g_strdup (p.glob.glob);
          break;

        case PATTERN_FILE:
          p.file.file = g_object_ref (p.file.file);
          break;

        default:
          g_assert_not_reached ();
        }

      g_array_append_val (copy, p);
    }

  return g_steal_pointer (&copy);
}

void
ide_directory_reaper_execute_async (IdeDirectoryReaper  *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GArray) copy = NULL;

  g_return_if_fail (IDE_IS_DIRECTORY_REAPER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  copy = ide_directory_reaper_copy_state (self);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_directory_reaper_execute_async);
  g_task_set_task_data (task, g_steal_pointer (&copy), (GDestroyNotify)g_array_unref);
  g_task_set_priority (task, G_PRIORITY_LOW + 1000);
  g_task_run_in_thread (task, ide_directory_reaper_execute_worker);
}

gboolean
ide_directory_reaper_execute_finish (IdeDirectoryReaper  *self,
                                     GAsyncResult        *result,
                                     GError             **error)
{
  g_return_val_if_fail (IDE_IS_DIRECTORY_REAPER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
ide_directory_reaper_execute (IdeDirectoryReaper  *self,
                              GCancellable        *cancellable,
                              GError             **error)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GArray) copy = NULL;

  g_return_val_if_fail (IDE_IS_DIRECTORY_REAPER (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  copy = ide_directory_reaper_copy_state (self);

  task = g_task_new (self, cancellable, NULL, NULL);
  g_task_set_source_tag (task, ide_directory_reaper_execute);
  g_task_set_task_data (task, g_steal_pointer (&copy), (GDestroyNotify)g_array_unref);
  g_task_run_in_thread_sync (task, ide_directory_reaper_execute_worker);

  return g_task_propagate_boolean (task, error);
}
