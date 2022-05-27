/* ide-lsp-diagnostic.h
 *
 * Copyright 2022 JCWasmx86 <JCWasmx86@t-online.de>
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

#if !defined (IDE_LSP_INSIDE) && !defined (IDE_LSP_COMPILATION)
# error "Only <libide-lsp.h> can be included directly."
#endif

#include <libide-code.h>

G_BEGIN_DECLS

#define IDE_TYPE_LSP_DIAGNOSTIC (ide_lsp_diagnostic_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeLspDiagnostic, ide_lsp_diagnostic, IDE, LSP_DIAGNOSTIC, IdeDiagnostic)

struct _IdeLspDiagnosticClass
{
  IdeDiagnosticClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
IdeLspDiagnostic *ide_lsp_diagnostic_new     (IdeDiagnosticSeverity  severity,
                                              const gchar           *message,
                                              IdeLocation           *location,
                                              GVariant              *raw_value);
IDE_AVAILABLE_IN_ALL
GVariant         *ide_lsp_diagnostic_dup_raw (IdeLspDiagnostic      *self);

G_END_DECLS
