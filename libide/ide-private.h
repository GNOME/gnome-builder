/* ide-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_PRIVATE_H
#define IDE_PRIVATE_H

#include <clang-c/Index.h>

#include "ide-clang-translation-unit.h"
#include "ide-types.h"

G_BEGIN_DECLS

IdeUnsavedFile *_ide_unsaved_file_new (GFile  *file,
                                       GBytes *content,
                                       gint64  sequence);

IdeClangTranslationUnit *_ide_clang_translation_unit_new (IdeContext        *contxt,
                                                          CXTranslationUnit  tu,
                                                          gint64             sequence);

void _ide_diagnostician_add_provider    (IdeDiagnostician      *self,
                                         IdeDiagnosticProvider *provider);
void _ide_diagnostician_remove_provider (IdeDiagnostician      *self,
                                         IdeDiagnosticProvider *provider);

IdeDiagnostics *_ide_diagnostics_new (GPtrArray *ar);


G_END_DECLS

#endif /* IDE_PRIVATE_H */
