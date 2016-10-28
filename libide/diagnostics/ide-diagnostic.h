/* ide-diagnostic.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_DIAGNOSTIC_H
#define IDE_DIAGNOSTIC_H

#include <gio/gio.h>

#include "ide-fixit.h"
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

IdeSourceLocation     *ide_diagnostic_get_location         (IdeDiagnostic         *self);
GFile                 *ide_diagnostic_get_file             (IdeDiagnostic         *self);
guint                  ide_diagnostic_get_num_fixits       (IdeDiagnostic         *self);
IdeFixit              *ide_diagnostic_get_fixit            (IdeDiagnostic         *self,
                                                            guint                  index);
guint                  ide_diagnostic_get_num_ranges       (IdeDiagnostic         *self);
IdeSourceRange        *ide_diagnostic_get_range            (IdeDiagnostic         *self,
                                                            guint                  index);
IdeDiagnosticSeverity  ide_diagnostic_get_severity         (IdeDiagnostic         *self);
const gchar           *ide_diagnostic_get_text             (IdeDiagnostic         *self);
gchar                 *ide_diagnostic_get_text_for_display (IdeDiagnostic         *self);
GType                  ide_diagnostic_get_type             (void);
IdeDiagnostic         *ide_diagnostic_ref                  (IdeDiagnostic         *self);
void                   ide_diagnostic_unref                (IdeDiagnostic         *self);
IdeDiagnostic         *ide_diagnostic_new                  (IdeDiagnosticSeverity  severity,
                                                            const gchar           *text,
                                                            IdeSourceLocation     *location);
void                   ide_diagnostic_add_range            (IdeDiagnostic         *self,
                                                            IdeSourceRange        *range);
void                   ide_diagnostic_take_fixit           (IdeDiagnostic         *self,
                                                            IdeFixit              *fixit);
void                   ide_diagnostic_take_range           (IdeDiagnostic         *self,
                                                            IdeSourceRange        *range);
gint                   ide_diagnostic_compare              (const IdeDiagnostic   *a,
                                                            const IdeDiagnostic   *b);
guint                  ide_diagnostic_hash                 (IdeDiagnostic         *self);


const gchar           *ide_diagnostic_severity_to_string   (IdeDiagnosticSeverity severity);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeDiagnostic, ide_diagnostic_unref)

G_END_DECLS

#endif /* IDE_DIAGNOSTIC_H */
