/* gb-source-emacs.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Roberto Majadas <roberto.majadas@openshine.com>
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

#ifndef GB_SOURCE_EMACS_H
#define GB_SOURCE_EMACS_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_EMACS            (gb_source_emacs_get_type())
#define GB_SOURCE_EMACS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_EMACS, GbSourceEmacs))
#define GB_SOURCE_EMACS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_EMACS, GbSourceEmacs const))
#define GB_SOURCE_EMACS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_EMACS, GbSourceEmacsClass))
#define GB_IS_SOURCE_EMACS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_EMACS))
#define GB_IS_SOURCE_EMACS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_EMACS))
#define GB_SOURCE_EMACS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_EMACS, GbSourceEmacsClass))

typedef struct _GbSourceEmacs        GbSourceEmacs;
typedef struct _GbSourceEmacsClass   GbSourceEmacsClass;
typedef struct _GbSourceEmacsPrivate GbSourceEmacsPrivate;

struct _GbSourceEmacs
{
  GObject parent;

  /*< private >*/
  GbSourceEmacsPrivate *priv;
};

struct _GbSourceEmacsClass
{
  GObjectClass parent_class;

  gpointer _padding1;
  gpointer _padding2;
  gpointer _padding3;
};

GType            gb_source_emacs_get_type          (void);
GbSourceEmacs    *gb_source_emacs_new              (GtkTextView     *text_view);
gboolean         gb_source_emacs_get_enabled       (GbSourceEmacs     *emacs);
void             gb_source_emacs_set_enabled       (GbSourceEmacs     *emacs,
                                                    gboolean         enabled);


G_END_DECLS

#endif /* GB_SOURCE_EMACS_H */
