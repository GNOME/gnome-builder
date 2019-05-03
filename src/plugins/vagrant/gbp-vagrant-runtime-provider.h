/* gbp-vagrant-runtime-provider.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-foundry.h>

#include "gbp-vagrant-table.h"

G_BEGIN_DECLS

#define GBP_TYPE_VAGRANT_RUNTIME_PROVIDER (gbp_vagrant_runtime_provider_get_type())

G_DECLARE_FINAL_TYPE (GbpVagrantRuntimeProvider, gbp_vagrant_runtime_provider, GBP, VAGRANT_RUNTIME_PROVIDER, IdeObject)

void             gbp_vagrant_runtime_provider_command_async  (GbpVagrantRuntimeProvider  *self,
                                                              const gchar * const        *command,
                                                              GCancellable               *cancellable,
                                                              GAsyncReadyCallback         callback,
                                                              gpointer                    user_data);
GbpVagrantTable *gbp_vagrant_runtime_provider_command_finish (GbpVagrantRuntimeProvider  *self,
                                                              GAsyncResult               *result,
                                                              GError                    **error);

G_END_DECLS
