/* ide-project-edit.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include <glib-object.h>

#include "diagnostics/ide-source-range.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_EDIT (ide_project_edit_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeProjectEdit, ide_project_edit, IDE, PROJECT_EDIT, GObject)

struct _IdeProjectEditClass
{
  GObjectClass parent_instance;

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

IdeProjectEdit *ide_project_edit_new             (void);
IdeSourceRange *ide_project_edit_get_range       (IdeProjectEdit *self);
void            ide_project_edit_set_range       (IdeProjectEdit *self,
                                                  IdeSourceRange *range);
const gchar    *ide_project_edit_get_replacement (IdeProjectEdit *self);
void            ide_project_edit_set_replacement (IdeProjectEdit *self,
                                                  const gchar    *replacement);

G_END_DECLS
