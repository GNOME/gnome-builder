/* ide-docs-provider.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-docs-item.h"
#include "ide-docs-query.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCS_PROVIDER (ide_docs_provider_get_type ())

IDE_AVAILABLE_IN_3_34
G_DECLARE_INTERFACE (IdeDocsProvider, ide_docs_provider, IDE, DOCS_PROVIDER, GObject)

struct _IdeDocsProviderInterface
{
  GTypeInterface parent;

  void        (*populate_async)  (IdeDocsProvider      *self,
                                  IdeDocsItem          *parent,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data);
  gboolean    (*populate_finish) (IdeDocsProvider      *self,
                                  GAsyncResult         *result,
                                  GError              **error);
  void        (*search_async)    (IdeDocsProvider      *self,
                                  IdeDocsQuery         *query,
                                  IdeDocsItem          *results,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data);
  gboolean    (*search_finish)   (IdeDocsProvider      *self,
                                  GAsyncResult         *result,
                                  GError              **error);
};

IDE_AVAILABLE_IN_3_34
void        ide_docs_provider_populate_async  (IdeDocsProvider      *self,
                                               IdeDocsItem          *item,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
IDE_AVAILABLE_IN_3_34
gboolean    ide_docs_provider_populate_finish (IdeDocsProvider      *self,
                                               GAsyncResult         *result,
                                               GError              **error);
IDE_AVAILABLE_IN_3_34
void        ide_docs_provider_search_async    (IdeDocsProvider      *self,
                                               IdeDocsQuery         *query,
                                               IdeDocsItem          *results,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
IDE_AVAILABLE_IN_3_34
gboolean    ide_docs_provider_search_finish   (IdeDocsProvider      *self,
                                               GAsyncResult         *result,
                                               GError              **error);

G_END_DECLS
