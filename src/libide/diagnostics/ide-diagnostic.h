/* ide-diagnostic.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include "diagnostics/ide-fixit.h"
#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTIC (ide_diagnostic_get_type())

typedef enum
{
  IDE_DIAGNOSTIC_IGNORED    = 0,
  IDE_DIAGNOSTIC_NOTE       = 1,
  IDE_DIAGNOSTIC_DEPRECATED = 2,
  IDE_DIAGNOSTIC_WARNING    = 3,
  IDE_DIAGNOSTIC_ERROR      = 4,
  IDE_DIAGNOSTIC_FATAL      = 5,
} IdeDiagnosticSeverity;

IDE_AVAILABLE_IN_ALL
IdeSourceLocation     *ide_diagnostic_get_location         (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
GFile                 *ide_diagnostic_get_file             (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
guint                  ide_diagnostic_get_num_fixits       (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
IdeFixit              *ide_diagnostic_get_fixit            (IdeDiagnostic         *self,
                                                            guint                  index);
IDE_AVAILABLE_IN_ALL
guint                  ide_diagnostic_get_num_ranges       (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
IdeSourceRange        *ide_diagnostic_get_range            (IdeDiagnostic         *self,
                                                            guint                  index);
IDE_AVAILABLE_IN_ALL
IdeDiagnosticSeverity  ide_diagnostic_get_severity         (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_diagnostic_get_text             (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
gchar                 *ide_diagnostic_get_text_for_display (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
GType                  ide_diagnostic_get_type             (void);
IDE_AVAILABLE_IN_ALL
IdeDiagnostic         *ide_diagnostic_ref                  (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_unref                (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
IdeDiagnostic         *ide_diagnostic_new                  (IdeDiagnosticSeverity  severity,
                                                            const gchar           *text,
                                                            IdeSourceLocation     *location);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_add_range            (IdeDiagnostic         *self,
                                                            IdeSourceRange        *range);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_take_fixit           (IdeDiagnostic         *self,
                                                            IdeFixit              *fixit);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_take_range           (IdeDiagnostic         *self,
                                                            IdeSourceRange        *range);
IDE_AVAILABLE_IN_ALL
gint                   ide_diagnostic_compare              (const IdeDiagnostic   *a,
                                                            const IdeDiagnostic   *b);
IDE_AVAILABLE_IN_ALL
guint                  ide_diagnostic_hash                 (IdeDiagnostic         *self);


IDE_AVAILABLE_IN_ALL
const gchar           *ide_diagnostic_severity_to_string   (IdeDiagnosticSeverity severity);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeDiagnostic, ide_diagnostic_unref)

G_END_DECLS
