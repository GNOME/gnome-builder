/* ide-tweaks-addin.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_TWEAKS_INSIDE) && !defined (IDE_TWEAKS_COMPILATION)
# error "Only <libide-tweaks.h> can be included directly."
#endif

#include "ide-tweaks.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_ADDIN (ide_tweaks_addin_get_type())

#define ide_tweaks_addin_bind_callback(instance, callback) \
  G_STMT_START {                                           \
    ide_tweaks_addin_add_callback(instance,                \
                                  #callback,               \
                                  G_CALLBACK (callback));  \
  } G_STMT_END

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTweaksAddin, ide_tweaks_addin, IDE, TWEAKS_ADDIN, GObject)

struct _IdeTweaksAddinClass
{
  GObjectClass parent_class;

  void (*load)   (IdeTweaksAddin *self,
                  IdeTweaks      *tweaks);
  void (*unload) (IdeTweaksAddin *self,
                  IdeTweaks      *tweaks);
};

IDE_AVAILABLE_IN_ALL
const char * const *ide_tweaks_addin_get_resource_paths (IdeTweaksAddin     *self);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_addin_set_resource_paths (IdeTweaksAddin     *self,
                                                         const char * const *resource_path);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_addin_add_callback       (IdeTweaksAddin     *self,
                                                         const char         *name,
                                                         GCallback           callback);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_addin_load               (IdeTweaksAddin     *self,
                                                         IdeTweaks          *tweaks);
IDE_AVAILABLE_IN_ALL
void                ide_tweaks_addin_unload             (IdeTweaksAddin     *self,
                                                         IdeTweaks          *tweaks);

G_END_DECLS
