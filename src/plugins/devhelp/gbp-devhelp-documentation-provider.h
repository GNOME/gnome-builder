/*gbp-devhelp-documentation-provider.h
 *
 * Copyright 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#define GBP_TYPE_DEVHELP_DOCUMENTATION_PROVIDER (gbp_devhelp_documentation_provider_get_type())

G_DECLARE_FINAL_TYPE (GbpDevhelpDocumentationProvider, gbp_devhelp_documentation_provider, GBP, DEVHELP_DOCUMENTATION_PROVIDER, IdeObject)

G_END_DECLS
