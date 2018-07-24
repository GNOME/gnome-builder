/* ide-session.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "ide-object.h"

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_SESSION (ide_session_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSession, ide_session, IDE, SESSION, IdeObject)

IDE_AVAILABLE_IN_ALL
void     ide_session_restore_async  (IdeSession           *self,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_session_restore_finish (IdeSession           *self,
                                     GAsyncResult         *result,
                                     GError              **error);
IDE_AVAILABLE_IN_ALL
void     ide_session_save_async     (IdeSession           *self,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_session_save_finish    (IdeSession           *self,
                                     GAsyncResult         *result,
                                     GError              **error);

G_END_DECLS
