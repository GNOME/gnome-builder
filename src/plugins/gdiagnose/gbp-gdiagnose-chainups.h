/*
 * This file is part of gnome-c-utils.
 *
 * Copyright © 2016, 2017 Sébastien Wilmet <swilmet@gnome.org>
 *
 * gnome-c-utils is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-c-utils is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gnome-c-utils.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <libide-code.h>

G_BEGIN_DECLS

void gbp_gdiagnose_check_chainups (GtkSourceBuffer *buffer,
                                   GFile           *file,
                                   const char      *basename,
                                   IdeDiagnostics  *diagnostics);

G_END_DECLS
