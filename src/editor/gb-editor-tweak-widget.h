/* gb-editor-tweak-widget.h
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

#ifndef GB_EDITOR_TWEAK_WIDGET_H
#define GB_EDITOR_TWEAK_WIDGET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_TWEAK_WIDGET            (gb_editor_tweak_widget_get_type())
#define GB_EDITOR_TWEAK_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_TWEAK_WIDGET, GbEditorTweakWidget))
#define GB_EDITOR_TWEAK_WIDGET_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_TWEAK_WIDGET, GbEditorTweakWidget const))
#define GB_EDITOR_TWEAK_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_TWEAK_WIDGET, GbEditorTweakWidgetClass))
#define GB_IS_EDITOR_TWEAK_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_TWEAK_WIDGET))
#define GB_IS_EDITOR_TWEAK_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_TWEAK_WIDGET))
#define GB_EDITOR_TWEAK_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_TWEAK_WIDGET, GbEditorTweakWidgetClass))

typedef struct _GbEditorTweakWidget        GbEditorTweakWidget;
typedef struct _GbEditorTweakWidgetClass   GbEditorTweakWidgetClass;
typedef struct _GbEditorTweakWidgetPrivate GbEditorTweakWidgetPrivate;

struct _GbEditorTweakWidget
{
  GtkBin parent;

  /*< private >*/
  GbEditorTweakWidgetPrivate *priv;
};

struct _GbEditorTweakWidgetClass
{
  GtkBinClass parent;
};

GType      gb_editor_tweak_widget_get_type (void);
GtkWidget *gb_editor_tweak_widget_new      (void);

G_END_DECLS

#endif /* GB_EDITOR_TWEAK_WIDGET_H */
