/* ide-session.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-session-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_SESSION (ide_session_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSession, ide_session, IDE, SESSION, GObject)

IDE_AVAILABLE_IN_ALL
IdeSession     *ide_session_new              (void);
IDE_AVAILABLE_IN_ALL
void            ide_session_append           (IdeSession      *self,
                                              IdeSessionItem  *item);
IDE_AVAILABLE_IN_ALL
void            ide_session_prepend          (IdeSession      *self,
                                              IdeSessionItem  *item);
IDE_AVAILABLE_IN_ALL
void            ide_session_insert           (IdeSession      *self,
                                              guint            position,
                                              IdeSessionItem  *item);
IDE_AVAILABLE_IN_ALL
void            ide_session_remove           (IdeSession      *self,
                                              IdeSessionItem  *item);
IDE_AVAILABLE_IN_ALL
void            ide_session_remove_at        (IdeSession      *self,
                                              guint            position);
IDE_AVAILABLE_IN_ALL
guint           ide_session_get_n_items      (IdeSession      *self);
IDE_AVAILABLE_IN_ALL
IdeSessionItem *ide_session_get_item         (IdeSession      *self,
                                              guint            position);
IDE_AVAILABLE_IN_ALL
IdeSession     *ide_session_new_from_variant (GVariant        *variant,
                                              GError         **error);
IDE_AVAILABLE_IN_ALL
GVariant       *ide_session_to_variant       (IdeSession      *self);
IDE_AVAILABLE_IN_44
IdeSessionItem *ide_session_lookup_by_id     (IdeSession      *self,
                                              const char      *id);

G_END_DECLS
