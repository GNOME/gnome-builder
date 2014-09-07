/* gb-tab-label.h
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

#ifndef GB_TAB_LABEL_H
#define GB_TAB_LABEL_H

#include <gtk/gtk.h>

#include "gb-tab.h"

G_BEGIN_DECLS

#define GB_TYPE_TAB_LABEL            (gb_tab_label_get_type())
#define GB_TAB_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TAB_LABEL, GbTabLabel))
#define GB_TAB_LABEL_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TAB_LABEL, GbTabLabel const))
#define GB_TAB_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_TAB_LABEL, GbTabLabelClass))
#define GB_IS_TAB_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_TAB_LABEL))
#define GB_IS_TAB_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_TAB_LABEL))
#define GB_TAB_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_TAB_LABEL, GbTabLabelClass))

typedef struct _GbTabLabel        GbTabLabel;
typedef struct _GbTabLabelClass   GbTabLabelClass;
typedef struct _GbTabLabelPrivate GbTabLabelPrivate;

struct _GbTabLabel
{
  GtkBin parent;

  /*< private >*/
  GbTabLabelPrivate *priv;
};

struct _GbTabLabelClass
{
  GtkBinClass parent_class;

  void (*close_clicked) (GbTabLabel *label);
};

GType      gb_tab_label_get_type (void) G_GNUC_CONST;
GbTab     *gb_tab_label_get_tab  (GbTabLabel *label);
GtkWidget *gb_tab_label_new      (GbTab *tab);

G_END_DECLS

#endif /* GB_TAB_LABEL_H */
