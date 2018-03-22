/* ide-vcs-initializer.h
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_VCS_INITIALIZER (ide_vcs_initializer_get_type ())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeVcsInitializer, ide_vcs_initializer, IDE, VCS_INITIALIZER, GObject)

struct _IdeVcsInitializerInterface
{
  GTypeInterface parent;

  gchar   *(*get_title)         (IdeVcsInitializer    *self);
  void     (*initialize_async)  (IdeVcsInitializer    *self,
                                 GFile                *file,
                                 GCancellable         *cancellable,
                                 GAsyncReadyCallback   callback,
                                 gpointer              user_data);
  gboolean (*initialize_finish) (IdeVcsInitializer    *self,
                                 GAsyncResult         *result,
                                 GError              **error);
};

IDE_AVAILABLE_IN_ALL
gchar   *ide_vcs_initializer_get_title         (IdeVcsInitializer    *self);
IDE_AVAILABLE_IN_ALL
void     ide_vcs_initializer_initialize_async  (IdeVcsInitializer    *self,
                                                GFile                *file,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean ide_vcs_initializer_initialize_finish (IdeVcsInitializer    *self,
                                                GAsyncResult         *result,
                                                GError              **error);

G_END_DECLS
