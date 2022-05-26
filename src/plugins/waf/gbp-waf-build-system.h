/* gbp-waf-build-system.h
 *
 * Copyright 2019 Alex Mitchell
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define GBP_TYPE_WAF_BUILD_SYSTEM (gbp_waf_build_system_get_type())

G_DECLARE_FINAL_TYPE (GbpWafBuildSystem, gbp_waf_build_system, GBP, WAF_BUILD_SYSTEM, IdeObject)

gboolean  gbp_waf_build_system_wants_python2 (GbpWafBuildSystem  *self,
                                              GError            **error);
char     *gbp_waf_build_system_locate_waf    (GbpWafBuildSystem  *self);

G_END_DECLS
