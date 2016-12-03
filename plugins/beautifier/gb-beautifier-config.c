/* gb-beautifier-config.c
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <string.h>
#include <ide.h>

#include "gb-beautifier-private.h"
#include "gb-beautifier-config.h"

static void
config_entry_clear_func (gpointer data)
{
  GbBeautifierConfigEntry *entry = (GbBeautifierConfigEntry *)data;

  g_assert (entry != NULL);

  g_object_unref (entry->file);
  g_free (entry->name);
  g_free (entry->lang_id);

  if (entry->command_args != NULL)
    g_ptr_array_unref (entry->command_args);
}

static void
map_entry_clear_func (gpointer data)
{
  GbBeautifierMapEntry *entry = (GbBeautifierMapEntry *)data;

  g_assert (entry != NULL);

  g_free (entry->lang_id);
  g_free (entry->default_profile);
}

static gboolean
gb_beautifier_config_check_duplicates (GbBeautifierWorkbenchAddin *self,
                                       GArray                     *entries,
                                       const gchar                *lang_id,
                                       const gchar                *display_name)
{
  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
  g_assert (entries != NULL);
  g_assert (!ide_str_empty0 (lang_id));
  g_assert (!ide_str_empty0 (display_name));

  for (gint i = 0; i < entries->len; ++i)
    {
      GbBeautifierConfigEntry *entry = &g_array_index (entries, GbBeautifierConfigEntry, i);

      /* Check for a NULL element at the array end */
      if (entry->file == NULL)
        break;

      if (0 == g_strcmp0 (entry->lang_id, lang_id) &&
          0 == g_strcmp0 (entry->name, display_name))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gb_beautifier_map_check_duplicates (GbBeautifierWorkbenchAddin *self,
                                    GArray                     *map,
                                    const gchar                *lang_id)
{
  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
  g_assert (map != NULL);
  g_assert (!ide_str_empty0 (lang_id));

  for (gint i = 0; i < map->len; ++i)
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

static gboolean
add_entries_from_config_ini_file (GbBeautifierWorkbenchAddin *self,
                                  const gchar                *base_path,
                                  const gchar                *lang_id,
                                  const gchar                *real_lang_id,
                                  GArray                     *entries,
                                  const gchar                *map_default,
                                  gboolean                    is_from_map)
{
  g_autoptr(GKeyFile) key_file = NULL;
  g_autofree gchar *ini_path = NULL;
  g_auto(GStrv) profiles = NULL;
  g_autofree gchar *default_profile = NULL;
  GbBeautifierConfigEntry entry;
  gsize nb_profiles;
  GError *error = NULL;

  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
  g_assert (!ide_str_empty0 (base_path));
  g_assert (!ide_str_empty0 (lang_id));
  g_assert (!ide_str_empty0 (real_lang_id));
  g_assert (entries != NULL);

  key_file = g_key_file_new ();
  ini_path = g_build_filename (base_path, real_lang_id, "config.ini", NULL);
  if (!g_key_file_load_from_file (key_file, ini_path, G_KEY_FILE_NONE, &error))
    return FALSE;

  if (map_default != NULL)
    default_profile = g_strdup (map_default);

  if (NULL != (profiles = g_key_file_get_groups (key_file, &nb_profiles)))
    {
      for (gint i = 0; i < nb_profiles; ++i)
        {
          g_autofree gchar *display_name = NULL;
          g_autofree gchar *command = NULL;
          g_autofree gchar *command_pattern = NULL;
          g_autoptr(GFile) file = NULL;
          g_autofree gchar *config_path = NULL;
          g_auto(GStrv) strv = NULL;
          gint argc;
          gchar *profile;
          gboolean has_command;
          gboolean has_command_pattern;

          profile = profiles [i];
          if (0 == g_strcmp0 (profile, "global"))
            {
              if (!is_from_map && default_profile == NULL)
                default_profile = g_key_file_get_string (key_file, profile, "default", &error);

              continue;
            }

          if (NULL == (display_name = g_key_file_get_string (key_file, profile, "name", &error)))
            goto fail;

          if (gb_beautifier_config_check_duplicates (self, entries, lang_id, display_name))
            continue;

          has_command = g_key_file_has_key (key_file, profile, "command", &error);
          has_command_pattern = g_key_file_has_key (key_file, profile, "command-pattern", &error);
          if (!has_command && !has_command_pattern)
            {
              g_warning ("beautifier plugin: neither command nor command-pattern keys found");
              g_warning ("entry \"%s\" disabled", display_name);
              continue;
            }

          memset (&entry, 0, sizeof(GbBeautifierConfigEntry));
          if (has_command)
            {
              command = g_key_file_get_string (key_file, profile, "command", &error);
              if (0 == g_strcmp0 (command, "clang-format"))
                entry.command = GB_BEAUTIFIER_CONFIG_COMMAND_CLANG_FORMAT;
              else
                {
                  g_warning ("beautifier plugin: command key out of possible values");
                  g_warning ("entry \"%s\" disabled", display_name);
                  continue;
                }
            }
          else
            {
              command_pattern = g_key_file_get_string (key_file, profile, "command-pattern", &error);
              if (!g_shell_parse_argv (command_pattern, &argc, &strv, &error))
                {
                  g_warning ("beautifier plugin: \"%s\"", error->message);
                  return FALSE;
                }

              entry.command = GB_BEAUTIFIER_CONFIG_COMMAND_NONE;
              entry.command_args = g_ptr_array_new_with_free_func (g_free);
              for (gint j = 0; strv [j] != NULL; ++j)
                g_ptr_array_add (entry.command_args, g_strdup (strv [j]));

              g_ptr_array_add (entry.command_args, NULL);
            }

          config_path = g_build_filename (base_path, real_lang_id, profile, NULL);
          file = g_file_new_for_path (config_path);
          if (g_file_query_exists (file, NULL))
            {
              entry.name = g_steal_pointer (&display_name);
              entry.file = g_steal_pointer (&file);
              entry.lang_id = g_strdup (lang_id);

              if (0 == g_strcmp0 (default_profile, profile))
                {
                  entry.is_default = TRUE;
                  g_clear_pointer (&default_profile, g_free);
                }
              else
                entry.is_default = FALSE;

              g_array_append_val (entries, entry);
            }
          else
            g_warning ("Can't find \"%s\"", config_path);
        }
    }

  return TRUE;

fail:
  g_warning ("\"%s\"", error->message);

  return FALSE;
}

static gboolean
is_a_lang_id (GbBeautifierWorkbenchAddin *self,
              const gchar                *lang_id)
{
  GtkSourceLanguageManager *lang_manager;
  const gchar * const * lang_ids = NULL;

  lang_manager = gtk_source_language_manager_get_default ();
  lang_ids = gtk_source_language_manager_get_language_ids (lang_manager);

  return g_strv_contains (lang_ids, lang_id);
}

static gboolean
add_entries_from_base_path (GbBeautifierWorkbenchAddin *self,
                            const gchar                *base_path,
                            GArray                     *entries,
                            GArray                     *map)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GFile) parent_file = NULL;
  GFileInfo *child_info;
  GError *error = NULL;
  gboolean ret = FALSE;

  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
  g_assert (!ide_str_empty0 (base_path));
  g_assert (entries != NULL);
  g_assert (map != NULL);

  parent_file = g_file_new_for_path (base_path);
  if (NULL == (enumerator = g_file_enumerate_children (parent_file,
                                                       G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                                       G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                       G_FILE_QUERY_INFO_NONE,
                                                       NULL,
                                                       &error)))
    {
      g_warning ("\"%s\"", error->message);
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
              add_entries_from_config_ini_file (self, base_path, real_lang_id, real_lang_id, entries, NULL, FALSE))
            ret = TRUE;

          for (gint i = 0; i < map->len; ++i)
            {
              entry = &g_array_index (map, GbBeautifierMapEntry, i);
              if (0 == g_strcmp0 (entry->profile, real_lang_id) &&
                  add_entries_from_config_ini_file (self, base_path, entry->lang_id, real_lang_id, entries, entry->default_profile, TRUE))
                ret = TRUE;
            }
        }
    }

  if (error != NULL)
    g_warning ("\"%s\"", error->message);

  return ret;
}

static GArray *
gb_beautifier_config_get_map (GbBeautifierWorkbenchAddin *self,
                              const gchar                *path)
{
  GArray *map;
  g_autofree gchar *file_name = NULL;
  g_autoptr(GKeyFile) key_file = NULL;
  g_auto(GStrv) lang_ids = NULL;
  gsize nb_lang_ids;
  GError *error = NULL;

  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
  g_assert (!ide_str_empty0 (path));

  map = g_array_new (TRUE, TRUE, sizeof (GbBeautifierMapEntry));
  g_array_set_clear_func (map, map_entry_clear_func);
  file_name = g_build_filename (path,
                                "global.ini",
                                NULL);

  key_file = g_key_file_new ();
  if (g_key_file_load_from_file (key_file, file_name, G_KEY_FILE_NONE, &error) &&
      NULL != (lang_ids = g_key_file_get_groups (key_file, &nb_lang_ids)))
    {
      for (gint i = 0; i < nb_lang_ids; ++i)
        {
          g_autofree gchar *profile = NULL;
          g_autofree gchar *default_profile = NULL;
          GbBeautifierMapEntry entry;
          gchar *lang_id = lang_ids [i];

          if (!is_a_lang_id (self, lang_id) ||
              NULL == (profile = g_key_file_get_string (key_file, lang_id, "map", &error)))
            continue;

          if (gb_beautifier_map_check_duplicates (self, map, lang_id))
            continue;

          default_profile = g_key_file_get_string (key_file, lang_id, "default", &error);

          entry.lang_id = g_strdup (lang_id);
          entry.profile = g_steal_pointer (&profile);
          entry.default_profile = g_steal_pointer (&default_profile);
          g_array_append_val (map, entry);
        }
    }

  return map;
}

GArray *
gb_beautifier_config_get_entries (GbBeautifierWorkbenchAddin *self)
{
  IdeContext *context;
  IdeVcs *vcs;
  GArray *entries;
  GArray *map = NULL;
  g_autofree gchar *project_config_path = NULL;
  g_autofree gchar *user_config_path = NULL;

  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));

  entries = g_array_new (TRUE, TRUE, sizeof (GbBeautifierConfigEntry));
  g_array_set_clear_func (entries, config_entry_clear_func);

  /* User wide config: ~/.config/gnome-builder/beautifier_plugin */
  user_config_path = g_build_filename (g_get_user_config_dir (),
                                       ide_get_program_name (),
                                       "beautifier_plugin",
                                       NULL);
  map = gb_beautifier_config_get_map (self, user_config_path);
  add_entries_from_base_path (self, user_config_path, entries, map);
  if (map != NULL)
    g_array_free (map, TRUE);

  /* Project wide config */
  if (NULL != (context = ide_workbench_get_context (self->workbench)) &&
      NULL != (vcs = ide_context_get_vcs (context)))
    {
      GFile *workdir;
      g_autofree gchar *workdir_path = NULL;

      workdir = ide_vcs_get_working_directory (vcs);
      workdir_path = g_file_get_path (workdir);
      project_config_path = g_build_filename (workdir_path,
                                              ".beautifier",
                                              NULL);
      map = gb_beautifier_config_get_map (self, project_config_path);
      add_entries_from_base_path (self, project_config_path, entries, map);
      if (map != NULL)
        g_array_free (map, TRUE);
    }

  /* System wide config */
  map = gb_beautifier_config_get_map (self, GB_BEAUTIFIER_PLUGIN_DATADIR);
  add_entries_from_base_path (self, GB_BEAUTIFIER_PLUGIN_DATADIR, entries, map);
  if (map != NULL)
    g_array_free (map, TRUE);

  return entries;
}
