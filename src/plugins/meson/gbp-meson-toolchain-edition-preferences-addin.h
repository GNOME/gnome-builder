/* gbp-meson-toolchain-edition-preferences.h
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, eitIher version 3 of the License, or
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

#define GBP_TYPE_MESON_TOOLCHAIN_EDITION_PREFERENCES_ADDIN (gbp_meson_toolchain_edition_preferences_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpMesonToolchainEditionPreferencesAddin, gbp_meson_toolchain_edition_preferences_addin, GBP, MESON_TOOLCHAIN_EDITION_PREFERENCES_ADDIN, GObject)

G_END_DECLS
