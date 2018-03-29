/* ide-triplet.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#pragma once

#include <glib-object.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_TRIPLET (ide_triplet_get_type())

typedef struct _IdeTriplet IdeTriplet;

IDE_AVAILABLE_IN_3_30
GType         ide_triplet_get_type             (void);
IDE_AVAILABLE_IN_3_30
IdeTriplet   *ide_triplet_new                  (const gchar  *full_name);
IDE_AVAILABLE_IN_3_30
IdeTriplet   *ide_triplet_new_from_system      (void);
IDE_AVAILABLE_IN_3_30
IdeTriplet   *ide_triplet_new_with_triplet     (const gchar  *cpu,
                                                const gchar  *kernel,
                                                const gchar  *operating_system);
IDE_AVAILABLE_IN_3_30
IdeTriplet   *ide_triplet_new_with_quadruplet  (const gchar  *cpu,
                                                const gchar  *vendor,
                                                const gchar  *kernel,
                                                const gchar  *operating_system);
IDE_AVAILABLE_IN_3_30
IdeTriplet   *ide_triplet_ref                  (IdeTriplet   *self);
IDE_AVAILABLE_IN_3_30
void          ide_triplet_unref                (IdeTriplet   *self);
IDE_AVAILABLE_IN_3_30
const gchar  *ide_triplet_get_full_name        (IdeTriplet   *self);
IDE_AVAILABLE_IN_3_30
const gchar  *ide_triplet_get_cpu              (IdeTriplet   *self);
IDE_AVAILABLE_IN_3_30
const gchar  *ide_triplet_get_vendor           (IdeTriplet   *self);
IDE_AVAILABLE_IN_3_30
const gchar  *ide_triplet_get_kernel           (IdeTriplet   *self);
IDE_AVAILABLE_IN_3_30
const gchar  *ide_triplet_get_operating_system (IdeTriplet   *self);
IDE_AVAILABLE_IN_3_30
gboolean      ide_triplet_is_system            (IdeTriplet   *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeTriplet, ide_triplet_unref)

G_END_DECLS
