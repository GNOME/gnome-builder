/* ide-language-defaults.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-language-defaults"

#include "config.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <libide-threading.h>

#include "ide-language-defaults.h"

#define SCHEMA_ID "org.gnome.builder.editor.language"
#define PATH_BASE "/org/gnome/builder/editor/language/"

static gboolean  initialized;
static gboolean  initializing;
static GList    *tasks;

G_LOCK_DEFINE (lock);

static gboolean
strv_equal (gchar **a,
            gchar **b)
{
  if (a == b)
    return TRUE;
  else if (!a && b)
    return FALSE;
  else if (a && !b)
    return FALSE;

  for (;;)
    {
      if (*a == NULL && *b == NULL)
        return TRUE;

      if (*a == NULL || *b == NULL)
        return FALSE;

      if (g_strcmp0 (*a, *b) == 0)
        continue;

      return FALSE;
    }
}

static gboolean
ide_language_defaults_migrate (GKeyFile  *key_file,
                               gint       current_version,
                               gint       new_version,
                               GError   **error)
{
  gchar **groups;
  gsize n_groups = 0;

  g_assert (key_file);
  g_assert (current_version >= 0);
  g_assert (current_version >= 0);
  g_assert (new_version > current_version);

  groups = g_key_file_get_groups (key_file, &n_groups);

  for (gsize i = 0; i < n_groups; i++)
    {
      const gchar *group = groups [i];
      g_autoptr(GSettings) settings = NULL;
      g_autofree gchar *lang_path = NULL;
      gchar **keys;
      gsize n_keys = 0;

      g_assert (group != NULL);

      if (g_str_equal (group, "global"))
        continue;

      lang_path = g_strdup_printf (PATH_BASE"%s/", group);
      g_assert (lang_path);

      settings = g_settings_new_with_path (SCHEMA_ID, lang_path);
      g_assert (G_IS_SETTINGS (settings));

      keys = g_key_file_get_keys (key_file, group, &n_keys, NULL);
      g_assert (keys);

      for (gsize j = 0; j < n_keys; j++)
        {
          const gchar *key = keys [j];
          g_autoptr(GVariant) default_value = NULL;

          g_assert (key);

          default_value = g_settings_get_default_value (settings, key);
          g_assert (default_value);

          /*
           * For all of the variant types we support, check to see if the value
           * is matching the default schema value. If so, update the key to the
           * new override value.
           *
           * This will not overwrite any change settings for files that the
           * user has previously loaded, but will for others. Overriding things
           * we have overriden gets pretty nasty, since we change things out
           * from under the user.
           *
           * That may change in the future, but not today.
           */

          if (g_variant_is_of_type (default_value, G_VARIANT_TYPE_STRING))
            {
              g_autofree gchar *current_str = NULL;
              g_autofree gchar *override_str = NULL;
              const gchar *default_str;

              default_str = g_variant_get_string (default_value, NULL);
              current_str = g_settings_get_string (settings, key);
              override_str = g_key_file_get_string (key_file, group, key, NULL);

              if (g_strcmp0 (default_str, current_str) == 0)
                g_settings_set_string (settings, key, override_str);
            }
          else if (g_variant_is_of_type (default_value, G_VARIANT_TYPE_BOOLEAN))
            {
              gboolean current_bool;
              gboolean override_bool;
              gboolean default_bool;

              default_bool = g_variant_get_boolean (default_value);
              current_bool = g_settings_get_boolean (settings, key);
              override_bool = g_key_file_get_boolean (key_file, group, key, NULL);

              if (default_bool == current_bool)
                g_settings_set_boolean (settings, key, override_bool);
            }
          else if (g_variant_is_of_type (default_value, G_VARIANT_TYPE_INT32))
            {
              gint32 current_int32;
              gint32 override_int32;
              gint32 default_int32;

              default_int32 = g_variant_get_int32 (default_value);
              current_int32 = g_settings_get_int (settings, key);
              override_int32 = g_key_file_get_integer (key_file, group, key, NULL);

              if (default_int32 == current_int32)
                g_settings_set_int (settings, key, override_int32);
            }
          else if (g_variant_is_of_type (default_value, G_VARIANT_TYPE_STRING_ARRAY))
            {
              g_auto(GStrv) current_strv = NULL;
              g_auto(GStrv) override_strv = NULL;
              g_autofree const gchar **default_strv = NULL;

              default_strv = g_variant_get_strv (default_value, NULL);
              current_strv = g_settings_get_strv (settings, key);
              override_strv = g_key_file_get_string_list (key_file, group, key, NULL, NULL);

              if (strv_equal ((gchar **)default_strv, current_strv))
                g_settings_set_strv (settings, key, (const gchar * const *)override_strv);
            }
          else
            {
              g_error ("Teach me about variant type: %s",
                       g_variant_get_type_string (default_value));
              g_assert_not_reached ();
            }
        }
    }

  return TRUE;
}

static gint
ide_language_defaults_get_current_version (const gchar  *path,
                                           GError      **error)
{
  GError *local_error = NULL;
  g_autofree gchar *contents = NULL;
  gsize length = 0;
  gint64 version;

  if (!g_file_get_contents (path, &contents, &length, &local_error))
    {
      if (g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          g_clear_error (&local_error);
          return 0;
        }
      else
        {
          g_propagate_error (error, local_error);
          return -1;
        }
    }

  if (!g_str_is_ascii (contents))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("%s contained invalid ASCII"),
                   path);
      return -1;
    }

  if ((length == 0) || (contents [0] == '\0'))
    return 0;

  version = g_ascii_strtoll (contents, NULL, 10);

  if ((version < 0) || (version >= G_MAXINT))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("Failed to parse integer from “%s”"),
                   path);
      return -1;
    }

  return version;
}

static GBytes *
ide_language_defaults_get_defaults (GError **error)
{
  return g_resources_lookup_data ("/org/gnome/builder/file-settings/defaults.ini",
                                  G_RESOURCE_LOOKUP_FLAGS_NONE, error);
}

static void
ide_language_defaults_init_worker (IdeTask      *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  g_autofree gchar *version_path = NULL;
  g_autofree gchar *version_contents = NULL;
  g_autofree gchar *version_dir = NULL;
  g_autoptr(GBytes) defaults = NULL;
  g_autoptr(GKeyFile) key_file = NULL;
  g_autoptr(GError) error = NULL;
  gint global_version;
  gint current_version;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (source_object == NULL);
  g_assert (task_data == NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  version_path = g_build_filename (g_get_user_config_dir (),
                                   ide_get_program_name (),
                                   "syntax",
                                   ".defaults",
                                   NULL);
  current_version = ide_language_defaults_get_current_version (version_path, &error);

  if (current_version < 0)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (failure);
    }

  IDE_TRACE_MSG ("Current language defaults at version %d", current_version);

  defaults = ide_language_defaults_get_defaults (&error);
  g_assert (defaults != NULL);

  key_file = g_key_file_new ();
  ret = g_key_file_load_from_data (key_file,
                                   g_bytes_get_data (defaults, NULL),
                                   g_bytes_get_size (defaults),
                                   G_KEY_FILE_NONE,
                                   &error);

  if (!ret)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (failure);
    }

  if (!g_key_file_has_group (key_file, "global") ||
      !g_key_file_has_key (key_file, "global", "version", NULL))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 _("language defaults missing version in [global] group."));
      IDE_GOTO (failure);
    }

  global_version = g_key_file_get_integer (key_file, "global", "version", &error);

  if (global_version == 0 && error != NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_GOTO (failure);
    }

  g_clear_error (&error);

  if (global_version > current_version)
    {
      if (!ide_language_defaults_migrate (key_file, current_version, global_version, &error))
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          IDE_GOTO (failure);
        }

      version_contents = g_strdup_printf ("%d", global_version);

      version_dir = g_path_get_dirname (version_path);

      if (!g_file_test (version_dir, G_FILE_TEST_IS_DIR))
        {
          if (g_mkdir_with_parents (version_dir, 0750) == -1)
            {
              ide_task_return_new_error (task,
                                         G_IO_ERROR,
                                         g_io_error_from_errno (errno),
                                         "%s", g_strerror (errno));
              IDE_GOTO (failure);
            }
        }

      IDE_TRACE_MSG ("Writing new language defaults version to \"%s\"", version_path);

      if (!g_file_set_contents (version_path, version_contents, -1, &error))
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          IDE_GOTO (failure);
        }
    }

  ide_task_return_boolean (task, TRUE);

  {
    GList *list;
    GList *iter;

    G_LOCK (lock);

    initializing = FALSE;
    initialized = TRUE;

    list = tasks;
    tasks = NULL;

    G_UNLOCK (lock);

    for (iter = list; iter; iter = iter->next)
      {
        ide_task_return_boolean (iter->data, TRUE);
        g_object_unref (iter->data);
      }

    g_list_free (list);
  }

  IDE_EXIT;

failure:
  {
    GList *list;
    GList *iter;

    G_LOCK (lock);

    initializing = FALSE;
    initialized = TRUE;

    list = tasks;
    tasks = NULL;

    G_UNLOCK (lock);

    for (iter = list; iter; iter = iter->next)
      {
        ide_task_return_new_error (iter->data,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   _("Failed to initialize defaults."));
        g_object_unref (iter->data);
      }

    g_list_free (list);
  }

  IDE_EXIT;
}

void
ide_language_defaults_init_async (GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (NULL, cancellable, callback, user_data);

  G_LOCK (lock);

  if (initialized)
    {
      ide_task_return_boolean (task, TRUE);
    }
  else if (initializing)
    {
      tasks = g_list_prepend (tasks, g_object_ref (task));
    }
  else
    {
      initializing = TRUE;
      ide_task_run_in_thread (task, ide_language_defaults_init_worker);
    }

  G_UNLOCK (lock);

  IDE_EXIT;
}

gboolean
ide_language_defaults_init_finish (GAsyncResult  *result,
                                   GError       **error)
{
  IdeTask *task = (IdeTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_TASK (task), FALSE);

  ret = ide_task_propagate_boolean (task, error);

  IDE_RETURN (ret);
}
