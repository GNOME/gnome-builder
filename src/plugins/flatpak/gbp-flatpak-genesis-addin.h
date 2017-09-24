/* gbp-flatpak-genesis-addin.h
 *
 * Copyright (C) 2016 Endless Mobile, Inc.
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_GENESIS_ADDIN (gbp_flatpak_genesis_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakGenesisAddin, gbp_flatpak_genesis_addin, GBP, FLATPAK_GENESIS_ADDIN, GObject)

G_END_DECLS
