/* ide-preferences-flow-box.h
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

#ifndef IDE_PREFERENCES_FLOW_BOX_H
#define IDE_PREFERENCES_FLOW_BOX_H

#include <dazzle.h>

G_BEGIN_DECLS

#define IDE_TYPE_PREFERENCES_FLOW_BOX (ide_preferences_flow_box_get_type())

G_DECLARE_FINAL_TYPE (IdePreferencesFlowBox, ide_preferences_flow_box, IDE, PREFERENCES_FLOW_BOX, DzlColumnLayout)

GtkWidget *ide_preferences_flow_box_new (void);

G_END_DECLS

#endif /* IDE_PREFERENCES_FLOW_BOX_H */
