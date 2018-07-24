/* ide-session-addin.h
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

#define IDE_TYPE_SESSION_ADDIN (ide_session_addin_get_type ())

IDE_AVAILABLE_IN_3_30
G_DECLARE_INTERFACE (IdeSessionAddin, ide_session_addin, IDE, SESSION_ADDIN, IdeObject)

struct _IdeSessionAddinInterface
{
  GTypeInterface parent;

  void      (*save_async)     (IdeSessionAddin      *self,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data);
  GVariant *(*save_finish)    (IdeSessionAddin      *self,
                               GAsyncResult         *result,
                               GError              **error);
  void      (*restore_async)  (IdeSessionAddin      *self,
                               GVariant             *state,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data);
  gboolean  (*restore_finish) (IdeSessionAddin      *self,
                               GAsyncResult         *result,
                               GError              **error);
};

IDE_AVAILABLE_IN_3_30
void      ide_session_addin_save_async     (IdeSessionAddin      *self,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_3_30
GVariant *ide_session_addin_save_finish    (IdeSessionAddin      *self,
                                            GAsyncResult         *result,
                                            GError              **error);
IDE_AVAILABLE_IN_3_30
void      ide_session_addin_restore_async  (IdeSessionAddin      *self,
                                            GVariant             *state,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data);
IDE_AVAILABLE_IN_3_30
gboolean  ide_session_addin_restore_finish (IdeSessionAddin      *self,
                                            GAsyncResult         *result,
                                            GError              **error);

G_END_DECLS
