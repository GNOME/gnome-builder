/* gbp-flatpak-configuration.h
 *
 * Copyright (C) 2016 Matthew Leeds <mleeds@redhat.com>
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

#ifndef GBP_FLATPAK_CONFIGURATION_H
#define GBP_FLATPAK_CONFIGURATION_H

#include <ide.h>

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_CONFIGURATION (gbp_flatpak_configuration_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakConfiguration, gbp_flatpak_configuration, GBP, FLATPAK_CONFIGURATION, IdeConfiguration)

GFile       *gbp_flatpak_configuration_get_manifest (GbpFlatpakConfiguration *self);
const gchar *gbp_flatpak_configuration_get_primary_module (GbpFlatpakConfiguration *self);
void         gbp_flatpak_configuration_set_primary_module (GbpFlatpakConfiguration *self,
                                                           const gchar *primary_module);

G_END_DECLS

#endif /* GBP_FLATPAK_CONFIGURATION_H */
