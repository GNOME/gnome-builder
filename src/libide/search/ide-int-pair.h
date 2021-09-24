/* ide-int-pair.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_INT_PAIR_H
#define IDE_INT_PAIR_H

#ifndef __GI_SCANNER__

#include <glib.h>
#include <stdlib.h>

G_BEGIN_DECLS

#if GLIB_SIZEOF_VOID_P == 8
# define IDE_INT_PAIR_64
#endif

#ifdef IDE_INT_PAIR_64

typedef union
{
  /*< private >*/
  struct {
    gint first;
    gint second;
  };
  gpointer ptr;
} IdeIntPair;

typedef union
{
  /*< private >*/
  struct {
    guint first;
    guint second;
  };
  gpointer ptr;
} IdeUIntPair;

#else

typedef struct
{
  /*< private >*/
  gint first;
  gint second;
} IdeIntPair;

typedef struct
{
  /*< private >*/
  guint first;
  guint second;
} IdeUIntPair;

#endif

/**
 * ide_int_pair_new: (skip)
 */
static inline IdeIntPair *
ide_int_pair_new (gint first, gint second)
{
  IdeIntPair pair;

  /* Avoid tripping g-ir-scanner by putting this
   * inside the inline function.
   */
  G_STATIC_ASSERT (sizeof (IdeIntPair) == 8);

  pair.first = first;
  pair.second = second;

#ifdef IDE_INT_PAIR_64
  return (IdeIntPair *)pair.ptr;
#else
  return g_slice_copy (sizeof (IdeIntPair), &pair);
#endif
}

/**
 * ide_uint_pair_new: (skip)
 */
static inline IdeUIntPair *
ide_uint_pair_new (guint first, guint second)
{
  IdeUIntPair pair;

  /* Avoid tripping g-ir-scanner by putting this
   * inside the inline function.
   */
  G_STATIC_ASSERT (sizeof (IdeUIntPair) == 8);

  pair.first = first;
  pair.second = second;

#ifdef IDE_INT_PAIR_64
  return (IdeUIntPair *)pair.ptr;
#else
  return g_slice_copy (sizeof (IdeUIntPair), &pair);
#endif
}

/**
 * ide_int_pair_first: (skip)
 */
static inline gint
ide_int_pair_first (IdeIntPair *pair)
{
  IdeIntPair p;
#ifdef IDE_INT_PAIR_64
  p.ptr = pair;
#else
  p = *pair;
#endif
  return p.first;
}

/**
 * ide_int_pair_second: (skip)
 */
static inline gint
ide_int_pair_second (IdeIntPair *pair)
{
  IdeIntPair p;
#ifdef IDE_INT_PAIR_64
  p.ptr = pair;
#else
  p = *pair;
#endif
  return p.second;
}

/**
 * ide_uint_pair_first: (skip)
 */
static inline guint
ide_uint_pair_first (IdeUIntPair *pair)
{
  IdeUIntPair p;
#ifdef IDE_INT_PAIR_64
  p.ptr = pair;
#else
  p = *pair;
#endif
  return p.first;
}

/**
 * ide_uint_pair_second: (skip)
 */
static inline guint
ide_uint_pair_second (IdeUIntPair *pair)
{
  IdeUIntPair p;
#ifdef IDE_INT_PAIR_64
  p.ptr = pair;
#else
  p = *pair;
#endif
  return p.second;
}

/**
 * ide_int_pair_free: (skip)
 */
static inline void
ide_int_pair_free (IdeIntPair *pair)
{
#ifdef IDE_INT_PAIR_64
  /* Do Nothing */
#else
  g_slice_free (IdeIntPair, pair);
#endif
}

/**
 * ide_uint_pair_free: (skip)
 */
static inline void
ide_uint_pair_free (IdeUIntPair *pair)
{
#ifdef IDE_INT_PAIR_64
  /* Do Nothing */
#else
  g_slice_free (IdeUIntPair, pair);
#endif
}

G_END_DECLS

#endif /* __GI_SCANNER__ */

#endif /* IDE_INT_PAIR_H */
