/* gb-beautifier-config.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "beautifier-config"

#include <string.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libide-editor.h>
#include <libpeas/peas.h>

#include "gb-beautifier-helper.h"
#include "gb-beautifier-private.h"
#include "gb-beautifier-config.h"

static void
config_entry_clear_func (gpointer data)
{
  GbBeautifierConfigEntry *entry = (GbBeautifierConfigEntry *)data;

  g_assert (entry != NULL);

  g_clear_object (&entry->config_file);

  g_free (entry->name);
  g_free (entry->lang_id);

  if (entry->command_args != NULL)
    g_array_unref (entry->command_args);
}

static void
map_entry_clear_func (gpointer data)
{
  GbBeautifierMapEntry *entry = (GbBeautifierMapEntry *)data;

  g_assert (entry != NULL);

  g_free (entry->lang_id);
  g_free (entry->mapped_lang_id);
  g_free (entry->default_profile);
}

static void
command_arg_clear_func (gpointer data)
{
  GbBeautifierCommandArg *arg = (GbBeautifierCommandArg *)data;

  g_assert (arg != NULL);

  g_clear_pointer (&arg->str, g_free);
}

static gboolean
gb_beautifier_config_check_duplicates (GbBeautifierEditorAddin *self,
                                       GArray                  *entries,
                                       const gchar             *lang_id,
                                       const gchar             *display_name)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (entries != NULL);
  g_assert (!ide_str_empty0 (lang_id));
  g_assert (!ide_str_empty0 (display_name));

  for (guint i = 0; i < entries->len; ++i)
    {
      GbBeautifierConfigEntry *entry = &g_array_index (entries, GbBeautifierConfigEntry, i);

      /* Check for a NULL element at the array end */
      if (entry->config_file == NULL)
        break;

      if (0 == g_strcmp0 (entry->lang_id, lang_id) &&
          0 == g_strcmp0 (entry->name, display_name))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gb_beautifier_map_check_duplicates (GbBeautifierEditorAddin *self,
                                    GArray                  *map,
                                    const gchar             *lang_id)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (map != NULL);
  g_assert (!ide_str_empty0 (lang_id));

  for (guint i = 0; i < map->len; ++i)
    {
      GbBeautifierMapEntry *entry = &g_array_index (map, GbBeautifierMapEntry, i);

      /* Check for a NULL element at the array end */
      if (entry->lang_id == NULL)
        break;

      if (0 == g_strcmp0 (entry->lang_id, lang_id))
        return TRUE;
    }

  return FALSE;
}

static gchar *
copy_to_tmp_file (GbBeautifierEditorAddin *self,
                  const gchar             *tmp_dir,
                  const gchar             *source_path,
                  gboolean                 is_executable)
{
  g_autoptr (GFile) src_file = NULL;
  g_autoptr (GFile) dst_file = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *tmp_path = NULL;
  gint fd;

  g_assert (!ide_str_empty0 (tmp_dir));
  g_assert (!ide_str_empty0 (source_path));

  tmp_path = g_build_filename (tmp_dir, "XXXXXX.txt", NULL);
  if (-1 != (fd = g_mkstemp (tmp_path)))
    {
      close (fd);
      src_file = g_file_new_for_uri (source_path);
      dst_file = g_file_new_for_path (tmp_path);
      if (g_file_copy (src_file, dst_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error))
        {
          if (is_executable)
            g_chmod (tmp_path, 0777);

          return g_steal_pointer (&tmp_path);
        }
    }

  if (error != NULL)
    {
      ide_object_warning (self,
                          /* translators: %s and %s are replaced with source file path and the error message */
                          _("Beautifier plugin: error copying the gresource config file for “%s”: %s"),
                          source_path,
                          error->message);
    }
  else
    {
      ide_object_warning (self,
                          /* translators: %s is replaced with the source file path */
                          _("Beautifier plugin: error creating temporary config file for “%s”"),
                          source_path);
    }

  return NULL;
}

static gboolean
add_entries_from_config_ini_file (GbBeautifierEditorAddin *self,
                                  const gchar             *base_path,
                                  const gchar             *lang_id,
                                  const gchar             *real_lang_id,
                                  GArray                  *entries,
                                  const gchar             *map_default,
                                  gboolean                 is_from_map,
                                  gboolean                *has_default)
{
  g_autoptr(GKeyFile) key_file = NULL;
  g_autofree gchar *ini_path = NULL;
  g_autoptr(GFile) file = NULL;
  g_auto(GStrv) profiles = NULL;
  g_autofree gchar *default_profile = NULL;
  g_autofree gchar *data = NULL;
  g_autoptr (GError) error = NULL;
  gsize data_len;
  gsize nb_profiles;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!ide_str_empty0 (base_path));
  g_assert (!ide_str_empty0 (lang_id));
  g_assert (!ide_str_empty0 (real_lang_id));
  g_assert (entries != NULL);

  *has_default = FALSE;
  key_file = g_key_file_new ();
  ini_path = g_build_filename (base_path, real_lang_id, "config.ini", NULL);
  if (g_str_has_prefix (ini_path, "resource://"))
    file = g_file_new_for_uri (ini_path);
  else
    file = g_file_new_for_path (ini_path);

  if (!g_file_load_contents (file, NULL, &data, &data_len, NULL, &error))
    {
      /* translators: %s is replaced with the .ini source file path */
      ide_object_warning (self, _("Beautifier plugin: Can’t read .ini file: %s"), error->message);
      return FALSE;
    }

  if (!g_key_file_load_from_data (key_file, data, data_len, G_KEY_FILE_NONE, &error))
    goto fail;

  if (map_default != NULL)
    default_profile = g_strdup (map_default);

  if (NULL != (profiles = g_key_file_get_groups (key_file, &nb_profiles)))
    {
      for (guint i = 0; i < nb_profiles; ++i)
        {
          g_autofree gchar *display_name = NULL;
          g_autofree gchar *command = NULL;
          g_autofree gchar *command_pattern = NULL;
          g_autofree gchar *config_name = NULL;
          g_autofree gchar *config_path = NULL;
          g_autoptr(GFile) config_file = NULL;
          g_auto(GStrv) strv = NULL;
          GbBeautifierConfigEntry entry = {0};
          gint argc;
          gchar *profile;
          gboolean has_command = FALSE;
          gboolean has_command_pattern = FALSE;

          profile = profiles [i];
          if (0 == g_strcmp0 (profile, "global"))
            {
              if (!is_from_map && default_profile == NULL)
                default_profile = g_key_file_get_string (key_file, profile, "default", NULL);

              continue;
            }

          if (NULL == (display_name = g_key_file_get_string (key_file, profile, "name", &error)))
            goto fail;

          if (gb_beautifier_config_check_duplicates (self, entries, lang_id, display_name))
            continue;

          has_command = g_key_file_has_key (key_file, profile, "command", NULL);
          has_command_pattern = g_key_file_has_key (key_file, profile, "command-pattern", NULL);
          if (!has_command && !has_command_pattern)
            {
              ide_object_warning (self,
                                  /* translators: %s is replaced with the config entry name */
                                  _("Beautifier plugin: neither command nor command-pattern keys found: entry “%s” disabled"),
                                  display_name);

              continue;
            }

          if (has_command && has_command_pattern)
            {
              ide_object_warning (self,
                                  /* translators: %s is replaced with the config entry name */
                                  _("Beautifier plugin: both command and command-pattern keys found: entry “%s” disabled"),
                                  display_name);
              continue;
            }

          if (NULL != (config_name = g_key_file_get_string (key_file, profile, "config", NULL)))
            {
              config_path = g_build_filename (base_path, real_lang_id, config_name, NULL);
              if (g_str_has_prefix (config_path, "resource://"))
                {
                  gchar *tmp_config_path = copy_to_tmp_file (self, self->tmp_dir, config_path, FALSE);

                  g_free (config_path);
                  config_path = tmp_config_path;
                  config_file = g_file_new_for_path (config_path);
                  entry.is_config_file_temp = TRUE;
                }
              else
                {
                  config_file = g_file_new_for_path (config_path);
                  if (!g_file_query_exists (config_file, NULL))
                    {
                      ide_object_warning (self,
                                          /* translators: %s and %s are replaced with the config path and the entry name */
                                          _("Beautifier plugin: config path “%s” does not exist, entry “%s” disabled"),
                                          config_path,
                                          display_name);
                      continue;
                    }
                }
            }

          if (has_command)
            {
              command = g_key_file_get_string (key_file, profile, "command", NULL);
              if (0 == g_strcmp0 (command, "clang-format"))
                entry.command = GB_BEAUTIFIER_CONFIG_COMMAND_CLANG_FORMAT;
              else
                {
                  ide_object_warning (self,
                                      /* translators: %s is replaced with the entry name */
                                      _("Beautifier plugin: command key out of possible values: entry “%s” disabled"),
                                      display_name);

                  if (entry.is_config_file_temp)
                    gb_beautifier_helper_remove_temp_for_file (self, config_file);

                  continue;
                }
            }
          else
            {
              command = g_key_file_get_string (key_file, profile, "command-pattern", NULL);
              if (g_str_has_prefix (command, "[internal]"))
                {
                  command_pattern = g_build_filename ("resource:///plugins/beautifier/internal/",
                                                      command + 10,
                                                      NULL);
                }
              else
                command_pattern = g_strdup (command);

              if (g_strstr_len (command_pattern, -1, "@c@") != NULL && config_file == NULL)
                {
                  ide_object_warning (self,
                                      /* translators: %s and %s are replaced with the profile name and the entry name */
                                      _("Beautifier plugin: @c@ in “%s” command-pattern key but no config file set: entry “%s” disabled"),
                                      profile,
                                      display_name);
                  continue;
                }

              if (!g_shell_parse_argv (command_pattern, &argc, &strv, &error))
                {
                  if (entry.is_config_file_temp)
                    gb_beautifier_helper_remove_temp_for_file (self, config_file);

                  goto fail;
                }

              entry.command = GB_BEAUTIFIER_CONFIG_COMMAND_NONE;
              entry.command_args = g_array_new (FALSE, TRUE, sizeof (GbBeautifierCommandArg));
              g_array_set_clear_func (entry.command_args, command_arg_clear_func);

              for (gint j = 0; strv [j] != NULL; ++j)
                {
                  GbBeautifierCommandArg arg = {0};
                  gboolean is_executable = FALSE;

                  if (g_str_has_prefix (strv[j], "resource://"))
                    {
                      if (g_strstr_len (strv[j], -1, "internal"))
                        is_executable = TRUE;

                      if (NULL == (arg.str = copy_to_tmp_file (self, self->tmp_dir, strv[j], is_executable)))
                        {
                          ide_object_warning (self,
                                              /* translators: %s and %s are replaced with the profile name and the entry name */
                                              _("Beautifier plugin: can’t create temporary file for “%s”: entry “%s” disabled"),
                                              strv[j],
                                              display_name);

                          if (entry.is_config_file_temp)
                            gb_beautifier_helper_remove_temp_for_file (self, config_file);

                          gb_beautifier_helper_config_entry_remove_temp_files (self, &entry);
                          config_entry_clear_func (&entry);

                          continue;
                        }

                      arg.is_temp = TRUE;
                    }
                  else
                    arg.str = g_strdup (strv [j]);

                  g_array_append_val (entry.command_args, arg);
                }
            }

          entry.name = g_steal_pointer (&display_name);
          entry.config_file = g_steal_pointer (&config_file);
          entry.lang_id = g_strdup (lang_id);

          if (0 == g_strcmp0 (default_profile, profile))
            {
              *has_default = entry.is_default = TRUE;
              g_clear_pointer (&default_profile, g_free);
            }
          else
            entry.is_default = FALSE;

          g_array_append_val (entries, entry);
        }
    }

  return TRUE;

fail:
  /* translators: %s is replaced with the error message */
  ide_object_warning (self, _("Beautifier plugin: “%s”"), error->message);

  return FALSE;
}

static gboolean
is_a_lang_id (GbBeautifierEditorAddin *self,
              const gchar             *lang_id)
{
  GtkSourceLanguageManager *lang_manager;
  const gchar * const * lang_ids = NULL;

  lang_manager = gtk_source_language_manager_get_default ();
  lang_ids = gtk_source_language_manager_get_language_ids (lang_manager);

  return g_strv_contains (lang_ids, lang_id);
}

static gboolean
add_entries_from_base_path (GbBeautifierEditorAddin *self,
                            const gchar             *base_path,
                            GArray                  *entries,
                            GArray                  *map,
                            gboolean                *has_default)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GFile) parent_file = NULL;
  g_autoptr (GError) error = NULL;
  GFileInfo *child_info;
  gboolean ret = FALSE;
  gboolean ret_has_default = FALSE;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!ide_str_empty0 (base_path));
  g_assert (entries != NULL);
  g_assert (map != NULL);

  *has_default = FALSE;

  if (g_str_has_prefix (base_path, "resource://"))
    parent_file = g_file_new_for_uri (base_path);
  else
    parent_file = g_file_new_for_path (base_path);

  if (NULL == (enumerator = g_file_enumerate_children (parent_file,
                                                       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                                       G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                       G_FILE_QUERY_INFO_NONE,
                                                       NULL,
                                                       &error)))
    {
      g_debug ("\"%s\"", error->message);
      return FALSE;
    }

  while ((child_info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      g_autoptr(GFileInfo) info = child_info;
      GFileType type;
      GbBeautifierMapEntry *entry;
      const gchar *real_lang_id;

      type = g_file_info_get_file_type (info);
      if (type == G_FILE_TYPE_DIRECTORY)
        {
          real_lang_id = g_file_info_get_display_name (info);
          if (is_a_lang_id (self, real_lang_id) &&
              add_entries_from_config_ini_file (self,
                                                base_path,
                                                real_lang_id,
                                                real_lang_id,
                                                entries,
                                                NULL,
                                                FALSE,
                                                &ret_has_default))
            ret = TRUE;

          *has_default |= ret_has_default;

          for (guint i = 0; i < map->len; ++i)
            {
              entry = &g_array_index (map, GbBeautifierMapEntry, i);
              if (0 == g_strcmp0 (entry->mapped_lang_id, real_lang_id) &&
                  add_entries_from_config_ini_file (self,
                                                    base_path,
                                                    entry->lang_id,
                                                    real_lang_id,
                                                    entries,
                                                    entry->default_profile,
                                                    TRUE,
                                                    &ret_has_default))
                ret = TRUE;

              *has_default |= ret_has_default;
            }
        }
    }

  if (error != NULL)
    {
      /* translators: %s is replaced with the error message */
      ide_object_warning (self,_("Beautifier plugin: %s"), error->message);
    }

  return ret;
}

static GArray *
gb_beautifier_config_get_map (GbBeautifierEditorAddin *self,
                              const gchar             *path)
{
  GArray *map;
  g_autofree gchar *file_name = NULL;
  g_autoptr(GKeyFile) key_file = NULL;
  g_autoptr (GFile) file = NULL;
  g_auto(GStrv) lang_ids = NULL;
  g_autofree gchar *data = NULL;
  g_autoptr (GError) error = NULL;
  gsize nb_lang_ids;
  gsize data_len;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!ide_str_empty0 (path));

  map = g_array_new (TRUE, TRUE, sizeof (GbBeautifierMapEntry));
  g_array_set_clear_func (map, map_entry_clear_func);

  file_name = g_build_filename (path,  "global.ini", NULL);
  if (g_str_has_prefix (file_name, "resource://"))
    file = g_file_new_for_uri (file_name);
  else
    file = g_file_new_for_path (file_name);

  key_file = g_key_file_new ();
  if (!g_file_query_exists (file, NULL))
    return map;

  if (!g_file_load_contents (file, NULL, &data, &data_len, NULL, NULL))
    {
      /* translators: %s is replaced with a path name */
      ide_object_warning (self, _("Beautifier plugin: can’t read the following resource file: “%s”"), file_name);
      return map;
    }

  if (g_key_file_load_from_data (key_file, data, data_len, G_KEY_FILE_NONE, &error) &&
      NULL != (lang_ids = g_key_file_get_groups (key_file, &nb_lang_ids)))
    {
      for (guint i = 0; i < nb_lang_ids; ++i)
        {
          g_autofree gchar *mapped_lang_id = NULL;
          g_autofree gchar *default_profile = NULL;
          GbBeautifierMapEntry entry;
          gchar *lang_id = lang_ids [i];

          if (!is_a_lang_id (self, lang_id) ||
              NULL == (mapped_lang_id = g_key_file_get_string (key_file, lang_id, "map", NULL)))
            continue;

          if (gb_beautifier_map_check_duplicates (self, map, lang_id))
            continue;

          default_profile = g_key_file_get_string (key_file, lang_id, "default", NULL);

          entry.lang_id = g_strdup (lang_id);
          entry.mapped_lang_id = g_steal_pointer (&mapped_lang_id);
          entry.default_profile = g_steal_pointer (&default_profile);
          g_array_append_val (map, entry);
        }
    }

  return map;
}

void
gb_beautifier_entries_result_free (gpointer data)
{
  GbBeautifierEntriesResult *result = (GbBeautifierEntriesResult *)data;

  g_return_if_fail (result != NULL);

  if (result->entries != NULL)
    g_array_unref (result->entries);

  g_slice_free (GbBeautifierEntriesResult, result);
}

static void
get_entries_worker (IdeTask      *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)source_object;
  GbBeautifierEntriesResult *result;
  g_autofree gchar *project_config_path = NULL;
  g_autofree gchar *user_config_path = NULL;
  g_autofree gchar *project_id = NULL;
  g_autoptr(GFile) workdir = NULL;
  GArray *entries;
  GArray *map = NULL;
  gchar *configdir;
  gboolean has_default = FALSE;
  gboolean ret_has_default = FALSE;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (self->context == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to initialized the Beautifier plugin, no IdeContext ready");
      return;
    }

  entries = g_array_new (TRUE, TRUE, sizeof (GbBeautifierConfigEntry));
  g_array_set_clear_func (entries, config_entry_clear_func);

  /* User wide config: ~/.config/gnome-builder/beautifier_plugin */
  user_config_path = g_build_filename (g_get_user_config_dir (),
                                       ide_get_program_name (),
                                       "beautifier_plugin",
                                       NULL);
  map = gb_beautifier_config_get_map (self, user_config_path);
  add_entries_from_base_path (self, user_config_path, entries, map, &ret_has_default);
  has_default |= ret_has_default;

  g_clear_pointer (&map, g_array_unref);

  project_id = ide_context_dup_project_id (self->context);

  /* Project wide config */
  if (project_id != NULL)
    {
      if (ide_str_equal0 (project_id, "Builder"))
        {
          configdir = g_strdup ("resource:///plugins/beautifier/self/");
          map = gb_beautifier_config_get_map (self, configdir);
          add_entries_from_base_path (self, configdir, entries, map, &ret_has_default);
          has_default |= ret_has_default;
          g_clear_pointer (&configdir, g_free);

          g_clear_pointer (&map, g_array_unref);
        }
      else if ((workdir = ide_context_ref_workdir (self->context)))
        {
          project_config_path = g_build_filename (g_file_peek_path (workdir),
                                                  ".beautifier",
                                                  NULL);
          map = gb_beautifier_config_get_map (self, project_config_path);
          add_entries_from_base_path (self, project_config_path, entries, map, &ret_has_default);
          has_default |= ret_has_default;

          g_clear_pointer (&map, g_array_unref);
        }
    }

  /* System wide config */
  configdir = g_strdup ("resource:///plugins/beautifier/config/");

  map = gb_beautifier_config_get_map (self, configdir);
  add_entries_from_base_path (self, configdir, entries, map, &ret_has_default);
  g_clear_pointer (&configdir, g_free);
  has_default |= ret_has_default;

  g_clear_pointer (&map, g_array_unref);

  result = g_slice_new0 (GbBeautifierEntriesResult);
  result->entries = g_steal_pointer (&entries);
  result->has_default = has_default;

  ide_task_return_pointer (task, result, gb_beautifier_entries_result_free);
}

void
gb_beautifier_config_get_entries_async (GbBeautifierEditorAddin *self,
                                        gboolean                *has_default,
                                        GAsyncReadyCallback      callback,
                                        GCancellable            *cancellable,
                                        gpointer                 user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (callback != NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gb_beautifier_config_get_entries_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  ide_task_run_in_thread (task, get_entries_worker);
}

GbBeautifierEntriesResult *
gb_beautifier_config_get_entries_finish (GbBeautifierEditorAddin  *self,
                                         GAsyncResult             *result,
                                         GError                  **error)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (ide_task_is_valid (result, self));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}
