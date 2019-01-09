/* ide-run-manager.h
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUN_MANAGER (ide_run_manager_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeRunManager, ide_run_manager, IDE, RUN_MANAGER, IdeObject)

typedef void (*IdeRunHandler) (IdeRunManager *self,
                               IdeRunner     *runner,
                               gpointer       user_data);

IDE_AVAILABLE_IN_3_32
IdeRunManager  *ide_run_manager_from_context                   (IdeContext           *context);
IDE_AVAILABLE_IN_3_32
IdeBuildTarget *ide_run_manager_get_build_target               (IdeRunManager        *self);
IDE_AVAILABLE_IN_3_32
void            ide_run_manager_set_build_target               (IdeRunManager        *self,
                                                                IdeBuildTarget       *build_target);
IDE_AVAILABLE_IN_3_32
void            ide_run_manager_cancel                         (IdeRunManager        *self);
IDE_AVAILABLE_IN_3_32
gboolean        ide_run_manager_get_busy                       (IdeRunManager        *self);
IDE_AVAILABLE_IN_3_32
const gchar    *ide_run_manager_get_handler                    (IdeRunManager        *self);
IDE_AVAILABLE_IN_3_32
void            ide_run_manager_set_handler                    (IdeRunManager        *self,
                                                                const gchar          *id);
IDE_AVAILABLE_IN_3_32
void            ide_run_manager_add_handler                    (IdeRunManager        *self,
                                                                const gchar          *id,
                                                                const gchar          *title,
                                                                const gchar          *icon_name,
                                                                const gchar          *accel,
                                                                IdeRunHandler         run_handler,
                                                                gpointer              user_data,
                                                                GDestroyNotify        user_data_destroy);
IDE_AVAILABLE_IN_3_32
void            ide_run_manager_remove_handler                 (IdeRunManager        *self,
                                                                const gchar          *id);
IDE_AVAILABLE_IN_3_32
void            ide_run_manager_run_async                      (IdeRunManager        *self,
                                                                IdeBuildTarget       *build_target,
                                                                GCancellable         *cancellable,
                                                                GAsyncReadyCallback   callback,
                                                                gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean        ide_run_manager_run_finish                     (IdeRunManager        *self,
                                                                GAsyncResult         *result,
                                                                GError              **error);
IDE_AVAILABLE_IN_3_32
void            ide_run_manager_discover_default_target_async  (IdeRunManager        *self,
                                                                GCancellable         *cancellable,
                                                                GAsyncReadyCallback   callback,
                                                                gpointer              user_data);
IDE_AVAILABLE_IN_3_32
IdeBuildTarget *ide_run_manager_discover_default_target_finish (IdeRunManager        *self,
                                                                GAsyncResult         *result,
                                                                GError              **error);

G_END_DECLS
