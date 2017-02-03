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

#include "ide-debug.h"

#include "util/ide-directory-reaper.h"

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
  p.min_age = min_age;
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
  p.min_age = min_age;
  p.file.file = g_object_ref (file);

  g_array_append_val (self->patterns, p);
}

IdeDirectoryReaper *
ide_directory_reaper_new (void)
{
  return g_object_new (IDE_TYPE_DIRECTORY_REAPER, NULL);
}

static gboolean
has_expired (guint64 mtime,
             guint64 min_age)
{
  guint64 now = g_get_real_time () / G_USEC_PER_SEC;

  if (now > min_age)
    return (now - min_age) > mtime;

  return FALSE;
}

static gboolean
remove_directory_with_children (GFile         *file,
                                GCancellable  *cancellable,
                                GError       **error)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  gpointer infoptr;

  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *uri = g_file_get_uri (file);
    IDE_TRACE_MSG ("Removing directory recursively \"%s\"", uri);
  }
#endif

  enumerator = g_file_enumerate_children (file,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                          G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          cancellable,
                                          error);

  if (enumerator == NULL)
    return FALSE;

  while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      const gchar *name = g_file_info_get_name (info);
      GFile *child = g_file_get_child (file, name);

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
          if (!remove_directory_with_children (child, cancellable, error))
            return FALSE;
          continue;
        }

      if (!g_file_delete (child, cancellable, error))
        return FALSE;
    }

  return TRUE;
}

static void
ide_directory_reaper_execute_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  GArray *patterns = task_data;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_DIRECTORY_REAPER (source_object));
  g_assert (patterns != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  for (guint i = 0; i < patterns->len; i++)
    {
      const Pattern *p = &g_array_index (patterns, Pattern, i);
      g_autoptr(GFileInfo) info = NULL;
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

          if (has_expired (v64, p->min_age))
            {
              if (!g_file_delete (p->file.file, cancellable, &error))
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

          enumerator = g_file_enumerate_children (p->glob.directory,
                                                  G_FILE_ATTRIBUTE_STANDARD_NAME","
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
              const gchar *name;

              name = g_file_info_get_name (info);
              v64 = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

              if (has_expired (v64, p->min_age))
                {
                  g_autoptr(GFile) file = g_file_get_child (p->glob.directory, name);

                  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable) == G_FILE_TYPE_DIRECTORY)
                    {
                      if (!remove_directory_with_children (file, cancellable, &error))
                        {
                          g_warning ("%s", error->message);
                          g_clear_error (&error);
                        }
                    }
                  else if (!g_file_delete (file, cancellable, &error))
                    {
                      g_warning ("%s", error->message);
                      g_clear_error (&error);
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
