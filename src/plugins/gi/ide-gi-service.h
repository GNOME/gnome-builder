/* ide-gi-service.h
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <ide.h>

#include "ide-gi-repository.h"

G_BEGIN_DECLS

#define IDE_TYPE_GI_SERVICE (ide_gi_service_get_type())

G_DECLARE_FINAL_TYPE (IdeGiService, ide_gi_service, IDE, GI_SERVICE, IdeObject)

IdeGiRepository        *ide_gi_service_get_repository              (IdeGiService  *self);

G_END_DECLS
