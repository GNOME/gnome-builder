/* gb-source-style-scheme-button.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_SOURCE_STYLE_SCHEME_BUTTON_H
#define GB_SOURCE_STYLE_SCHEME_BUTTON_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON            (gb_source_style_scheme_button_get_type())
#define GB_SOURCE_STYLE_SCHEME_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON, GbSourceStyleSchemeButton))
#define GB_SOURCE_STYLE_SCHEME_BUTTON_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON, GbSourceStyleSchemeButton const))
#define GB_SOURCE_STYLE_SCHEME_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON, GbSourceStyleSchemeButtonClass))
#define GB_IS_SOURCE_STYLE_SCHEME_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON))
#define GB_IS_SOURCE_STYLE_SCHEME_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON))
#define GB_SOURCE_STYLE_SCHEME_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_STYLE_SCHEME_BUTTON, GbSourceStyleSchemeButtonClass))

typedef struct _GbSourceStyleSchemeButton        GbSourceStyleSchemeButton;
typedef struct _GbSourceStyleSchemeButtonClass   GbSourceStyleSchemeButtonClass;
typedef struct _GbSourceStyleSchemeButtonPrivate GbSourceStyleSchemeButtonPrivate;

struct _GbSourceStyleSchemeButton
{
  GtkToggleButton parent;

  /*< private >*/
  GbSourceStyleSchemeButtonPrivate *priv;
};

struct _GbSourceStyleSchemeButtonClass
{
  GtkToggleButtonClass parent;
};

GType                 gb_source_style_scheme_button_get_type              (void);
GtkWidget            *gb_source_style_scheme_button_new                   (void);
GtkSourceStyleScheme *gb_source_style_scheme_button_get_style_scheme      (GbSourceStyleSchemeButton *button);
const gchar          *gb_source_style_scheme_button_get_style_scheme_name (GbSourceStyleSchemeButton *button);
void                  gb_source_style_scheme_button_set_style_scheme_name (GbSourceStyleSchemeButton *button,
                                                                           const gchar               *style_scheme_name);

G_END_DECLS

#endif /* GB_SOURCE_STYLE_SCHEME_BUTTON_H */
