/* ide-theme-manager.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_THEME_MANAGER_H
#define IDE_THEME_MANAGER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_THEME_MANAGER (ide_theme_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeThemeManager, ide_theme_manager, IDE, THEME_MANAGER, GObject)

IdeThemeManager *ide_theme_manager_new (void);

G_END_DECLS

#endif /* IDE_THEME_MANAGER_H */
