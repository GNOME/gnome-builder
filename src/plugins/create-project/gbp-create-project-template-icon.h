/* gbp-create-project-template-icon.h
 *
 * Copyright (C) 2016 Akshaya Kakkilaya <akshaya.kakkilaya@gmail.com>
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

#include <gtk/gtk.h>
#include <ide.h>

G_BEGIN_DECLS

#define GBP_TYPE_CREATE_PROJECT_TEMPLATE_ICON (gbp_create_project_template_icon_get_type())

G_DECLARE_FINAL_TYPE (GbpCreateProjectTemplateIcon, gbp_create_project_template_icon, GBP, CREATE_PROJECT_TEMPLATE_ICON, GtkBin)

G_END_DECLS
