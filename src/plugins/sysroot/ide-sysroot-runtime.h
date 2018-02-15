/* ide-sysroot-runtime.h
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#define IDE_TYPE_SYSROOT_RUNTIME (ide_sysroot_runtime_get_type())

G_DECLARE_FINAL_TYPE (IdeSysrootRuntime, ide_sysroot_runtime, IDE, SYSROOT_RUNTIME, IdeRuntime)

GObject *ide_sysroot_runtime_new (IdeContext *context, gchar* sysroot_id);
const gchar *ide_sysroot_runtime_get_sysroot_id (IdeSysrootRuntime *self);

G_END_DECLS
