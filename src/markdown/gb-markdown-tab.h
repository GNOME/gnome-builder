/* gb-markdown-tab.h
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

#ifndef GB_MARKDOWN_TAB_H
#define GB_MARKDOWN_TAB_H

#include "gb-tab.h"

G_BEGIN_DECLS

#define GB_TYPE_MARKDOWN_TAB            (gb_markdown_tab_get_type())
#define GB_MARKDOWN_TAB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_MARKDOWN_TAB, GbMarkdownTab))
#define GB_MARKDOWN_TAB_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_MARKDOWN_TAB, GbMarkdownTab const))
#define GB_MARKDOWN_TAB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_MARKDOWN_TAB, GbMarkdownTabClass))
#define GB_IS_MARKDOWN_TAB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_MARKDOWN_TAB))
#define GB_IS_MARKDOWN_TAB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_MARKDOWN_TAB))
#define GB_MARKDOWN_TAB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_MARKDOWN_TAB, GbMarkdownTabClass))

typedef struct _GbMarkdownTab        GbMarkdownTab;
typedef struct _GbMarkdownTabClass   GbMarkdownTabClass;
typedef struct _GbMarkdownTabPrivate GbMarkdownTabPrivate;

struct _GbMarkdownTab
{
  GbTab parent;

  /*< private >*/
  GbMarkdownTabPrivate *priv;
};

struct _GbMarkdownTabClass
{
  GbTabClass parent;
};

GType  gb_markdown_tab_get_type (void);
GbTab *gb_markdown_tab_new      (GtkTextBuffer *buffer);

G_END_DECLS

#endif /* GB_MARKDOWN_TAB_H */
