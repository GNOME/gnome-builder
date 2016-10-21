/* ide-langserv-diagnostic-provider.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_LANGSERV_DIAGNOSTIC_PROVIDER_H
#define IDE_LANGSERV_DIAGNOSTIC_PROVIDER_H

#include "ide-object.h"

#include "diagnostics/ide-diagnostic-provider.h"
#include "langserv/ide-langserv-client.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_DIAGNOSTIC_PROVIDER (ide_langserv_diagnostic_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeLangservDiagnosticProvider, ide_langserv_diagnostic_provider, IDE, LANGSERV_DIAGNOSTIC_PROVIDER, IdeObject)

struct _IdeLangservDiagnosticProviderClass
{
  IdeObjectClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

IdeLangservClient *ide_langserv_diagnostic_provider_get_client (IdeLangservDiagnosticProvider *self);
void               ide_langserv_diagnostic_provider_set_client (IdeLangservDiagnosticProvider *self,
                                                                IdeLangservClient             *client);

G_END_DECLS

#endif /* IDE_LANGSERV_DIAGNOSTIC_PROVIDER_H */
