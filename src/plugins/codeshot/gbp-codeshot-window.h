/* gbp-codeshot-window.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <adwaita.h>

#include <libide-code.h>

G_BEGIN_DECLS

#define GBP_TYPE_CODESHOT_WINDOW (gbp_codeshot_window_get_type())

G_DECLARE_FINAL_TYPE (GbpCodeshotWindow, gbp_codeshot_window, GBP, CODESHOT_WINDOW, AdwWindow)

GtkWidget *gbp_codeshot_window_new (IdeBuffer         *buffer,
                                    const GtkTextIter *begin,
                                    const GtkTextIter *end);

G_END_DECLS
