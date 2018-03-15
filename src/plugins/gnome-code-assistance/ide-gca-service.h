/* ide-gca-service.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <ide.h>

#include "gca-service.h"

G_BEGIN_DECLS

#define IDE_TYPE_GCA_SERVICE (ide_gca_service_get_type())

G_DECLARE_FINAL_TYPE (IdeGcaService, ide_gca_service, IDE, GCA_SERVICE, IdeObject)

void        ide_gca_service_get_proxy_async  (IdeGcaService        *self,
                                              const gchar          *language_id,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
GcaService *ide_gca_service_get_proxy_finish (IdeGcaService        *self,
                                              GAsyncResult         *result,
                                              GError              **error);

G_END_DECLS
