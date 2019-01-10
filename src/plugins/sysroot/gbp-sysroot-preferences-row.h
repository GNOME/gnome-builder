/* gbp-sysroot-preferences-row.h
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define GBP_TYPE_SYSROOT_PREFERENCES_ROW (gbp_sysroot_preferences_row_get_type())

G_DECLARE_FINAL_TYPE (GbpSysrootPreferencesRow, gbp_sysroot_preferences_row, GBP, SYSROOT_PREFERENCES_ROW, DzlPreferencesBin)

void gbp_sysroot_preferences_row_show_popup (GbpSysrootPreferencesRow *self);

G_END_DECLS
