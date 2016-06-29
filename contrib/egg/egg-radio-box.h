/* egg-radio-box.h
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

#ifndef EGG_RADIO_BOX_H
#define EGG_RADIO_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_RADIO_BOX (egg_radio_box_get_type())

G_DECLARE_DERIVABLE_TYPE (EggRadioBox, egg_radio_box, EGG, RADIO_BOX, GtkBin)

struct _EggRadioBoxClass
{
  GtkBinClass parent_class;

  gpointer _padding1;
  gpointer _padding2;
  gpointer _padding3;
  gpointer _padding4;
};

GtkWidget   *egg_radio_box_new           (void);
void         egg_radio_box_add_item      (EggRadioBox *self,
                                          const gchar *id,
                                          const gchar *text);
const gchar *egg_radio_box_get_active_id (EggRadioBox *self);
void         egg_radio_box_set_active_id (EggRadioBox *self,
                                          const gchar *id);

G_END_DECLS

#endif /* EGG_RADIO_BOX_H */
