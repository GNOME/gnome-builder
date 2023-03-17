/* gbp-meson-utils.h
 *
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
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <libide-foundry.h>

G_BEGIN_DECLS

void         gbp_meson_key_file_set_string_quoted       (GKeyFile     *keyfile,
                                                         const gchar  *group,
                                                         const gchar  *key,
                                                         const gchar  *unquoted_value);
void         gbp_meson_key_file_set_string_array_quoted (GKeyFile     *keyfile,
                                                         const gchar  *group,
                                                         const gchar  *key,
                                                         const gchar  *unquoted_value);
gchar       *gbp_meson_key_file_get_string_quoted       (GKeyFile     *key_file,
                                                         const gchar  *group_name,
                                                         const gchar  *key,
                                                         GError      **error);
const gchar *gbp_meson_get_toolchain_language           (const gchar  *meson_tool_name);
const gchar *gbp_meson_get_tool_display_name            (const gchar  *tool_id);
const gchar *gbp_meson_get_tool_binary_name             (const gchar  *tool_id);
const gchar *gbp_meson_get_tool_id_from_binary          (const gchar  *meson_tool_name);
gboolean     gbp_meson_devenv_sanity_check              (const gchar  *path);

G_END_DECLS
