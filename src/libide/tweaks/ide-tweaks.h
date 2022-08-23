/* ide-tweaks.h
 *
 * Copyright $year Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_TWEAKS_INSIDE) && !defined (IDE_TWEAKS_COMPILATION)
# error "Only <libide-tweaks.h> can be included directly."
#endif

#include "ide-tweaks-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS (ide_tweaks_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTweaks, ide_tweaks, IDE, TWEAKS, IdeTweaksItem)

IDE_AVAILABLE_IN_ALL
IdeTweaks *ide_tweaks_new             (void);
IDE_AVAILABLE_IN_ALL
IdeTweaks *ide_tweaks_new_for_context (IdeContext    *context);
IDE_AVAILABLE_IN_ALL
IdeContext *ide_tweaks_get_context    (IdeTweaks     *self);
IDE_AVAILABLE_IN_ALL
const char *ide_tweaks_get_project_id (IdeTweaks     *self);
IDE_AVAILABLE_IN_ALL
void        ide_tweaks_set_project_id (IdeTweaks     *self,
                                       const char    *project_id);
IDE_AVAILABLE_IN_ALL
void       ide_tweaks_expose_object   (IdeTweaks     *self,
                                       const char    *name,
                                       GObject       *object);
IDE_AVAILABLE_IN_ALL
GObject   *ide_tweaks_get_object      (IdeTweaks     *self,
                                       const char    *name);
IDE_AVAILABLE_IN_ALL
void       ide_tweaks_add_callback    (IdeTweaks     *self,
                                       const char    *name,
                                       GCallback      callback);
IDE_AVAILABLE_IN_ALL
gboolean   ide_tweaks_load_from_file  (IdeTweaks     *self,
                                       GFile         *file,
                                       GCancellable  *cancellable,
                                       GError       **error);

G_END_DECLS
