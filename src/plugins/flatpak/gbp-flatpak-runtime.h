/* gbp-flatpak-runtime.h
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

#include <libide-foundry.h>

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_RUNTIME (gbp_flatpak_runtime_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakRuntime, gbp_flatpak_runtime, GBP, FLATPAK_RUNTIME, IdeRuntime)

GbpFlatpakRuntime   *gbp_flatpak_runtime_new            (const char        *name,
                                                         const char        *arch,
                                                         const char        *branch,
                                                         const char        *sdk_name,
                                                         const char        *sdk_branch,
                                                         const char        *deploy_dir,
                                                         const char        *metadata,
                                                         gboolean           is_extension);
IdeTriplet          *gbp_flatpak_runtime_get_triplet    (GbpFlatpakRuntime *self);
const gchar         *gbp_flatpak_runtime_get_branch     (GbpFlatpakRuntime *self);
const gchar         *gbp_flatpak_runtime_get_platform   (GbpFlatpakRuntime *self);
const gchar         *gbp_flatpak_runtime_get_sdk        (GbpFlatpakRuntime *self);
gchar               *gbp_flatpak_runtime_get_sdk_name   (GbpFlatpakRuntime *self);
char               **gbp_flatpak_runtime_get_refs       (GbpFlatpakRuntime *self);
const char          *gbp_flatpak_runtime_get_deploy_dir (GbpFlatpakRuntime *self);

G_END_DECLS
