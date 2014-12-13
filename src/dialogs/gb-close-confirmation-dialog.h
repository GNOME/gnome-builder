/* gb-close-confirmation-dialog.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2004-2005 GNOME Foundation
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#ifndef GB_CLOSE_CONFIRMATION_DIALOG_H
#define GB_CLOSE_CONFIRMATION_DIALOG_H

#include <gtk/gtk.h>

#include "gb-document.h"

G_BEGIN_DECLS

#define GB_TYPE_CLOSE_CONFIRMATION_DIALOG            (gb_close_confirmation_dialog_get_type())
#define GB_CLOSE_CONFIRMATION_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_CLOSE_CONFIRMATION_DIALOG, GbCloseConfirmationDialog))
#define GB_CLOSE_CONFIRMATION_DIALOG_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_CLOSE_CONFIRMATION_DIALOG, GbCloseConfirmationDialog const))
#define GB_CLOSE_CONFIRMATION_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_CLOSE_CONFIRMATION_DIALOG, GbCloseConfirmationDialogClass))
#define GB_IS_CLOSE_CONFIRMATION_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_CLOSE_CONFIRMATION_DIALOG))
#define GB_IS_CLOSE_CONFIRMATION_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_CLOSE_CONFIRMATION_DIALOG))
#define GB_CLOSE_CONFIRMATION_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_CLOSE_CONFIRMATION_DIALOG, GbCloseConfirmationDialogClass))

typedef struct _GbCloseConfirmationDialog        GbCloseConfirmationDialog;
typedef struct _GbCloseConfirmationDialogClass   GbCloseConfirmationDialogClass;
typedef struct _GbCloseConfirmationDialogPrivate GbCloseConfirmationDialogPrivate;

struct _GbCloseConfirmationDialog
{
  GtkDialog parent;

  /*< private >*/
  GbCloseConfirmationDialogPrivate *priv;
};

struct _GbCloseConfirmationDialogClass
{
  GtkDialogClass parent_class;
};

GType        gb_close_confirmation_dialog_get_type               (void);
GtkWidget   *gb_close_confirmation_dialog_new                    (GtkWindow                 *parent,
                                                                  GList                     *unsaved_documents);
GtkWidget   *gb_close_confirmation_dialog_new_single             (GtkWindow                 *parent,
                                                                  GbDocument                *doc);
const GList *gb_close_confirmation_dialog_get_unsaved_documents  (GbCloseConfirmationDialog *dlg);
GList       *gb_close_confirmation_dialog_get_selected_documents (GbCloseConfirmationDialog *dlg);

G_END_DECLS

#endif /* GB_CLOSE_CONFIRMATION_DIALOG_H */
