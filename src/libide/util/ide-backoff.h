/* ide-backoff.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <glib.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

typedef struct
{
  guint min_delay;
  guint max_delay;
  guint cur_delay;
  guint n_failures;
} IdeBackoff;

IDE_AVAILABLE_IN_3_32
void ide_backoff_init      (IdeBackoff *self,
                            guint       min_delay,
                            guint       max_delay);
IDE_AVAILABLE_IN_3_32
void ide_backoff_failed    (IdeBackoff *self,
                            guint      *next_delay);
IDE_AVAILABLE_IN_3_32
void ide_backoff_succeeded (IdeBackoff *self);

G_END_DECLS
