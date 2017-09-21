/* ide-doap-person.h
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

#ifndef IDE_DOAP_PERSON_H
#define IDE_DOAP_PERSON_H

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_DOAP_PERSON (ide_doap_person_get_type())

G_DECLARE_FINAL_TYPE (IdeDoapPerson, ide_doap_person, IDE, DOAP_PERSON, GObject)

IdeDoapPerson *ide_doap_person_new       (void);
const gchar   *ide_doap_person_get_name  (IdeDoapPerson *self);
void           ide_doap_person_set_name  (IdeDoapPerson *self,
                                          const gchar   *name);
const gchar   *ide_doap_person_get_email (IdeDoapPerson *self);
void           ide_doap_person_set_email (IdeDoapPerson *self,
                                          const gchar   *email);

G_END_DECLS

#endif /* IDE_DOAP_PERSON_H */
