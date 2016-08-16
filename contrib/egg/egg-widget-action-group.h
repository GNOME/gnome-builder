/* egg-widget-action-group.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EGG_WIDGET_ACTION_GROUP_H
#define EGG_WIDGET_ACTION_GROUP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_WIDGET_ACTION_GROUP (egg_widget_action_group_get_type())

G_DECLARE_FINAL_TYPE (EggWidgetActionGroup, egg_widget_action_group, EGG, WIDGET_ACTION_GROUP, GObject)

GActionGroup *egg_widget_action_group_new                (GtkWidget            *widget);
void          egg_widget_action_group_attach             (gpointer              widget,
                                                          const gchar          *group_name);
void          egg_widget_action_group_set_action_enabled (EggWidgetActionGroup *self,
                                                          const gchar          *action_name,
                                                          gboolean              enabled);

G_END_DECLS

#endif /* EGG_WIDGET_ACTION_GROUP_H */
