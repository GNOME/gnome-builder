/* ide-docs-library.h
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

#define IDE_TYPE_DOCS_LIBRARY (ide_docs_library_get_type())

IDE_AVAILABLE_IN_3_34
G_DECLARE_FINAL_TYPE (IdeDocsLibrary, ide_docs_library, IDE, DOCS_LIBRARY, IdeObject)

IDE_AVAILABLE_IN_3_34
IdeDocsLibrary *ide_docs_library_from_context  (IdeContext           *context);
IDE_AVAILABLE_IN_3_34
void            ide_docs_library_search_async  (IdeDocsLibrary       *self,
                                                IdeDocsQuery         *query,
                                                IdeDocsItem          *results,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
IDE_AVAILABLE_IN_3_34
gboolean        ide_docs_library_search_finish (IdeDocsLibrary       *self,
                                                GAsyncResult         *result,
                                                GError              **error);

G_END_DECLS
