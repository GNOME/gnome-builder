/* gb-devhelp-document.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_DEVHELP_DOCUMENT_H
#define GB_DEVHELP_DOCUMENT_H

#include "gb-document.h"

G_BEGIN_DECLS

#define GB_TYPE_DEVHELP_DOCUMENT (gb_devhelp_document_get_type())

G_DECLARE_FINAL_TYPE (GbDevhelpDocument, gb_devhelp_document, GB, DEVHELP_DOCUMENT, GObject)

GbDevhelpDocument *gb_devhelp_document_new        (void);
void               gb_devhelp_document_set_search (GbDevhelpDocument *document,
                                                   const gchar       *search);
const gchar       *gb_devhelp_document_get_uri    (GbDevhelpDocument *document);
void               gb_devhelp_document_set_uri    (GbDevhelpDocument *document,
                                                   const gchar       *uri);

G_END_DECLS

#endif /* GB_DEVHELP_DOCUMENT_H */
