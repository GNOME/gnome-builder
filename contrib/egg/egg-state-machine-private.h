/* egg-state-machine-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EGG_STATE_MACHINE_PRIVATE_H
#define EGG_STATE_MACHINE_PRIVATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct
{
  gchar      *state;
  GPtrArray  *actions;
  GHashTable *states;
  gchar      *freeze_state;
  gint        freeze_count;
} EggStateMachinePrivate;

typedef struct
{
  gchar      *name;
  GHashTable *signals;
  GHashTable *bindings;
  GPtrArray  *properties;
  GPtrArray  *styles;
} EggState;

typedef struct
{
  gpointer  object;
  gchar    *property;
  GValue    value;
} EggStateProperty;

typedef struct
{
  GtkWidget *widget;
  gchar     *name;
} EggStateStyle;

G_END_DECLS

#endif /* EGG_STATE_MACHINE_PRIVATE_H */
