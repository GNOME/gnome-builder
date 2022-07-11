/* ide-doap-person.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#if !defined (IDE_PROJECTS_INSIDE) && !defined (IDE_PROJECTS_COMPILATION)
# error "Only <libide-projects.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_DOAP_PERSON (ide_doap_person_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeDoapPerson, ide_doap_person, IDE, DOAP_PERSON, GObject)

IDE_AVAILABLE_IN_ALL
IdeDoapPerson *ide_doap_person_new       (void);
IDE_AVAILABLE_IN_ALL
const gchar   *ide_doap_person_get_name  (IdeDoapPerson *self);
IDE_AVAILABLE_IN_ALL
void           ide_doap_person_set_name  (IdeDoapPerson *self,
                                          const gchar   *name);
IDE_AVAILABLE_IN_ALL
const gchar   *ide_doap_person_get_email (IdeDoapPerson *self);
IDE_AVAILABLE_IN_ALL
void           ide_doap_person_set_email (IdeDoapPerson *self,
                                          const gchar   *email);

G_END_DECLS
