/* ide-diagnostic.h
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

#ifndef IDE_DIAGNOSTIC_H
#define IDE_DIAGNOSTIC_H

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTIC (ide_diagnostic_get_type())

typedef enum
{
  IDE_DIAGNOSTIC_IGNORED = 0,
  IDE_DIAGNOSTIC_NOTE,
  IDE_DIAGNOSTIC_WARNING,
  IDE_DIAGNOSTIC_ERROR,
  IDE_DIAGNOSTIC_FATAL,
} IdeDiagnosticSeverity;

GType                  ide_diagnostic_get_type     (void);
IdeDiagnostic         *ide_diagnostic_ref          (IdeDiagnostic *self);
void                   ide_diagnostic_unref        (IdeDiagnostic *self);
IdeDiagnosticSeverity  ide_diagnostic_get_severity (IdeDiagnostic *self);
const gchar           *ide_diagnostic_get_text     (IdeDiagnostic *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeDiagnostic, ide_diagnostic_unref)

G_END_DECLS

#endif /* IDE_DIAGNOSTIC_H */
