/* ide-preferences-addin.h
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <gtk/gtk.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_PREFERENCES_ADDIN (ide_preferences_addin_get_type())

G_DECLARE_INTERFACE (IdePreferencesAddin, ide_preferences_addin, IDE, PREFERENCES_ADDIN, GObject)

struct _IdePreferencesAddinInterface
{
  GTypeInterface parent_interface;

  void (*load)   (IdePreferencesAddin *self,
                  DzlPreferences      *preferences);
  void (*unload) (IdePreferencesAddin *self,
                  DzlPreferences      *preferences);
};

IDE_AVAILABLE_IN_ALL
void ide_preferences_addin_load   (IdePreferencesAddin *self,
                                   DzlPreferences      *preferences);
IDE_AVAILABLE_IN_ALL
void ide_preferences_addin_unload (IdePreferencesAddin *self,
                                   DzlPreferences      *preferences);

G_END_DECLS
