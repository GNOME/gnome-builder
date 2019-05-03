/* gbp-vagrant-runtime.h
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

G_BEGIN_DECLS

#define GBP_TYPE_VAGRANT_RUNTIME (gbp_vagrant_runtime_get_type())

G_DECLARE_FINAL_TYPE (GbpVagrantRuntime, gbp_vagrant_runtime, GBP, VAGRANT_RUNTIME, IdeRuntime)

const gchar *gbp_vagrant_runtime_get_vagrant_id (GbpVagrantRuntime *self);
const gchar *gbp_vagrant_runtime_get_provider   (GbpVagrantRuntime *self);
void         gbp_vagrant_runtime_set_provider   (GbpVagrantRuntime *self,
                                                 const gchar       *provider);
const gchar *gbp_vagrant_runtime_get_state      (GbpVagrantRuntime *self);
void         gbp_vagrant_runtime_set_state      (GbpVagrantRuntime *self,
                                                 const gchar       *state);

G_END_DECLS
