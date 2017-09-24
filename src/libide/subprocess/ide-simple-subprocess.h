/* ide-simple-subprocess.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-subprocess.h"

G_BEGIN_DECLS

#define IDE_TYPE_SIMPLE_SUBPROCESS (ide_simple_subprocess_get_type())

G_DECLARE_FINAL_TYPE (IdeSimpleSubprocess, ide_simple_subprocess, IDE, SIMPLE_SUBPROCESS, GObject)

IdeSubprocess *ide_simple_subprocess_new (GSubprocess *subprocess);

G_END_DECLS
