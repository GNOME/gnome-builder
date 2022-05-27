/* ide-diagnostics.h
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

#include "ide-code-types.h"
#include "ide-diagnostic.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTICS (ide_diagnostics_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDiagnostics, ide_diagnostics, IDE, DIAGNOSTICS, IdeObject)

/**
 * IdeDiagnosticsLineCallback:
 * @line: the line number, starting from 0
 * @severity: the severity of the diagnostic
 * @user_data: user data provided with callback
 *
 * This function prototype is used to notify a caller of every line that has a
 * diagnostic, and the most severe #IdeDiagnosticSeverity for that line.
 */
typedef void (*IdeDiagnosticsLineCallback) (guint                 line,
                                            IdeDiagnosticSeverity severity,
                                            gpointer              user_data);

struct _IdeDiagnosticsClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
IdeDiagnostics *ide_diagnostics_new                     (void);
IDE_AVAILABLE_IN_ALL
IdeDiagnostics *ide_diagnostics_new_from_array          (GPtrArray                  *array);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostics_add                     (IdeDiagnostics             *self,
                                                         IdeDiagnostic              *diagnostic);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostics_take                    (IdeDiagnostics             *self,
                                                         IdeDiagnostic              *diagnostic);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostics_merge                   (IdeDiagnostics             *self,
                                                         IdeDiagnostics             *other);
IDE_AVAILABLE_IN_ALL
guint           ide_diagnostics_get_n_errors            (IdeDiagnostics             *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_diagnostics_get_has_errors          (IdeDiagnostics             *self);
IDE_AVAILABLE_IN_ALL
guint           ide_diagnostics_get_n_warnings          (IdeDiagnostics             *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_diagnostics_get_has_warnings        (IdeDiagnostics             *self);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostics_foreach_line_in_range   (IdeDiagnostics             *self,
                                                         GFile                      *file,
                                                         guint                       begin_line,
                                                         guint                       end_line,
                                                         IdeDiagnosticsLineCallback  callback,
                                                         gpointer                    user_data);
IDE_AVAILABLE_IN_ALL
IdeDiagnostic  *ide_diagnostics_get_diagnostic_at_line  (IdeDiagnostics             *self,
                                                         GFile                      *file,
                                                         guint                       line);
IDE_AVAILABLE_IN_ALL
GPtrArray      *ide_diagnostics_get_diagnostics_at_line (IdeDiagnostics             *self,
                                                         GFile                      *file,
                                                         guint                       line);

#define ide_diagnostics_get_size(d) ((gsize)g_list_model_get_n_items(G_LIST_MODEL(d)))

G_END_DECLS
