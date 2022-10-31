/* gbp-flatpak-aux.c
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <ide-gfile-private.h>

#include "gbp-flatpak-aux.h"

#define SYSTEM_FONTS_DIR       "/usr/share/fonts"
#define SYSTEM_LOCAL_FONTS_DIR "/usr/local/share/fonts"

/* dirs are reversed from flatpak because we will always have
 * /var/cache/fontconfig inside of flatpak. We really need another
 * way of checking this, but this is good enough for now.
 */
#define SYSTEM_FONT_CACHE_DIRS "/var/cache/fontconfig:/usr/lib/fontconfig/cache"

/* The goal of this file is to help us setup things that might be
 * needed for applications to look/work right even though they are
 * not installed. For example, we need to setup font remaps for
 * applications since "flatpak build" will not do that for us.
 */

static GFile *local;
static GFile *mapped;
static GPtrArray *maps;
static gboolean initialized;

void
gbp_flatpak_aux_init (void)
{
  g_autoptr(GString) xml_snippet = g_string_new ("");
  g_auto(GStrv) system_cache_dirs = NULL;
  g_autoptr(GFile) user1 = NULL;
  g_autoptr(GFile) user2 = NULL;
  g_autoptr(GFile) user_cache = NULL;
  g_autofree char *cache_dir = NULL;
  g_autofree char *data_dir = NULL;
  guint i;

  if (initialized)
    return;

  initialized = TRUE;

  /* It would be nice if we had a way to get XDG dirs from the host
   * system when we need to break out of flatpak to run flatpak bits
   * through the system.
   */

  if (ide_is_flatpak ())
    {
      cache_dir = g_build_filename (g_get_home_dir (), ".cache", NULL);
      data_dir = g_build_filename (g_get_home_dir (), ".local", "share", NULL);
    }
  else
    {
      cache_dir = g_strdup (g_get_user_cache_dir ());
      data_dir = g_strdup (g_get_user_data_dir ());
    }

  local = g_file_new_for_path ("/run/host/font-dirs.xml");
  mapped = g_file_new_build_filename (cache_dir, "font-dirs.xml", NULL);
  maps = g_ptr_array_new ();

  g_string_append (xml_snippet,
                   "<?xml version=\"1.0\"?>\n"
                   "<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
                   "<fontconfig>\n");

  if (_ide_path_query_exists_on_host (SYSTEM_FONTS_DIR))
    {
      /* TODO: How can we *force* this read-only? */
      g_ptr_array_add (maps, g_strdup ("--bind-mount=/run/host/fonts=" SYSTEM_FONTS_DIR));
      g_string_append_printf (xml_snippet,
                              "\t<remap-dir as-path=\"%s\">/run/host/fonts</remap-dir>\n",
                              SYSTEM_FONTS_DIR);
    }

  if (_ide_path_query_exists_on_host (SYSTEM_LOCAL_FONTS_DIR))
    {
      /* TODO: How can we *force* this read-only? */
      g_ptr_array_add (maps, g_strdup ("--bind-mount=/run/host/local-fonts=/usr/local/share/fonts"));
      g_string_append_printf (xml_snippet,
                              "\t<remap-dir as-path=\"%s\">/run/host/local-fonts</remap-dir>\n",
                              "/usr/local/share/fonts");
    }

  system_cache_dirs = g_strsplit (SYSTEM_FONT_CACHE_DIRS, ":", 0);
  for (i = 0; system_cache_dirs[i] != NULL; i++)
    {
      g_autoptr(GFile) file = g_file_new_for_path (system_cache_dirs[i]);

      if (_ide_g_file_query_exists_on_host (file, NULL))
        {
          /* TODO: How can we *force* this read-only? */
          g_ptr_array_add (maps,
                           g_strdup_printf ("--bind-mount=/run/host/fonts-cache=%s",
                                            system_cache_dirs[i]));
          break;
        }
    }

  user1 = g_file_new_build_filename (data_dir, "fonts", NULL);
  user2 = g_file_new_build_filename (g_get_home_dir (), ".fonts", NULL);
  user_cache = g_file_new_build_filename (cache_dir, "fontconfig", NULL);

  if (_ide_g_file_query_exists_on_host (user1, NULL))
    {
      g_ptr_array_add (maps, g_strdup_printf ("--filesystem=%s:ro", g_file_peek_path (user1)));
      g_string_append_printf (xml_snippet,
                              "\t<remap-dir as-path=\"%s\">/run/host/user-fonts</remap-dir>\n",
                              g_file_peek_path (user1));

    }
  else if (_ide_g_file_query_exists_on_host (user2, NULL))
    {
      g_ptr_array_add (maps, g_strdup_printf ("--filesystem=%s:ro", g_file_peek_path (user2)));
      g_string_append_printf (xml_snippet,
                              "\t<remap-dir as-path=\"%s\">/run/host/user-fonts</remap-dir>\n",
                              g_file_peek_path (user2));
    }

  if (_ide_g_file_query_exists_on_host (user_cache, NULL))
    {
      g_ptr_array_add (maps, g_strdup_printf ("--filesystem=%s:ro", g_file_peek_path (user_cache)));
      g_ptr_array_add (maps, g_strdup_printf ("--bind-mount=/run/host/user-fonts-cache=%s",
                                              g_file_peek_path (user_cache)));
    }

  g_string_append (xml_snippet, "</fontconfig>\n");

  g_file_replace_contents (mapped, xml_snippet->str, xml_snippet->len,
                           NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION,
                           NULL, NULL, NULL);

  g_ptr_array_add (maps,
                   g_strdup_printf ("--bind-mount=/run/host/font-dirs.xml=%s",
                                    g_file_peek_path (mapped)));
}

void
gbp_flatpak_aux_append_to_run_context (IdeRunContext *run_context)
{
  static const char *arg;

  g_return_if_fail (IDE_IS_RUN_CONTEXT (run_context));
  g_return_if_fail (initialized);

  if (arg == NULL)
    arg = g_strdup_printf ("--bind-mount=/run/host/font-dirs.xml=%s",
                           g_file_peek_path (mapped));

  for (guint i = 0; i < maps->len; i++)
    {
      const char *element = g_ptr_array_index (maps, i);
      ide_run_context_append_argv (run_context, element);
    }

  ide_run_context_append_argv (run_context, arg);
}
