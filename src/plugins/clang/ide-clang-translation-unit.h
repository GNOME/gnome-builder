/* ide-clang-translation-unit.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <gtk/gtk.h>
#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_TRANSLATION_UNIT (ide_clang_translation_unit_get_type())

G_DECLARE_FINAL_TYPE (IdeClangTranslationUnit, ide_clang_translation_unit, IDE, CLANG_TRANSLATION_UNIT, IdeObject)

gint64             ide_clang_translation_unit_get_serial               (IdeClangTranslationUnit  *self);
IdeDiagnostics    *ide_clang_translation_unit_get_diagnostics          (IdeClangTranslationUnit  *self);
IdeDiagnostics    *ide_clang_translation_unit_get_diagnostics_for_file (IdeClangTranslationUnit  *self,
                                                                        GFile                    *file);
void               ide_clang_translation_unit_code_complete_async      (IdeClangTranslationUnit  *self,
                                                                        GFile                    *file,
                                                                        const GtkTextIter        *location,
                                                                        GCancellable             *cancellable,
                                                                        GAsyncReadyCallback       callback,
                                                                        gpointer                  user_data);
GPtrArray         *ide_clang_translation_unit_code_complete_finish     (IdeClangTranslationUnit  *self,
                                                                        GAsyncResult             *result,
                                                                        GError                  **error);
void               ide_clang_translation_unit_get_symbol_tree_async    (IdeClangTranslationUnit  *self,
                                                                        GFile                    *file,
                                                                        GCancellable             *cancellable,
                                                                        GAsyncReadyCallback       callback,
                                                                        gpointer                  user_data);
IdeSymbolTree     *ide_clang_translation_unit_get_symbol_tree_finish   (IdeClangTranslationUnit  *self,
                                                                        GAsyncResult             *result,
                                                                        GError                  **error);
IdeHighlightIndex *ide_clang_translation_unit_get_index                (IdeClangTranslationUnit  *self);
IdeSymbol         *ide_clang_translation_unit_lookup_symbol            (IdeClangTranslationUnit  *self,
                                                                        IdeSourceLocation        *location,
                                                                        GError                  **error);
GPtrArray         *ide_clang_translation_unit_get_symbols              (IdeClangTranslationUnit  *self,
                                                                        IdeFile                  *file);
IdeSymbol         *ide_clang_translation_unit_find_nearest_scope       (IdeClangTranslationUnit  *self,
                                                                        IdeSourceLocation        *location,
                                                                        GError                  **error);
gchar            *ide_clang_translation_unit_generate_key              (IdeClangTranslationUnit  *self,
                                                                        IdeSourceLocation        *location);
G_END_DECLS
