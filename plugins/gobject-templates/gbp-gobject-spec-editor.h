/* gbp-gobject-spec-editor.h
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

#ifndef GBP_GOBJECT_SPEC_EDITOR_H
#define GBP_GOBJECT_SPEC_EDITOR_H

#include <gtk/gtk.h>

#include "gbp-gobject-spec.h"

G_BEGIN_DECLS

#define GBP_TYPE_GOBJECT_SPEC_EDITOR (gbp_gobject_spec_editor_get_type())

G_DECLARE_FINAL_TYPE (GbpGobjectSpecEditor, gbp_gobject_spec_editor, GBP, GOBJECT_SPEC_EDITOR, GtkBin)

GtkWidget      *gbp_gobject_spec_editor_new           (GbpGobjectSpecEditor *self);
GbpGobjectSpec *gbp_gobject_spec_editor_get_spec      (GbpGobjectSpecEditor *self);
void            gbp_gobject_spec_editor_set_spec      (GbpGobjectSpecEditor *self,
                                                       GbpGobjectSpec       *spec);
GFile          *gbp_gobject_spec_editor_get_directory (GbpGobjectSpecEditor *self);
void            gbp_gobject_spec_editor_set_directory (GbpGobjectSpecEditor *self,
                                                       GFile                *file);

G_END_DECLS

#endif /* GBP_GOBJECT_SPEC_EDITOR_H */
