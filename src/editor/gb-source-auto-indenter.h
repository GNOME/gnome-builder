/* gb-source-auto-indenter.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_SOURCE_AUTO_INDENTER_H
#define GB_SOURCE_AUTO_INDENTER_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_AUTO_INDENTER            (gb_source_auto_indenter_get_type())
#define GB_SOURCE_AUTO_INDENTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_AUTO_INDENTER, GbSourceAutoIndenter))
#define GB_SOURCE_AUTO_INDENTER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_AUTO_INDENTER, GbSourceAutoIndenter const))
#define GB_SOURCE_AUTO_INDENTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_AUTO_INDENTER, GbSourceAutoIndenterClass))
#define GB_IS_SOURCE_AUTO_INDENTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_AUTO_INDENTER))
#define GB_IS_SOURCE_AUTO_INDENTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_AUTO_INDENTER))
#define GB_SOURCE_AUTO_INDENTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_AUTO_INDENTER, GbSourceAutoIndenterClass))

typedef struct _GbSourceAutoIndenter        GbSourceAutoIndenter;
typedef struct _GbSourceAutoIndenterClass   GbSourceAutoIndenterClass;
typedef struct _GbSourceAutoIndenterPrivate GbSourceAutoIndenterPrivate;

struct _GbSourceAutoIndenter
{
  GObject parent;

  /*< private >*/
  GbSourceAutoIndenterPrivate *priv;
};

struct _GbSourceAutoIndenterClass
{
  GObjectClass parent_class;

  /*
   * TODO: Remove "query".
   */
  gchar *(*query) (GbSourceAutoIndenter *indenter,
                   GtkTextView          *view,
                   GtkTextBuffer        *buffer,
                   GtkTextIter          *iter);

  gchar *(*format) (GbSourceAutoIndenter *indenter,
                    GtkTextView          *view,
                    GtkTextBuffer        *buffer,
                    GtkTextIter          *begin,
                    GtkTextIter          *end,
                    gint                 *cursor_offset,
                    GdkEventKey          *trigger);

  gboolean (*is_trigger) (GbSourceAutoIndenter *indenter,
                          GdkEventKey          *event);

  gpointer padding[6];
};

GType     gb_source_auto_indenter_get_type   (void) G_GNUC_CONST;
gchar    *gb_source_auto_indenter_query      (GbSourceAutoIndenter *indenter,
                                              GtkTextView          *view,
                                              GtkTextBuffer        *buffer,
                                              GtkTextIter          *iter);
gboolean  gb_source_auto_indenter_is_trigger (GbSourceAutoIndenter *indenter,
                                              GdkEventKey          *event);
gchar    *gb_source_auto_indenter_format     (GbSourceAutoIndenter *indenter,
                                              GtkTextView          *view,
                                              GtkTextBuffer        *buffer,
                                              GtkTextIter          *begin,
                                              GtkTextIter          *end,
                                              gint                 *cursor_offset,
                                              GdkEventKey          *event);

G_END_DECLS

#endif /* GB_SOURCE_AUTO_INDENTER_H */
