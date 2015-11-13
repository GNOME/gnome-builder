/* ide-loader.c
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

#include "ide-loader.h"

G_DEFINE_INTERFACE (IdeLoader, ide_loader, G_TYPE_OBJECT)

static gchar *
ide_loader_real_get_title (IdeLoader *self)
{
  return NULL;
}

static gboolean
ide_loader_real_can_load_uri (IdeLoader   *self,
                              IdeUri      *uri,
                              const gchar *content_type,
                              gint        *priority)
{
  *priority = 0;
  return FALSE;
}

static gboolean
ide_loader_real_load_uri (IdeLoader   *self,
                          IdeUri      *uri,
                          const gchar *content_type)
{
  return FALSE;
}

static void
ide_loader_default_init (IdeLoaderInterface *iface)
{
  iface->get_title = ide_loader_real_get_title;
  iface->can_load_uri = ide_loader_real_can_load_uri;
  iface->load_uri = ide_loader_real_load_uri;
}

gboolean
ide_loader_can_load_uri (IdeLoader   *self,
                         IdeUri      *uri,
                         const gchar *content_type,
                         gint        *priority)
{
  gint dummy_priority;

  g_return_val_if_fail (IDE_IS_LOADER (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  if (priority == NULL)
    priority = &dummy_priority;

  return IDE_LOADER_GET_IFACE (self)->can_load_uri (self, uri, content_type, priority);
}

gchar *
ide_loader_get_title (IdeLoader *self)
{
  g_return_val_if_fail (IDE_IS_LOADER (self), NULL);

  return IDE_LOADER_GET_IFACE (self)->get_title (self);
}

gboolean
ide_loader_load_uri (IdeLoader   *self,
                     IdeUri      *uri,
                     const gchar *content_type)
{
  g_return_val_if_fail (IDE_IS_LOADER (self), FALSE);
  g_return_val_if_fail (uri != NULL, FALSE);

  return IDE_LOADER_GET_IFACE (self)->load_uri (self, uri, content_type);
}
