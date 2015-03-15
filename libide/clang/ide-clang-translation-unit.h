/* ide-clang-translation-unit.h
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

#ifndef IDE_CLANG_TRANSLATION_UNIT_H
#define IDE_CLANG_TRANSLATION_UNIT_H

#include "ide-object.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_TRANSLATION_UNIT (ide_clang_translation_unit_get_type())

G_DECLARE_FINAL_TYPE (IdeClangTranslationUnit, ide_clang_translation_unit, IDE, CLANG_TRANSLATION_UNIT, IdeObject)

gint64          ide_clang_translation_unit_get_sequence         (IdeClangTranslationUnit  *self);
IdeDiagnostics *ide_clang_translation_unit_get_diagnostics      (IdeClangTranslationUnit  *self);
void            ide_clang_translation_unit_code_complete_async  (IdeClangTranslationUnit  *self,
                                                                 GFile                    *file,
                                                                 const GtkTextIter        *location,
                                                                 GCancellable             *cancellable,
                                                                 GAsyncReadyCallback       callback,
                                                                 gpointer                  user_data);
GPtrArray      *ide_clang_translation_unit_code_complete_finish (IdeClangTranslationUnit  *self,
                                                                 GAsyncResult             *result,
                                                                 GError                  **error);

G_END_DECLS

#endif /* IDE_CLANG_TRANSLATION_UNIT_H */
