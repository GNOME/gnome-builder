/* indexer-info.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "indexer-info"

#include "config.h"

#include <libide-core.h>
#include <libpeas.h>
#include <gtksourceview/gtksource.h>
#include <string.h>

#include "indexer-info.h"

void
indexer_info_free (IndexerInfo *info)
{
  g_clear_pointer (&info->specs, g_ptr_array_unref);
  g_clear_pointer (&info->mime_types, g_ptr_array_unref);
  g_clear_pointer (&info->lang_ids, g_strfreev);
  g_slice_free (IndexerInfo, info);
}

GPtrArray *
collect_indexer_info (void)
{
  GtkSourceLanguageManager *manager;
  g_autoptr(GPtrArray) indexers = NULL;
  PeasEngine *engine;
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());

  manager = gtk_source_language_manager_get_default ();
  engine = peas_engine_get_default ();
  n_items = g_list_model_get_n_items (G_LIST_MODEL (engine));
  indexers = g_ptr_array_new_with_free_func ((GDestroyNotify)indexer_info_free);

  for (guint p = 0; p < n_items; p++)
    {
      g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), p);
      const gchar *module_name;
      g_autofree gchar *str = NULL;
      g_auto(GStrv) split = NULL;
      IndexerInfo *info;

      if (!peas_plugin_info_is_loaded (plugin_info) ||
          !(str = g_strdup (peas_plugin_info_get_external_data (plugin_info, "Code-Indexer-Languages"))))
        continue;

      module_name = peas_plugin_info_get_module_name (plugin_info);
      split = g_strsplit (g_strdelimit (str, ",", ';'), ";", 0);

      info = g_slice_new0 (IndexerInfo);
      info->module_name = g_intern_string (module_name);
      info->mime_types = g_ptr_array_new ();
      info->specs = g_ptr_array_new_with_free_func ((GDestroyNotify)g_pattern_spec_free);
      info->lang_ids = g_strdupv (split);

      for (guint i = 0; split[i]; i++)
        {
          GtkSourceLanguage *lang;
          const gchar *name = split[i];
          g_auto(GStrv) globs = NULL;
          g_auto(GStrv) mime_types = NULL;

          if (!(lang = gtk_source_language_manager_get_language (manager, name)))
            {
              g_warning ("No such language \"%s\" in %s plugin description",
                         name, module_name);
              continue;
            }

          globs = gtk_source_language_get_globs (lang);
          mime_types = gtk_source_language_get_mime_types (lang);

          if (globs != NULL)
            {
              for (guint j = 0; globs[j] != NULL; j++)
                {
                  g_autoptr(GPatternSpec) spec = g_pattern_spec_new (globs[j]);
                  g_ptr_array_add (info->specs, g_steal_pointer (&spec));
                }
            }

          if (mime_types != NULL)
            {
              for (guint j = 0; mime_types[j] != NULL; j++)
                g_ptr_array_add (info->mime_types, (gchar *)g_intern_string (mime_types[j]));
            }
        }

      g_ptr_array_add (indexers, g_steal_pointer (&info));
    }

  return g_steal_pointer (&indexers);
}

gboolean
indexer_info_matches (const IndexerInfo *info,
                      const gchar       *filename,
                      const gchar       *filename_reversed,
                      const gchar       *mime_type)
{
  gsize len;

  g_assert (info != NULL);
  g_assert (filename != NULL);
  g_assert (filename_reversed != NULL);

  if (mime_type != NULL)
    {
      mime_type = g_intern_string (mime_type);

      for (guint i = 0; i < info->mime_types->len; i++)
        {
          const gchar *mt = g_ptr_array_index (info->mime_types, i);

          /* interned strings can-use pointer comparison */
          if (mt == mime_type)
            return TRUE;
        }
    }

  len = strlen (filename);

  for (guint i = 0; i < info->specs->len; i++)
    {
      GPatternSpec *spec = g_ptr_array_index (info->specs, i);

      if (g_pattern_spec_match (spec, len, filename, filename_reversed))
        return TRUE;
    }

  return FALSE;
}


