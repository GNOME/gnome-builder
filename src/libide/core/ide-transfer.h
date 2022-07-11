/* ide-transfer.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#if !defined (IDE_CORE_INSIDE) && !defined (IDE_CORE_COMPILATION)
# error "Only <libide-core.h> can be included directly."
#endif

#include "ide-notification.h"
#include "ide-object.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_TRANSFER  (ide_transfer_get_type())
#define IDE_TRANSFER_ERROR (ide_transfer_error_quark())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTransfer, ide_transfer, IDE, TRANSFER, IdeObject)

struct _IdeTransferClass
{
  IdeObjectClass parent_class;

  void     (*execute_async)  (IdeTransfer          *self,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);
  gboolean (*execute_finish) (IdeTransfer          *self,
                              GAsyncResult         *result,
                              GError              **error);

  /*< private >*/
  gpointer _reserved[8];
};

typedef enum
{
  IDE_TRANSFER_ERROR_UNKNOWN = 0,
  IDE_TRANSFER_ERROR_CONNECTION_IS_METERED = 1,
} IdeTransferError;

IDE_AVAILABLE_IN_ALL
GQuark           ide_transfer_error_quark         (void);
IDE_AVAILABLE_IN_ALL
void             ide_transfer_cancel              (IdeTransfer          *self);
IDE_AVAILABLE_IN_ALL
gboolean         ide_transfer_get_completed       (IdeTransfer          *self);
IDE_AVAILABLE_IN_ALL
gboolean         ide_transfer_get_active          (IdeTransfer          *self);
IDE_AVAILABLE_IN_ALL
const gchar     *ide_transfer_get_icon_name       (IdeTransfer          *self);
IDE_AVAILABLE_IN_ALL
void             ide_transfer_set_icon_name       (IdeTransfer          *self,
                                                   const gchar          *icon_name);
IDE_AVAILABLE_IN_ALL
gdouble          ide_transfer_get_progress        (IdeTransfer          *self);
IDE_AVAILABLE_IN_ALL
void             ide_transfer_set_progress        (IdeTransfer          *self,
                                                   gdouble               progress);
IDE_AVAILABLE_IN_ALL
const gchar     *ide_transfer_get_status          (IdeTransfer          *self);
IDE_AVAILABLE_IN_ALL
void             ide_transfer_set_status          (IdeTransfer          *self,
                                                   const gchar          *status);
IDE_AVAILABLE_IN_ALL
const gchar     *ide_transfer_get_title           (IdeTransfer          *self);
IDE_AVAILABLE_IN_ALL
void             ide_transfer_set_title           (IdeTransfer          *self,
                                                   const gchar          *title);
IDE_AVAILABLE_IN_ALL
void             ide_transfer_execute_async       (IdeTransfer          *self,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean         ide_transfer_execute_finish      (IdeTransfer          *self,
                                                   GAsyncResult         *result,
                                                   GError              **error);
IDE_AVAILABLE_IN_ALL
IdeNotification *ide_transfer_create_notification (IdeTransfer          *self);

G_END_DECLS
