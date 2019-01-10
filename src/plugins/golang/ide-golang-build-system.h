/* ide-golang-build-system.h
 *
 * Copyright 2018 Loïc BLOT <loic.blot@unix-experience.fr>
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

#include <libide-foundry.h>

G_BEGIN_DECLS

#define IDE_TYPE_GOLANG_BUILD_SYSTEM (ide_golang_build_system_get_type())

G_DECLARE_FINAL_TYPE (IdeGolangBuildSystem, ide_golang_build_system, IDE, GOLANG_BUILD_SYSTEM, IdeObject)

G_END_DECLS
