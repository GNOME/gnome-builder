/* gbp-flatpak-configuration.h
 *
 * Copyright © 2016 Matthew Leeds <mleeds@redhat.com>
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

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_CONFIGURATION (gbp_flatpak_configuration_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakConfiguration, gbp_flatpak_configuration, GBP, FLATPAK_CONFIGURATION, IdeConfiguration)

GbpFlatpakConfiguration *gbp_flatpak_configuration_new                (IdeContext              *context,
                                                                       const gchar             *id,
                                                                       const gchar             *display_name);
gboolean                 gbp_flatpak_configuration_load_from_file     (GbpFlatpakConfiguration *self,
                                                                       GFile                   *manifest);
const gchar             *gbp_flatpak_configuration_get_branch         (GbpFlatpakConfiguration *self);
void                     gbp_flatpak_configuration_set_branch         (GbpFlatpakConfiguration *self,
                                                                       const gchar             *branch);
const gchar             *gbp_flatpak_configuration_get_command        (GbpFlatpakConfiguration *self);
void                     gbp_flatpak_configuration_set_command        (GbpFlatpakConfiguration *self,
                                                                       const gchar             *command);
const gchar * const     *gbp_flatpak_configuration_get_build_args    (GbpFlatpakConfiguration *self);
void                     gbp_flatpak_configuration_set_build_args    (GbpFlatpakConfiguration *self,
                                                                       const gchar *const      *build_args);
const gchar * const     *gbp_flatpak_configuration_get_finish_args    (GbpFlatpakConfiguration *self);
void                     gbp_flatpak_configuration_set_finish_args    (GbpFlatpakConfiguration *self,
                                                                       const gchar *const      *finish_args);
GFile                   *gbp_flatpak_configuration_get_manifest       (GbpFlatpakConfiguration *self);
gchar                   *gbp_flatpak_configuration_get_manifest_path  (GbpFlatpakConfiguration *self);
const gchar             *gbp_flatpak_configuration_get_platform       (GbpFlatpakConfiguration *self);
void                     gbp_flatpak_configuration_set_platform       (GbpFlatpakConfiguration *self,
                                                                       const gchar             *platform);
const gchar             *gbp_flatpak_configuration_get_primary_module (GbpFlatpakConfiguration *self);
void                     gbp_flatpak_configuration_set_primary_module (GbpFlatpakConfiguration *self,
                                                                       const gchar             *primary_module);
const gchar             *gbp_flatpak_configuration_get_sdk            (GbpFlatpakConfiguration *self);
void                     gbp_flatpak_configuration_set_sdk            (GbpFlatpakConfiguration *self,
                                                                       const gchar             *sdk);
const gchar * const     *gbp_flatpak_configuration_get_sdk_extensions (GbpFlatpakConfiguration *self);
void                     gbp_flatpak_configuration_set_sdk_extensions (GbpFlatpakConfiguration *self,
                                                                       const gchar * const     *sdk_extensions);

G_END_DECLS
