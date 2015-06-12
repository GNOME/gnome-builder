/* gb-file-search-plugin.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <ide.h>

#include "gb-plugins.h"
#include "gb-file-search-provider.h"
#include "gb-file-search-resources.h"

GB_DEFINE_EMBEDDED_PLUGIN (gb_file_search,
                           gb_file_search_get_resource (),
                           "resource:///org/gnome/builder/plugins/file-search/gb-file-search.plugin",
                           GB_DEFINE_PLUGIN_TYPE (IDE_TYPE_SEARCH_PROVIDER, GB_TYPE_FILE_SEARCH_PROVIDER))
