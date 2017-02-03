/* ide-autotools-application-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-autotools-application-addin"

#include "ide-autotools-application-addin.h"

struct _IdeAutotoolsApplicationAddin
{
  GObject parent_instance;
};

static void
ide_autotools_application_addin_load (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  gpointer infoptr;
  GTimeVal now;

  g_assert (IDE_IS_AUTOTOOLS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  /*
   * TODO: Move this to an IdeDirectoryReaper
   */

  path = g_build_filename (g_get_user_cache_dir (),
                           "gnome-builder",
                           "makecache",
                           NULL);
  file = g_file_new_for_path (path);

  enumerator = g_file_enumerate_children (file,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          NULL);

  if (enumerator == NULL)
    return;

  g_get_current_time (&now);

  while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, NULL, NULL)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      const gchar *name = g_file_info_get_name (info);
      const gchar *suffix = strrchr (name, '.');

      if (suffix && g_str_has_prefix (suffix, ".tmp-"))
        {
          gint64 time_at;

          suffix += IDE_LITERAL_LENGTH (".tmp-");
          time_at = g_ascii_strtoll (suffix, NULL, 10);

          if (time_at == G_MININT64 || time_at == G_MAXINT64)
            continue;

          if (time_at + 60 < now.tv_sec)
            {
              g_autoptr(GFile) child = NULL;

              child = g_file_get_child (file, name);
              g_file_delete (child, NULL, NULL);
            }
        }
    }
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = ide_autotools_application_addin_load;
}

G_DEFINE_TYPE_EXTENDED (IdeAutotoolsApplicationAddin,
                        ide_autotools_application_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN,
                                               application_addin_iface_init))

static void
ide_autotools_application_addin_class_init (IdeAutotoolsApplicationAddinClass *klass)
{
}

static void
ide_autotools_application_addin_init (IdeAutotoolsApplicationAddin *self)
{
}
