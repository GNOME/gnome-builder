/* tmpl-iterator.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (TMPL_GLIB_INSIDE) && !defined (TMPL_GLIB_COMPILATION)
# error "Only <tmpl-glib.h> can be included directly."
#endif

#ifndef TMPL_ITERATOR_H
#define TMPL_ITERATOR_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TmplIterator TmplIterator;

struct _TmplIterator
{
  /*< private >*/
  gpointer instance;  /* Data */
  gpointer move_next; /* MoveNext */
  gpointer get_value; /* GetValue */
  gpointer destroy;   /* Destroy */
  gpointer data1;
  gpointer data2;
  gpointer data3;
  gpointer data4;
};

void     tmpl_iterator_init      (TmplIterator *self,
                                  const GValue *value);
gboolean tmpl_iterator_next      (TmplIterator *self);
void     tmpl_iterator_get_value (TmplIterator *self,
                                  GValue       *value);
void     tmpl_iterator_destroy   (TmplIterator *self);

G_END_DECLS

#endif /* TMPL_ITERATOR_H */
