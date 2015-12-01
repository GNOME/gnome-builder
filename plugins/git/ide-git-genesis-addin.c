/* ide-git-genesis-addin.c
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
#include <ide.h>

#include "ide-git-clone-widget.h"
#include "ide-git-genesis-addin.h"

struct _IdeGitGenesisAddin
{
  GObject    parent_instance;

  GtkWidget *clone_widget;
};

static void genesis_addin_iface_init (IdeGenesisAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGitGenesisAddin, ide_git_genesis_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_GENESIS_ADDIN, genesis_addin_iface_init))

static void
ide_git_genesis_addin_class_init (IdeGitGenesisAddinClass *klass)
{
}

static void
ide_git_genesis_addin_init (IdeGitGenesisAddin *self)
{
}

static gchar *
ide_git_genesis_addin_get_icon_name (IdeGenesisAddin *addin)
{
  return g_strdup ("gitg-symbolic");
}

static gchar *
ide_git_genesis_addin_get_title (IdeGenesisAddin *addin)
{
  return g_strdup (_("From a Git source code repository"));
}

static GtkWidget *
ide_git_genesis_addin_get_widget (IdeGenesisAddin *addin)
{
  IdeGitGenesisAddin *self = (IdeGitGenesisAddin *)addin;

  g_assert (IDE_IS_GIT_GENESIS_ADDIN (self));

  if (self->clone_widget == NULL)
    self->clone_widget = g_object_new (IDE_TYPE_GIT_CLONE_WIDGET,
                                       "visible", TRUE,
                                       NULL);

  return self->clone_widget;
}

static void
genesis_addin_iface_init (IdeGenesisAddinInterface *iface)
{
  iface->get_title = ide_git_genesis_addin_get_title;
  iface->get_icon_name = ide_git_genesis_addin_get_icon_name;
  iface->get_widget = ide_git_genesis_addin_get_widget;
}
