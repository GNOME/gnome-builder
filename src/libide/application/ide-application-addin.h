/* ide-application-addin.h
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

#include "application/ide-application.h"

G_BEGIN_DECLS

#define IDE_TYPE_APPLICATION_ADDIN (ide_application_addin_get_type())

G_DECLARE_INTERFACE (IdeApplicationAddin, ide_application_addin, IDE, APPLICATION_ADDIN, GObject)

/**
 * IdeApplicationAddinInterface:
 * @load: Set this field to implement the ide_application_addin_load()
 *   virtual method.
 * @unload: Set this field to implement the ide_application_addin_unload()
 *   virtual method.
 */
struct _IdeApplicationAddinInterface
{
  GTypeInterface parent_interface;

  void (*load)   (IdeApplicationAddin *self,
                  IdeApplication      *application);
  void (*unload) (IdeApplicationAddin *self,
                  IdeApplication      *application);
};

void ide_application_addin_load   (IdeApplicationAddin *self,
                                   IdeApplication      *application);
void ide_application_addin_unload (IdeApplicationAddin *self,
                                   IdeApplication      *application);

G_END_DECLS
