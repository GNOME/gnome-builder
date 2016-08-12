/* gbp-gobject-dialog.h
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

#ifndef GBP_GOBJECT_DIALOG_H
#define GBP_GOBJECT_DIALOG_H

#include <gtk/gtk.h>

#include "gbp-gobject-spec.h"

G_BEGIN_DECLS

#define GBP_TYPE_GOBJECT_DIALOG (gbp_gobject_dialog_get_type())

G_DECLARE_FINAL_TYPE (GbpGobjectDialog, gbp_gobject_dialog, GBP, GOBJECT_DIALOG, GtkAssistant)

GtkWidget      *gbp_gobject_dialog_new           (void);
GFile          *gbp_gobject_dialog_get_directory (GbpGobjectDialog *self);
void            gbp_gobject_dialog_set_directory (GbpGobjectDialog *self,
                                                  GFile            *directory);
GbpGobjectSpec *gbp_gobject_dialog_get_spec      (GbpGobjectDialog *self);

G_END_DECLS

#endif /* GBP_GOBJECT_DIALOG_H */
