/* gbp-podman-runtime-private.h
 *
 * Copyright 2022 Günther Wagner <info@gunibert.de>
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

typedef enum {
  LOCAL_STORAGE_CONFIGURATION,
  GLOBAL_STORAGE_CONFIGURATION
} StorageType;

char *_gbp_podman_runtime_parse_toml_line             (const char  *line);
char *_gbp_podman_runtime_parse_storage_configuration (const char  *storage_conf,
                                                       StorageType  type);

