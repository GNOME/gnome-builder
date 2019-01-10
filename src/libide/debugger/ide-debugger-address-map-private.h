/* ide-debugger-address-map-private.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-debugger-types.h"

G_BEGIN_DECLS

typedef struct _IdeDebuggerAddressMap IdeDebuggerAddressMap;

typedef struct
{
  /*
   * The file on disk that is mapped and the offset within the file.
   */
  const gchar *filename;
  guint64 offset;

  /*
   * The range within the processes address space. We only support up to 64-bit
   * address space for local and remote debugging.
   */
  IdeDebuggerAddress start;
  IdeDebuggerAddress end;

} IdeDebuggerAddressMapEntry;

IdeDebuggerAddressMap            *ide_debugger_address_map_new    (void);
void                              ide_debugger_address_map_insert (IdeDebuggerAddressMap            *self,
                                                                   const IdeDebuggerAddressMapEntry *entry);
gboolean                          ide_debugger_address_map_remove (IdeDebuggerAddressMap            *self,
                                                                   IdeDebuggerAddress                address);
const IdeDebuggerAddressMapEntry *ide_debugger_address_map_lookup (const IdeDebuggerAddressMap      *self,
                                                                   IdeDebuggerAddress                address);
void                              ide_debugger_address_map_free   (IdeDebuggerAddressMap            *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeDebuggerAddressMap, ide_debugger_address_map_free)

G_END_DECLS
