/* gb-source-formatter.h
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

#ifndef GB_SOURCE_FORMATTER_H
#define GB_SOURCE_FORMATTER_H

#include <gio/gio.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_FORMATTER            (gb_source_formatter_get_type())
#define GB_SOURCE_FORMATTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_FORMATTER, GbSourceFormatter))
#define GB_SOURCE_FORMATTER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_FORMATTER, GbSourceFormatter const))
#define GB_SOURCE_FORMATTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_FORMATTER, GbSourceFormatterClass))
#define GB_IS_SOURCE_FORMATTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_FORMATTER))
#define GB_IS_SOURCE_FORMATTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_FORMATTER))
#define GB_SOURCE_FORMATTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_FORMATTER, GbSourceFormatterClass))

typedef struct _GbSourceFormatter        GbSourceFormatter;
typedef struct _GbSourceFormatterClass   GbSourceFormatterClass;
typedef struct _GbSourceFormatterPrivate GbSourceFormatterPrivate;

struct _GbSourceFormatter
{
  GObject parent;

  /*< private >*/
  GbSourceFormatterPrivate *priv;
};

struct _GbSourceFormatterClass
{
  GObjectClass parent_class;
};

GType              gb_source_formatter_get_type          (void) G_GNUC_CONST;
GbSourceFormatter *gb_source_formatter_new_from_language (GtkSourceLanguage  *language);
gboolean           gb_source_formatter_format            (GbSourceFormatter  *formatter,
                                                          const gchar        *input,
                                                          gboolean            is_fragment,
                                                          GCancellable       *cancellable,
                                                          gchar             **output,
                                                          GError            **error);

G_END_DECLS

#endif /* GB_SOURCE_FORMATTER_H */
