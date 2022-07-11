/* ide-diagnostic.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>
#include <libide-io.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTIC (ide_diagnostic_get_type())

typedef enum
{
  IDE_DIAGNOSTIC_IGNORED    = 0,
  IDE_DIAGNOSTIC_NOTE       = 1,
  IDE_DIAGNOSTIC_UNUSED     = 2,
  IDE_DIAGNOSTIC_DEPRECATED = 3,
  IDE_DIAGNOSTIC_WARNING    = 4,
  IDE_DIAGNOSTIC_ERROR      = 5,
  IDE_DIAGNOSTIC_FATAL      = 6,
} IdeDiagnosticSeverity;

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDiagnostic, ide_diagnostic, IDE, DIAGNOSTIC, IdeObject)

struct _IdeDiagnosticClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
IdeDiagnostic         *ide_diagnostic_new                  (IdeDiagnosticSeverity  severity,
                                                            const gchar           *message,
                                                            IdeLocation           *location);
IDE_AVAILABLE_IN_ALL
IdeDiagnostic         *ide_diagnostic_new_from_variant     (GVariant              *variant);
IDE_AVAILABLE_IN_ALL
guint                  ide_diagnostic_hash                 (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
gboolean               ide_diagnostic_equal                (IdeDiagnostic         *a,
                                                            IdeDiagnostic         *b);
IDE_AVAILABLE_IN_ALL
IdeLocation           *ide_diagnostic_get_location         (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_diagnostic_get_text             (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
IdeMarkedKind          ide_diagnostic_get_marked_kind      (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_set_marked_kind      (IdeDiagnostic         *self,
                                                            IdeMarkedKind          marked_kind);
IDE_AVAILABLE_IN_ALL
IdeDiagnosticSeverity  ide_diagnostic_get_severity         (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
GFile                 *ide_diagnostic_get_file             (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
gchar                 *ide_diagnostic_get_text_for_display (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
const gchar           *ide_diagnostic_severity_to_string   (IdeDiagnosticSeverity  severity);
IDE_AVAILABLE_IN_ALL
guint                  ide_diagnostic_get_n_ranges         (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
IdeRange              *ide_diagnostic_get_range            (IdeDiagnostic         *self,
                                                            guint                  index);
IDE_AVAILABLE_IN_ALL
guint                  ide_diagnostic_get_n_fixits         (IdeDiagnostic         *self);
IDE_AVAILABLE_IN_ALL
IdeTextEdit           *ide_diagnostic_get_fixit            (IdeDiagnostic         *self,
                                                            guint                  index);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_add_range            (IdeDiagnostic         *self,
                                                            IdeRange              *range);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_take_range           (IdeDiagnostic         *self,
                                                            IdeRange              *range);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_add_fixit            (IdeDiagnostic         *self,
                                                            IdeTextEdit           *fixit);
IDE_AVAILABLE_IN_ALL
void                   ide_diagnostic_take_fixit           (IdeDiagnostic         *self,
                                                            IdeTextEdit           *fixit);
IDE_AVAILABLE_IN_ALL
gboolean               ide_diagnostic_compare              (IdeDiagnostic         *a,
                                                            IdeDiagnostic         *b);
IDE_AVAILABLE_IN_ALL
GVariant              *ide_diagnostic_to_variant           (IdeDiagnostic         *self);

G_END_DECLS
