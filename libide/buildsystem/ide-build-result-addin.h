/* ide-build-result-addin.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_BUILD_RESULT_ADDIN_H
#define IDE_BUILD_RESULT_ADDIN_H

#include "ide-build-result.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_RESULT_ADDIN (ide_build_result_addin_get_type ())

G_DECLARE_INTERFACE (IdeBuildResultAddin, ide_build_result_addin, IDE, BUILD_RESULT_ADDIN, GObject)

struct _IdeBuildResultAddinInterface
{
  GTypeInterface parent;

  void (*load)   (IdeBuildResultAddin *self,
                  IdeBuildResult      *result);
  void (*unload) (IdeBuildResultAddin *self,
                  IdeBuildResult      *result);
};

void ide_build_result_addin_load   (IdeBuildResultAddin *self,
                                    IdeBuildResult      *result);
void ide_build_result_addin_unload (IdeBuildResultAddin *self,
                                    IdeBuildResult      *result);

G_END_DECLS

#endif /* IDE_BUILD_RESULT_ADDIN_H */
