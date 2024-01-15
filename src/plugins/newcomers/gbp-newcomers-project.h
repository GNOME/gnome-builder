/* ide-newcomer-project.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-greeter.h>

G_BEGIN_DECLS

#define GBP_TYPE_NEWCOMERS_PROJECT (gbp_newcomers_project_get_type())

G_DECLARE_FINAL_TYPE (GbpNewcomersProject, gbp_newcomers_project, GBP, NEWCOMERS_PROJECT, IdeGreeterRow)

const char         *gbp_newcomers_project_get_name      (GbpNewcomersProject *self);
const char         *gbp_newcomers_project_get_uri       (GbpNewcomersProject *self);
GIcon              *gbp_newcomers_project_get_icon      (GbpNewcomersProject *self);
const char * const *gbp_newcomers_project_get_languages (GbpNewcomersProject *self);

G_END_DECLS
