/* ide-doap.h
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

#ifndef DOAP_DOCUMENT_H
#define DOAP_DOCUMENT_H

#include <gio/gio.h>

#include "doap-person.h"

G_BEGIN_DECLS

#define DOAP_DOCUMENT_ERROR (doap_document_error_quark())
#define DOAP_TYPE_DOCUMENT  (doap_document_get_type())

G_DECLARE_FINAL_TYPE (DoapDocument, doap_document, DOAP, DOCUMENT, GObject)

typedef enum
{
  DOAP_DOCUMENT_ERROR_INVALID_FORMAT = 1,
} DoapDocumentError;

DoapDocument  *doap_document_new               (void);
GQuark         doap_document_error_quark       (void);
gboolean       doap_document_load_from_file    (DoapDocument        *self,
                                                GFile               *file,
                                                GCancellable        *cancellable,
                                                GError             **error);
const gchar   *doap_document_get_name          (DoapDocument        *self);
const gchar   *doap_document_get_shortdesc     (DoapDocument        *self);
const gchar   *doap_document_get_description   (DoapDocument        *self);
const gchar   *doap_document_get_bug_database  (DoapDocument        *self);
const gchar   *doap_document_get_download_page (DoapDocument        *self);
const gchar   *doap_document_get_homepage      (DoapDocument        *self);
const gchar   *doap_document_get_category      (DoapDocument        *self);
gchar        **doap_document_get_languages     (DoapDocument        *self);
GList         *doap_document_get_maintainers   (DoapDocument        *self);

G_END_DECLS

#endif /* DOAP_DOCUMENT_H */
