/* ide-transfer.h
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

#ifndef IDE_TRANSFER_H
#define IDE_TRANSFER_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_TRANSFER (ide_transfer_get_type())

G_DECLARE_INTERFACE (IdeTransfer, ide_transfer, IDE, TRANSFER, GObject)

struct _IdeTransferInterface
{
  GTypeInterface parent;

  void     (*execute_async)  (IdeTransfer          *self,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);
  gboolean (*execute_finish) (IdeTransfer          *self,
                              GAsyncResult         *result,
                              GError              **error);
  gboolean (*has_completed)  (IdeTransfer          *self);
};

gdouble  ide_transfer_get_progress   (IdeTransfer          *self);
void     ide_transfer_execute_async  (IdeTransfer          *self,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);
gboolean ide_transfer_execute_finish (IdeTransfer          *self,
                                      GAsyncResult         *result,
                                      GError              **error);
gboolean ide_transfer_has_completed  (IdeTransfer          *self);

G_END_DECLS

#endif /* IDE_TRANSFER_H */
