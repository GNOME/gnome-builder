/* gb-keybindings.h
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

#ifndef GB_KEYBINDINGS_H
#define GB_KEYBINDINGS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_KEYBINDINGS            (gb_keybindings_get_type())
#define GB_KEYBINDINGS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_KEYBINDINGS, GbKeybindings))
#define GB_KEYBINDINGS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_KEYBINDINGS, GbKeybindings const))
#define GB_KEYBINDINGS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_KEYBINDINGS, GbKeybindingsClass))
#define GB_IS_KEYBINDINGS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_KEYBINDINGS))
#define GB_IS_KEYBINDINGS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_KEYBINDINGS))
#define GB_KEYBINDINGS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_KEYBINDINGS, GbKeybindingsClass))

typedef struct _GbKeybindings        GbKeybindings;
typedef struct _GbKeybindingsClass   GbKeybindingsClass;
typedef struct _GbKeybindingsPrivate GbKeybindingsPrivate;

struct _GbKeybindings
{
  GObject parent;

  /*< private >*/
  GbKeybindingsPrivate *priv;
};

struct _GbKeybindingsClass
{
  GObjectClass parent_class;
};

GType          gb_keybindings_get_type   (void);
GbKeybindings *gb_keybindings_new        (void);
gboolean       gb_keybindings_load_bytes (GbKeybindings   *keybindings,
                                          GBytes          *bytes,
                                          GError         **error);
gboolean       gb_keybindings_load_path  (GbKeybindings   *keybindings,
                                          const gchar     *path,
                                          GError         **error);
void           gb_keybindings_register   (GbKeybindings   *keybindings,
                                          GtkApplication  *application);

G_END_DECLS

#endif /* GB_KEYBINDINGS_H */
