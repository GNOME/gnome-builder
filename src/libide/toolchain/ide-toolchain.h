/* ide-toolchain.h
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
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
 */

#pragma once

#include "ide-object.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_TOOLCHAIN (ide_toolchain_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeToolchain, ide_toolchain, IDE, TOOLCHAIN, IdeObject)

struct _IdeToolchainClass
{
  IdeObjectClass parent;

  gpointer _reserved[12];
};

IDE_AVAILABLE_IN_3_30
IdeToolchain  *ide_toolchain_new                    (IdeContext             *context,
                                                     const gchar            *id);
IDE_AVAILABLE_IN_3_30
const gchar   *ide_toolchain_get_id                 (IdeToolchain           *self);
IDE_AVAILABLE_IN_3_30
void           ide_toolchain_set_id                 (IdeToolchain           *self,
                                                     const gchar            *id);
IDE_AVAILABLE_IN_3_30
const gchar   *ide_toolchain_get_host_system        (IdeToolchain           *self);
IDE_AVAILABLE_IN_3_30
void           ide_toolchain_set_host_system        (IdeToolchain           *self,
                                                     const gchar            *host_system);
IDE_AVAILABLE_IN_3_30
const gchar   *ide_toolchain_get_host_architecture  (IdeToolchain           *self);
IDE_AVAILABLE_IN_3_30
void           ide_toolchain_set_host_architecture  (IdeToolchain           *self,
                                                     const gchar            *host_architecture);
IDE_AVAILABLE_IN_3_30
const gchar   *ide_toolchain_get_compiler           (IdeToolchain           *self,
                                                     const gchar            *language);
IDE_AVAILABLE_IN_3_30
void           ide_toolchain_set_compiler           (IdeToolchain           *self,
                                                     const gchar            *language,
                                                     const gchar            *path);
IDE_AVAILABLE_IN_3_30
GHashTable    *ide_toolchain_get_compilers          (IdeToolchain           *self);
IDE_AVAILABLE_IN_3_30
const gchar   *ide_toolchain_get_archiver           (IdeToolchain           *self);
IDE_AVAILABLE_IN_3_30
void           ide_toolchain_set_archiver           (IdeToolchain           *self,
                                                     const gchar            *path);
IDE_AVAILABLE_IN_3_30
const gchar   *ide_toolchain_get_strip              (IdeToolchain           *self);
IDE_AVAILABLE_IN_3_30
void           ide_toolchain_set_strip              (IdeToolchain           *self,
                                                     const gchar            *path);
IDE_AVAILABLE_IN_3_30
const gchar   *ide_toolchain_get_pkg_config         (IdeToolchain           *self);
IDE_AVAILABLE_IN_3_30
void           ide_toolchain_set_pkg_config         (IdeToolchain           *self,
                                                     const gchar            *path);
IDE_AVAILABLE_IN_3_30
const gchar   *ide_toolchain_get_exe_wrapper        (IdeToolchain           *self);
IDE_AVAILABLE_IN_3_30
void           ide_toolchain_set_exe_wrapper        (IdeToolchain           *self,
                                                     const gchar            *path);

G_END_DECLS
