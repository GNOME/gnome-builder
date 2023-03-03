/* ide-pair-private.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _IdePair
{
  gpointer a;
  gpointer b;
} IdePair;

static inline IdePair *
ide_pair_new (gpointer instance_a,
              gpointer instance_b)
{
  IdePair *pair = g_atomic_rc_box_new0 (IdePair);
  g_set_object (&pair->a, instance_a);
  g_set_object (&pair->b, instance_b);
  return pair;
}

static inline IdePair *
ide_pair_ref (IdePair *pair)
{
  return g_atomic_rc_box_acquire (pair);
}

static inline void
_ide_pair_finalize (gpointer data)
{
  IdePair *pair = data;

  g_clear_object (&pair->a);
  g_clear_object (&pair->b);
}

static inline void
ide_pair_unref (IdePair *pair)
{
  g_atomic_rc_box_release_full (pair, _ide_pair_finalize);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdePair, ide_pair_unref)

G_END_DECLS
