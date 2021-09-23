/* ide-signal-group.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Garrett Regier <garrettregier@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
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

#pragma once

#include <glib-object.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_SIGNAL_GROUP (ide_signal_group_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSignalGroup, ide_signal_group, IDE, SIGNAL_GROUP, GObject)

IDE_AVAILABLE_IN_ALL
IdeSignalGroup *ide_signal_group_new             (GType           target_type);

IDE_AVAILABLE_IN_ALL
void            ide_signal_group_set_target      (IdeSignalGroup *self,
                                                  gpointer        target);
IDE_AVAILABLE_IN_ALL
gpointer        ide_signal_group_get_target      (IdeSignalGroup *self);

IDE_AVAILABLE_IN_ALL
void            ide_signal_group_block           (IdeSignalGroup *self);
IDE_AVAILABLE_IN_ALL
void            ide_signal_group_unblock         (IdeSignalGroup *self);

IDE_AVAILABLE_IN_ALL
void            ide_signal_group_connect_object  (IdeSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        object,
                                                  GConnectFlags   flags);
IDE_AVAILABLE_IN_ALL
void            ide_signal_group_connect_data    (IdeSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        data,
                                                  GClosureNotify  notify,
                                                  GConnectFlags   flags);
IDE_AVAILABLE_IN_ALL
void            ide_signal_group_connect         (IdeSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        data);
IDE_AVAILABLE_IN_ALL
void            ide_signal_group_connect_after   (IdeSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        data);
IDE_AVAILABLE_IN_ALL
void            ide_signal_group_connect_swapped (IdeSignalGroup *self,
                                                  const gchar    *detailed_signal,
                                                  GCallback       c_handler,
                                                  gpointer        data);

G_END_DECLS
