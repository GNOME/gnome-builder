/* ide-directory-genesis-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-directory-genesis-addin.h"
#include "ide-genesis-addin.h"

struct _IdeDirectoryGenesisAddin
{
  GObject               parent_instance;

  GtkFileChooserWidget *widget;
};

static void genesis_addin_iface_init (IdeGenesisAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeDirectoryGenesisAddin, ide_directory_genesis_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_GENESIS_ADDIN, genesis_addin_iface_init))

static void
ide_directory_genesis_addin_class_init (IdeDirectoryGenesisAddinClass *klass)
{
}

static void
ide_directory_genesis_addin_init (IdeDirectoryGenesisAddin *self)
{
}

static gchar *
ide_directory_genesis_addin_get_icon_name (IdeGenesisAddin *addin)
{
  return g_strdup ("folder-symbolic");
}

static gchar *
ide_directory_genesis_addin_get_title (IdeGenesisAddin *addin)
{
  return g_strdup (_("From an existing project on this computer"));
}

static void
ide_directory_genesis_addin_add_filters (GtkFileChooser *chooser)
{
  PeasEngine *engine = peas_engine_get_default ();
  const GList *list = peas_engine_get_plugin_list (engine);

  for (; list != NULL; list = list->next)
    {
      PeasPluginInfo *plugin_info = list->data;
      GtkFileFilter *filter;
      const gchar *pattern;
      const gchar *content_type;
      const gchar *name;
      gchar **patterns;
      gchar **content_types;
      gint i;

      if (!peas_plugin_info_is_loaded (plugin_info))
        continue;

      name = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Name");
      if (name == NULL)
        continue;

      pattern = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Pattern");
      content_type = peas_plugin_info_get_external_data (plugin_info, "X-Project-File-Filter-Content-Type");

      if (pattern == NULL && content_type == NULL)
        continue;

      patterns = g_strsplit (pattern ?: "", ",", 0);
      content_types = g_strsplit (content_type ?: "", ",", 0);

      filter = gtk_file_filter_new ();

      gtk_file_filter_set_name (filter, name);

      for (i = 0; patterns [i] != NULL; i++)
        {
          if (*patterns [i])
            gtk_file_filter_add_pattern (filter, patterns [i]);
        }

      for (i = 0; content_types [i] != NULL; i++)
        {
          if (*content_types [i])
            gtk_file_filter_add_mime_type (filter, content_types [i]);
        }

      gtk_file_chooser_add_filter (chooser, filter);

      g_strfreev (patterns);
      g_strfreev (content_types);
    }
}

static GtkWidget *
ide_directory_genesis_addin_get_widget (IdeGenesisAddin *addin)
{
  IdeDirectoryGenesisAddin *self = (IdeDirectoryGenesisAddin *)addin;

  g_assert (IDE_IS_DIRECTORY_GENESIS_ADDIN (self));

  if (self->widget == NULL)
    {
      self->widget = g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
                                   "action", GTK_FILE_CHOOSER_ACTION_OPEN,
                                   "visible", TRUE,
                                   NULL);
      ide_directory_genesis_addin_add_filters (GTK_FILE_CHOOSER (self->widget));
    }

  return GTK_WIDGET (self->widget);
}

static void
genesis_addin_iface_init (IdeGenesisAddinInterface *iface)
{
  iface->get_title = ide_directory_genesis_addin_get_title;
  iface->get_icon_name = ide_directory_genesis_addin_get_icon_name;
  iface->get_widget = ide_directory_genesis_addin_get_widget;
}
