/* ide-preferences-addin.h
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-preferences-window.h"

G_BEGIN_DECLS

#define IDE_TYPE_PREFERENCES_ADDIN (ide_preferences_addin_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdePreferencesAddin, ide_preferences_addin, IDE, PREFERENCES_ADDIN, GObject)

struct _IdePreferencesAddinInterface
{
  GTypeInterface parent_interface;

  void (*load)   (IdePreferencesAddin  *self,
                  IdePreferencesWindow *preferences,
                  IdeContext           *context);
  void (*unload) (IdePreferencesAddin  *self,
                  IdePreferencesWindow *preferences,
                  IdeContext           *context);
};

IDE_AVAILABLE_IN_ALL
void ide_preferences_addin_load   (IdePreferencesAddin  *self,
                                   IdePreferencesWindow *preferences,
                                   IdeContext           *context);
IDE_AVAILABLE_IN_ALL
void ide_preferences_addin_unload (IdePreferencesAddin  *self,
                                   IdePreferencesWindow *preferences,
                                   IdeContext           *context);

G_END_DECLS
