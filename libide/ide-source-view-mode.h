/* ide-key-mode.h
 *
 * Copyright (C) 2015 Alexander Larsson <alexl@redhat.com>
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

#ifndef IDE_SOURCE_VIEW_MODE_H
#define IDE_SOURCE_VIEW_MODE_H

#include <gtk/gtk.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW_MODE            (ide_source_view_mode_get_type())
#define IDE_SOURCE_VIEW_MODE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_VIEW_MODE, IdeSourceViewMode))
#define IDE_SOURCE_VIEW_MODE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_VIEW_MODE, IdeSourceViewMode const))
#define IDE_SOURCE_VIEW_MODE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_SOURCE_VIEW_MODE, IdeSourceViewModeClass))
#define IDE_IS_SOURCE_VIEW_MODE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_SOURCE_VIEW_MODE))
#define IDE_IS_SOURCE_VIEW_MODE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_SOURCE_VIEW_MODE))
#define IDE_SOURCE_VIEW_MODE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_SOURCE_VIEW_MODE, IdeSourceViewModeClass))

typedef struct _IdeSourceViewMode        IdeSourceViewMode;
typedef struct _IdeSourceViewModeClass   IdeSourceViewModeClass;

struct _IdeSourceViewMode
{
  GtkWidget parent;
};

struct _IdeSourceViewModeClass
{
  GtkWidgetClass parent_class;
};

gboolean     ide_source_view_mode_get_block_cursor      (IdeSourceViewMode *self);
gboolean     ide_source_view_mode_get_suppress_unbound  (IdeSourceViewMode *self);
gboolean     ide_source_view_mode_get_coalesce_undo     (IdeSourceViewMode *self);
const gchar *ide_source_view_mode_get_name              (IdeSourceViewMode *self);
const gchar *ide_source_view_mode_get_default_mode      (IdeSourceViewMode *self);
gboolean     ide_source_view_mode_get_keep_mark_on_char (IdeSourceViewMode *self);

G_END_DECLS

#endif /* IDE_SOURCE_VIEW_MODE_H */
