/* ide-context-addin.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-context.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_CONTEXT_ADDIN (ide_context_addin_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeContextAddin, ide_context_addin, IDE, CONTEXT_ADDIN, GObject)

struct _IdeContextAddinInterface
{
  GTypeInterface parent_iface;

  void     (*load)                (IdeContextAddin      *self,
                                   IdeContext           *context);
  void     (*unload)              (IdeContextAddin      *self,
                                   IdeContext           *context);
  void     (*load_project_async)  (IdeContextAddin      *self,
                                   IdeContext           *context,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data);
  gboolean (*load_project_finish) (IdeContextAddin      *self,
                                   GAsyncResult         *result,
                                   GError              **error);
  void     (*project_loaded)      (IdeContextAddin      *self,
                                   IdeContext           *context);
};

IDE_AVAILABLE_IN_3_32
void     ide_context_addin_load_project_async  (IdeContextAddin      *self,
                                                IdeContext           *context,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean ide_context_addin_load_project_finish (IdeContextAddin      *self,
                                                GAsyncResult         *result,
                                                GError              **error);
IDE_AVAILABLE_IN_3_32
void     ide_context_addin_load                (IdeContextAddin      *self,
                                                IdeContext           *context);
IDE_AVAILABLE_IN_3_32
void     ide_context_addin_unload              (IdeContextAddin      *self,
                                                IdeContext           *context);
IDE_AVAILABLE_IN_3_32
void     ide_context_addin_project_loaded      (IdeContextAddin      *self,
                                                IdeContext           *context);

G_END_DECLS
