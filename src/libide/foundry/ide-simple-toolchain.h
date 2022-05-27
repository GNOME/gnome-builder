/* ide-simple-toolchain.h
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-toolchain.h"

G_BEGIN_DECLS

#define IDE_TYPE_SIMPLE_TOOLCHAIN (ide_simple_toolchain_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeSimpleToolchain, ide_simple_toolchain, IDE, SIMPLE_TOOLCHAIN, IdeToolchain)

struct _IdeSimpleToolchainClass
{
  IdeToolchainClass parent;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeSimpleToolchain *ide_simple_toolchain_new                   (const gchar        *id,
                                                                const gchar        *display_name);
IDE_AVAILABLE_IN_ALL
void                ide_simple_toolchain_set_tool_for_language (IdeSimpleToolchain *self,
                                                                const gchar        *language,
                                                                const gchar        *tool_id,
                                                                const gchar        *tool_path);

G_END_DECLS
