/* ide-clang-service.h
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

#ifndef IDE_CLANG_SERVICE_H
#define IDE_CLANG_SERVICE_H

#include "ide-service.h"

G_BEGIN_DECLS

#define IDE_TYPE_CLANG_SERVICE (ide_clang_service_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeClangService, ide_clang_service, IDE, CLANG_SERVICE, IdeService)

struct _IdeClangServiceClass
{
  IdeServiceClass parent;
};

G_END_DECLS

#endif /* IDE_CLANG_SERVICE_H */
