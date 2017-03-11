/*
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef EGG_PROGRESS_BUTTON_H
#define EGG_PROGRESS_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_PROGRESS_BUTTON (egg_progress_button_get_type ())

G_DECLARE_DERIVABLE_TYPE (EggProgressButton, egg_progress_button, EGG, PROGRESS_BUTTON, GtkButton)

struct _EggProgressButtonClass
{
  GtkButtonClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

GtkWidget	*egg_progress_button_new               (void);
guint      egg_progress_button_get_progress      (EggProgressButton *self);
void       egg_progress_button_set_progress	     (EggProgressButton	*button,
                                                  guint              percentage);
gboolean   egg_progress_button_get_show_progress (EggProgressButton *self);
void       egg_progress_button_set_show_progress (EggProgressButton *button,
					      																 	gboolean           show_progress);

G_END_DECLS

#endif /* EGG_PROGRESS_BUTTON_H */
