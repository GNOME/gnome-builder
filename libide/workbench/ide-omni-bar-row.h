/* ide-omni-bar-row.h
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

#ifndef IDE_OMNI_BAR_ROW_H
#define IDE_OMNI_BAR_ROW_H

#include <gtk/gtk.h>

#include "buildsystem/ide-configuration.h"

G_BEGIN_DECLS

#define IDE_TYPE_OMNI_BAR_ROW (ide_omni_bar_row_get_type())

G_DECLARE_FINAL_TYPE (IdeOmniBarRow, ide_omni_bar_row, IDE, OMNI_BAR_ROW, GtkListBoxRow)

GtkWidget        *ide_omni_bar_row_new        (IdeConfiguration *configuration);
IdeConfiguration *ide_omni_bar_row_get_item   (IdeOmniBarRow    *self);
void              ide_omni_bar_row_set_active (IdeOmniBarRow    *self,
                                               gboolean          active);

G_END_DECLS

#endif /* IDE_OMNI_BAR_ROW_H */
