/* gbp-flatpak-manifest.h
 *
 * Copyright 2016 Matthew Leeds <mleeds@redhat.com>
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include "ipc-flatpak-service.h"

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_MANIFEST (gbp_flatpak_manifest_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakManifest, gbp_flatpak_manifest, GBP, FLATPAK_MANIFEST, IdeConfig)

GbpFlatpakManifest   *gbp_flatpak_manifest_new                       (GFile                *file,
                                                                      const gchar          *id);
GFile                *gbp_flatpak_manifest_get_file                  (GbpFlatpakManifest   *self);
const gchar          *gbp_flatpak_manifest_get_primary_module        (GbpFlatpakManifest   *self);
const gchar          *gbp_flatpak_manifest_get_command               (GbpFlatpakManifest   *self);
gchar                *gbp_flatpak_manifest_get_path                  (GbpFlatpakManifest   *self);
const gchar * const  *gbp_flatpak_manifest_get_x_run_args            (GbpFlatpakManifest   *self);
const gchar * const  *gbp_flatpak_manifest_get_build_args            (GbpFlatpakManifest   *self);
const gchar * const  *gbp_flatpak_manifest_get_finish_args           (GbpFlatpakManifest   *self);
const gchar * const  *gbp_flatpak_manifest_get_sdk_extensions        (GbpFlatpakManifest   *self);
const gchar          *gbp_flatpak_manifest_get_sdk                   (GbpFlatpakManifest   *self);
const char           *gbp_flatpak_manifest_get_base                  (GbpFlatpakManifest   *self);
const char           *gbp_flatpak_manifest_get_base_version          (GbpFlatpakManifest   *self);
const gchar          *gbp_flatpak_manifest_get_platform              (GbpFlatpakManifest   *self);
const char           *gbp_flatpak_manifest_get_branch                (GbpFlatpakManifest   *self);
const char           *gbp_flatpak_manifest_get_primary_build_system  (GbpFlatpakManifest   *self);
void                  gbp_flatpak_manifest_save_async                (GbpFlatpakManifest   *self,
                                                                      GCancellable         *cancellable,
                                                                      GAsyncReadyCallback   callback,
                                                                      gpointer              user_data);
gboolean              gbp_flatpak_manifest_save_finish               (GbpFlatpakManifest   *self,
                                                                      GAsyncResult         *result,
                                                                      GError              **error);
void                  gbp_flatpak_manifest_resolve_extensions_async  (GbpFlatpakManifest   *self,
                                                                      IpcFlatpakService    *service,
                                                                      GCancellable         *cancellable,
                                                                      GAsyncReadyCallback   callback,
                                                                      gpointer              user_data);
gboolean              gbp_flatpak_manifest_resolve_extensions_finish (GbpFlatpakManifest   *self,
                                                                      GAsyncResult         *result,
                                                                      GError              **error);
void                  gbp_flatpak_manifest_apply_primary_env         (GbpFlatpakManifest   *self,
                                                                      IdeRunContext        *run_context);


G_END_DECLS
