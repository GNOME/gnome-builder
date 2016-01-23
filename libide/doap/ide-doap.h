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

#ifndef IDE_DOAP_H
#define IDE_DOAP_H

#include <gio/gio.h>

#include "ide-doap-person.h"

G_BEGIN_DECLS

#define IDE_DOAP_ERROR (ide_doap_error_quark())
#define IDE_TYPE_DOAP  (ide_doap_get_type())

G_DECLARE_FINAL_TYPE (IdeDoap, ide_doap, IDE, DOAP, GObject)

typedef enum
{
  IDE_DOAP_ERROR_INVALID_FORMAT = 1,
} IdeDoapError;

IdeDoap       *ide_doap_new               (void);
GQuark         ide_doap_error_quark       (void);
gboolean       ide_doap_load_from_file    (IdeDoap        *self,
                                           GFile          *file,
                                           GCancellable   *cancellable,
                                           GError        **error);
gboolean       ide_doap_load_from_data    (IdeDoap        *self,
                                           const gchar    *data,
                                           gsize           length,
                                           GError        **error);
const gchar   *ide_doap_get_name          (IdeDoap        *self);
const gchar   *ide_doap_get_shortdesc     (IdeDoap        *self);
const gchar   *ide_doap_get_description   (IdeDoap        *self);
const gchar   *ide_doap_get_bug_database  (IdeDoap        *self);
const gchar   *ide_doap_get_download_page (IdeDoap        *self);
const gchar   *ide_doap_get_homepage      (IdeDoap        *self);
const gchar   *ide_doap_get_category      (IdeDoap        *self);
gchar        **ide_doap_get_languages     (IdeDoap        *self);
GList         *ide_doap_get_maintainers   (IdeDoap        *self);

G_END_DECLS

#endif /* IDE_DOAP_H */
