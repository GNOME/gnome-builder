/* gb-view.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_VIEW_H
#define GB_VIEW_H

#include <gtk/gtk.h>
#include <ide.h>

#include "gb-document.h"

G_BEGIN_DECLS

#define GB_TYPE_VIEW (gb_view_get_type())

G_DECLARE_DERIVABLE_TYPE (GbView, gb_view, GB, VIEW, GtkBox)

struct _GbViewClass
{
  GtkBinClass parent;

  gboolean     (*get_can_preview)       (GbView             *self);
  gboolean     (*get_can_split)         (GbView             *self);
  GbDocument  *(*get_document)          (GbView             *self);
  const gchar *(*get_title)             (GbView             *self);
  GbView      *(*create_split)          (GbView             *self);
  void         (*set_split_view)        (GbView             *self,
                                         gboolean            split_view);
  void         (*set_back_forward_list) (GbView             *self,
                                         IdeBackForwardList *back_forward_list);
  void         (*navigate_to)           (GbView             *self,
                                         IdeSourceLocation  *location);
};

GbView      *gb_view_create_split          (GbView             *self);
gboolean     gb_view_get_can_preview       (GbView             *self);
gboolean     gb_view_get_can_split         (GbView             *self);
GbDocument  *gb_view_get_document          (GbView             *self);
const gchar *gb_view_get_title             (GbView             *self);
GtkWidget   *gb_view_get_controls          (GbView             *self);
void         gb_view_set_split_view        (GbView             *self,
                                            gboolean            split_view);
void         gb_view_set_back_forward_list (GbView             *self,
                                            IdeBackForwardList *back_forward_list);
void         gb_view_navigate_to           (GbView             *self,
                                            IdeSourceLocation  *location);

G_END_DECLS

#endif /* GB_VIEW_H */
