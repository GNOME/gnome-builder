/* ide-application-plugins.c
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

#define G_LOG_DOMAIN "ide-application"

#include <libpeas/peas.h>
#include <girepository.h>

#include "ide-application.h"
#include "ide-application-addin.h"
#include "ide-application-private.h"
#include "ide-macros.h"

static gboolean
ide_application_can_load_plugin (IdeApplication *self,
                                 PeasPluginInfo *plugin_info)
{
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (plugin_info != NULL);

  if (self->mode == IDE_APPLICATION_MODE_WORKER)
    {
      if (self->worker != plugin_info)
        return FALSE;
    }

  if (self->mode == IDE_APPLICATION_MODE_TOOL)
    {
      if (self->tool != plugin_info)
        return FALSE;
    }

  /*
   * TODO: Do ABI check on external data.
   */

  return TRUE;
}

void
ide_application_discover_plugins (IdeApplication *self)
{
  PeasEngine *engine = peas_engine_get_default ();
  static const gchar *embedded [] = { "editor", "fallback", "git", NULL };
  const GList *list;
  gint i;

  g_return_if_fail (IDE_IS_APPLICATION (self));

  peas_engine_enable_loader (engine, "python3");

  if (g_getenv ("GB_IN_TREE_PLUGINS") != NULL)
    {
      GDir *dir;

      g_irepository_require_private (g_irepository_get_default (),
                                     BUILDDIR"/libide",
                                     "Ide", "1.0", 0, NULL);

      if ((dir = g_dir_open (BUILDDIR"/plugins", 0, NULL)))
        {
          const gchar *name;

          while ((name = g_dir_read_name (dir)))
            {
              gchar *path;

              path = g_build_filename (BUILDDIR, "plugins", name, NULL);
              peas_engine_prepend_search_path (engine, path, path);
              g_free (path);
            }

          g_dir_close (dir);
        }
    }
  else
    {
      peas_engine_prepend_search_path (engine,
                                       PACKAGE_LIBDIR"/gnome-builder/plugins",
                                       PACKAGE_DATADIR"/gnome-builder/plugins");
    }

  for (i = 0; embedded [i]; i++)
    {
      gchar *path;

      path = g_strdup_printf ("resource:///org/gnome/builder/plugins/%s", embedded [i]);
      peas_engine_prepend_search_path (engine, path, path);
      g_free (path);
    }

  peas_engine_rescan_plugins (engine);

  list = peas_engine_get_plugin_list (engine);

  for (; list; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;

      g_debug ("Discovered plugin \"%s\"",
               peas_plugin_info_get_module_name (plugin_info));
    }
}

void
ide_application_load_plugins (IdeApplication *self)
{
  PeasEngine *engine;
  const GList *list;

  g_return_if_fail (IDE_IS_APPLICATION (self));

  engine = peas_engine_get_default ();
  list = peas_engine_get_plugin_list (engine);

  for (; list; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;

      if (ide_application_can_load_plugin (self, plugin_info))
        {
          g_debug ("Loading plugin \"%s\"",
                   peas_plugin_info_get_module_name (plugin_info));
          peas_engine_load_plugin (engine, plugin_info);
        }
    }
}

static void
ide_application_addin_added (PeasExtensionSet *set,
                             PeasPluginInfo   *plugin_info,
                             PeasExtension    *extension,
                             gpointer          user_data)
{
  IdeApplication *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (extension));

  ide_application_addin_load (IDE_APPLICATION_ADDIN (extension), self);
}

static void
ide_application_addin_removed (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               PeasExtension    *extension,
                               gpointer          user_data)
{
  IdeApplication *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_APPLICATION_ADDIN (extension));

  ide_application_addin_unload (IDE_APPLICATION_ADDIN (extension), self);
}

void
ide_application_load_addins (IdeApplication *self)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_APPLICATION_ADDIN,
                                         NULL);

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (ide_application_addin_added),
                           self,
                           0);

  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (ide_application_addin_removed),
                           self,
                           0);

  peas_extension_set_foreach (self->addins,
                              ide_application_addin_added,
                              self);
}
