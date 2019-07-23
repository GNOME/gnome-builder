/* ide-docs-pane.h
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

#include <libide-gui.h>

#include "ide-docs-library.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCS_PANE (ide_docs_pane_get_type())

G_DECLARE_FINAL_TYPE (IdeDocsPane, ide_docs_pane, IDE, DOCS_PANE, IdePane)

IdeDocsLibrary *ide_docs_pane_get_library (IdeDocsPane    *self);
void            ide_docs_pane_set_library (IdeDocsPane    *self,
                                           IdeDocsLibrary *library);

G_END_DECLS
