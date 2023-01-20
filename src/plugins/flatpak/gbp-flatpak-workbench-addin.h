/* gbp-flatpak-workbench-addin.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-gui.h>

G_BEGIN_DECLS

#define GBP_TYPE_FLATPAK_WORKBENCH_ADDIN (gbp_flatpak_workbench_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpFlatpakWorkbenchAddin, gbp_flatpak_workbench_addin, GBP, FLATPAK_WORKBENCH_ADDIN, GObject)

void gbp_flatpak_begin_message (GbpFlatpakWorkbenchAddin *self,
                                const char               *message_id,
                                const char               *title,
                                const char               *icon_name,
                                const char               *message);
void gbp_flatpak_end_message   (GbpFlatpakWorkbenchAddin *self,
                                const char               *message_id);

G_END_DECLS
