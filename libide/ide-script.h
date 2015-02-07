/* ide-script.h
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

#ifndef IDE_SCRIPT_H
#define IDE_SCRIPT_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SCRIPT (ide_script_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeScript, ide_script, IDE, SCRIPT, IdeObject)

struct _IdeScriptClass
{
  IdeObjectClass parent;

  void (*load)   (IdeScript *script);
  void (*unload) (IdeScript *script);
};

void ide_script_load   (IdeScript *script);
void ide_script_unload (IdeScript *script);

G_END_DECLS

#endif /* IDE_SCRIPT_H */
