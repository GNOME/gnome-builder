/* ide-pausable.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_PAUSABLE (ide_pausable_get_type())

G_DECLARE_FINAL_TYPE (IdePausable, ide_pausable, IDE, PAUSABLE, GObject)

IDE_AVAILABLE_IN_ALL
IdePausable *ide_pausable_new          (void);
IDE_AVAILABLE_IN_ALL
gboolean     ide_pausable_get_paused   (IdePausable *self);
IDE_AVAILABLE_IN_ALL
void         ide_pausable_set_paused   (IdePausable *self,
                                        gboolean     paused);
IDE_AVAILABLE_IN_ALL
void         ide_pausable_pause        (IdePausable *self);
IDE_AVAILABLE_IN_ALL
void         ide_pausable_unpause      (IdePausable *self);
IDE_AVAILABLE_IN_ALL
const gchar *ide_pausable_get_title    (IdePausable *self);
IDE_AVAILABLE_IN_ALL
void         ide_pausable_set_title    (IdePausable *self,
                                        const gchar *title);
IDE_AVAILABLE_IN_ALL
const gchar *ide_pausable_get_subtitle (IdePausable *self);
IDE_AVAILABLE_IN_ALL
void         ide_pausable_set_subtitle (IdePausable *self,
                                        const gchar *subtitle);

G_END_DECLS
