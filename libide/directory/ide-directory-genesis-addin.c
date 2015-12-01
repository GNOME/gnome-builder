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
