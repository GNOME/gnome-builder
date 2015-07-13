/* ide-diagnostician.h
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

#ifndef IDE_DIAGNOSTICIAN_H
#define IDE_DIAGNOSTICIAN_H

#include <gtksourceview/gtksource.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTICIAN (ide_diagnostician_get_type())

G_DECLARE_FINAL_TYPE (IdeDiagnostician, ide_diagnostician, IDE, DIAGNOSTICIAN, IdeObject)

GtkSourceLanguage *ide_diagnostician_get_language    (IdeDiagnostician     *self);
void               ide_diagnostician_set_language    (IdeDiagnostician     *self,
                                                      GtkSourceLanguage    *language);
void               ide_diagnostician_diagnose_async  (IdeDiagnostician     *diagnostician,
                                                      IdeFile              *file,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
IdeDiagnostics    *ide_diagnostician_diagnose_finish (IdeDiagnostician     *diagnostician,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

#endif /* IDE_DIAGNOSTICIAN_H */
