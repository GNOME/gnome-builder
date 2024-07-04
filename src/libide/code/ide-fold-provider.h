/*
 * ide-fold-provider.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>

#include "ide-buffer.h"
#include "ide-fold-regions.h"

G_BEGIN_DECLS

#define IDE_TYPE_FOLD_PROVIDER (ide_fold_provider_get_type())

IDE_AVAILABLE_IN_47
G_DECLARE_DERIVABLE_TYPE (IdeFoldProvider, ide_fold_provider, IDE, FOLD_PROVIDER, IdeObject)

struct _IdeFoldProviderClass
{
  IdeObjectClass parent_class;

  void            (*list_regions_async)  (IdeFoldProvider      *self,
                                          IdeBuffer            *buffer,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
  IdeFoldRegions *(*list_regions_finish) (IdeFoldProvider      *self,
                                          GAsyncResult         *result,
                                          GError              **error);

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_47
void            ide_fold_provider_list_regions_async  (IdeFoldProvider      *self,
                                                       IdeBuffer            *buffer,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IDE_AVAILABLE_IN_47
IdeFoldRegions *ide_fold_provider_list_regions_finish (IdeFoldProvider      *self,
                                                       GAsyncResult         *result,
                                                       GError              **error);

G_END_DECLS
