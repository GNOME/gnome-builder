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
 */

#pragma once

#include "ide-version-macros.h"

#include "toolchain/ide-toolchain.h"

G_BEGIN_DECLS

#define IDE_TYPE_SIMPLE_TOOLCHAIN (ide_simple_toolchain_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_DERIVABLE_TYPE (IdeSimpleToolchain, ide_simple_toolchain, IDE, SIMPLE_TOOLCHAIN, IdeToolchain)

struct _IdeSimpleToolchainClass
{
  IdeToolchainClass parent;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_3_30
IdeSimpleToolchain  *ide_simple_toolchain_new                    (IdeContext          *context,
                                                                  const gchar         *id,
                                                                  const gchar         *display_name);
IDE_AVAILABLE_IN_3_30
void                 ide_simple_toolchain_set_tool_for_language  (IdeSimpleToolchain  *self,
                                                                  const gchar         *language,
                                                                  const gchar         *tool_id,
                                                                  const gchar         *tool_path);

G_END_DECLS
