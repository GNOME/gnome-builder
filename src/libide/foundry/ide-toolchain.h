/* ide-toolchain.h
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

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_TOOLCHAIN (ide_toolchain_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeToolchain, ide_toolchain, IDE, TOOLCHAIN, IdeObject)

struct _IdeToolchainClass
{
  IdeObjectClass parent;

  const gchar *(*get_tool_for_language) (IdeToolchain  *self,
                                         const gchar   *language,
                                         const gchar   *tool_id);

  GHashTable  *(*get_tools_for_id)      (IdeToolchain  *self,
                                         const gchar   *tool_id);

  /*< private >*/
  gpointer _reserved[16];
};

#define IDE_TOOLCHAIN_TOOL_CC          "cc"
#define IDE_TOOLCHAIN_TOOL_CPP         "cpp"
#define IDE_TOOLCHAIN_TOOL_AR          "ar"
#define IDE_TOOLCHAIN_TOOL_LD          "ld"
#define IDE_TOOLCHAIN_TOOL_STRIP       "strip"
#define IDE_TOOLCHAIN_TOOL_EXEC        "exec"
#define IDE_TOOLCHAIN_TOOL_PKG_CONFIG  "pkg-config"

#define IDE_TOOLCHAIN_LANGUAGE_ANY       "*"
#define IDE_TOOLCHAIN_LANGUAGE_C         "c"
#define IDE_TOOLCHAIN_LANGUAGE_CPLUSPLUS "c++"
#define IDE_TOOLCHAIN_LANGUAGE_PYTHON    "python"
#define IDE_TOOLCHAIN_LANGUAGE_VALA      "vala"
#define IDE_TOOLCHAIN_LANGUAGE_FORTRAN   "fortran"
#define IDE_TOOLCHAIN_LANGUAGE_D         "d"

IDE_AVAILABLE_IN_ALL
const gchar *ide_toolchain_get_id                (IdeToolchain *self);
IDE_AVAILABLE_IN_ALL
void         ide_toolchain_set_id                (IdeToolchain *self,
                                                  const gchar  *id);
IDE_AVAILABLE_IN_ALL
const gchar *ide_toolchain_get_display_name      (IdeToolchain *self);
IDE_AVAILABLE_IN_ALL
void         ide_toolchain_set_display_name      (IdeToolchain *self,
                                                  const gchar  *display_name);
IDE_AVAILABLE_IN_ALL
IdeTriplet  *ide_toolchain_get_host_triplet      (IdeToolchain *self);
IDE_AVAILABLE_IN_ALL
void         ide_toolchain_set_host_triplet      (IdeToolchain *self,
                                                  IdeTriplet   *host_triplet);
IDE_AVAILABLE_IN_ALL
const gchar *ide_toolchain_get_tool_for_language (IdeToolchain *self,
                                                  const gchar  *language,
                                                  const gchar  *tool_id);
IDE_AVAILABLE_IN_ALL
GHashTable  *ide_toolchain_get_tools_for_id      (IdeToolchain *self,
                                                  const gchar  *tool_id);

G_END_DECLS
