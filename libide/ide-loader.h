/* ide-loader.h
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

#ifndef IDE_LOADER_H
#define IDE_LOADER_H

#include <gio/gio.h>

#include "ide-uri.h"

G_BEGIN_DECLS

#define IDE_TYPE_LOADER (ide_loader_get_type())

G_DECLARE_INTERFACE (IdeLoader, ide_loader, IDE, LOADER, GObject)

struct _IdeLoaderInterface
{
  GTypeInterface parent_interface;

  gchar    *(*get_title)     (IdeLoader   *self);
  gboolean  (*can_load_uri)  (IdeLoader   *self,
                              IdeUri      *uri,
                              const gchar *content_type,
                              gint        *priority);
  gboolean  (*load_uri)      (IdeLoader   *self,
                              IdeUri      *uri,
                              const gchar *content_type);
};

gboolean  ide_loader_can_load_uri  (IdeLoader   *self,
                                    IdeUri      *uri,
                                    const gchar *content_type,
                                    gint        *priority);
gchar    *ide_loader_get_title     (IdeLoader *self);
gboolean  ide_loader_load_uri      (IdeLoader   *self,
                                    IdeUri      *uri,
                                    const gchar *content_type);

G_END_DECLS

#endif /* IDE_LOADER_H */
