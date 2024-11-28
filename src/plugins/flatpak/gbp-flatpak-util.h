/* gbp-flatpak-util.h
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
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

const char *gbp_flatpak_get_a11y_bus     (const char       **out_unix_path,
                                          const char       **out_address_suffix);
void        gbp_flatpak_set_config_dir   (IdeRunContext     *run_context);
const char *gbp_flatpak_get_default_arch (void);
gboolean    gbp_flatpak_is_ignored       (const gchar       *name);
gchar      *gbp_flatpak_get_repo_dir     (IdeContext        *context);
gchar      *gbp_flatpak_get_staging_dir  (IdePipeline       *pipeline);
gboolean    gbp_flatpak_split_id         (const gchar       *str,
                                          gchar            **id,
                                          gchar            **arch,
                                          gchar            **branch);
JsonNode *  gbp_flatpak_yaml_to_json     (const gchar       *contents,
                                          gsize              len,
                                          GError           **error);

G_END_DECLS
