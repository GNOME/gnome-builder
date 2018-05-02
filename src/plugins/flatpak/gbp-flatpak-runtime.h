/* gbp-flatpak-runtime.h
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#include <flatpak.h>
#include <ide.h>

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_RUNTIME (gbp_flatpak_runtime_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakRuntime, gbp_flatpak_runtime, GBP, FLATPAK_RUNTIME, IdeRuntime)

/* TODO: Get rid of this with custom installation */
#define FLATPAK_REPO_NAME "gnome-builder-builds"

GbpFlatpakRuntime   *gbp_flatpak_runtime_new          (IdeContext           *context,
                                                       FlatpakInstalledRef  *ref,
                                                       GCancellable         *cancellable,
                                                       GError              **error);
IdeTriplet          *gbp_flatpak_runtime_get_triplet  (GbpFlatpakRuntime    *self);
const gchar         *gbp_flatpak_runtime_get_branch   (GbpFlatpakRuntime    *self);
const gchar         *gbp_flatpak_runtime_get_platform (GbpFlatpakRuntime    *self);
const gchar         *gbp_flatpak_runtime_get_sdk      (GbpFlatpakRuntime    *self);
gchar               *gbp_flatpak_runtime_get_sdk_name (GbpFlatpakRuntime    *self);

G_END_DECLS
