/* gb-document-menu-button.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_DOCUMENT_MENU_BUTTON_H
#define GB_DOCUMENT_MENU_BUTTON_H

#include <gtk/gtk.h>

#include "gb-document.h"
#include "gb-document-manager.h"

G_BEGIN_DECLS

#define GB_TYPE_DOCUMENT_MENU_BUTTON            (gb_document_menu_button_get_type())
#define GB_DOCUMENT_MENU_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT_MENU_BUTTON, GbDocumentMenuButton))
#define GB_DOCUMENT_MENU_BUTTON_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_DOCUMENT_MENU_BUTTON, GbDocumentMenuButton const))
#define GB_DOCUMENT_MENU_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_DOCUMENT_MENU_BUTTON, GbDocumentMenuButtonClass))
#define GB_IS_DOCUMENT_MENU_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_DOCUMENT_MENU_BUTTON))
#define GB_IS_DOCUMENT_MENU_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_DOCUMENT_MENU_BUTTON))
#define GB_DOCUMENT_MENU_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_DOCUMENT_MENU_BUTTON, GbDocumentMenuButtonClass))

typedef struct _GbDocumentMenuButton        GbDocumentMenuButton;
typedef struct _GbDocumentMenuButtonClass   GbDocumentMenuButtonClass;
typedef struct _GbDocumentMenuButtonPrivate GbDocumentMenuButtonPrivate;

struct _GbDocumentMenuButton
{
  GtkMenuButton parent;

  /*< private >*/
  GbDocumentMenuButtonPrivate *priv;
};

struct _GbDocumentMenuButtonClass
{
  GtkMenuButtonClass parent;

  void (*document_selected) (GbDocumentMenuButton *button,
                             GbDocument           *document);
};

GType              gb_document_menu_button_get_type             (void);
GtkWidget         *gb_document_menu_button_new                  (void);
GbDocumentManager *gb_document_menu_button_get_document_manager (GbDocumentMenuButton *button);
void               gb_document_menu_button_set_document_manager (GbDocumentMenuButton *button,
                                                                 GbDocumentManager    *document_manager);
void               gb_document_menu_button_select_document      (GbDocumentMenuButton *button,
                                                                 GbDocument           *document);
void               gb_document_menu_button_focus_search         (GbDocumentMenuButton *button);

G_END_DECLS

#endif /* GB_DOCUMENT_MENU_BUTTON_H */
