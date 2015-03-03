/* ide-source-view.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_SOURCE_VIEW_H
#define IDE_SOURCE_VIEW_H

#include <gtksourceview/gtksource.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW            (ide_source_view_get_type())
#define IDE_SOURCE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_VIEW, IdeSourceView))
#define IDE_SOURCE_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_VIEW, IdeSourceView const))
#define IDE_SOURCE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_SOURCE_VIEW, IdeSourceViewClass))
#define IDE_IS_SOURCE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_SOURCE_VIEW))
#define IDE_IS_SOURCE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_SOURCE_VIEW))
#define IDE_SOURCE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_SOURCE_VIEW, IdeSourceViewClass))

typedef struct _IdeSourceView      IdeSourceView;
typedef struct _IdeSourceViewClass IdeSourceViewClass;

/**
 * IdeSourceViewModeType:
 * @IDE_SOURCE_VIEW_MODE_TRANSIENT: Transient
 * @IDE_SOURCE_VIEW_MODE_PERMANENT: Permanent
 * @IDE_SOURCE_VIEW_MODE_MODAL: Modal
 *
 * The type of keyboard mode.
 */
typedef enum
{
  IDE_SOURCE_VIEW_MODE_TYPE_TRANSIENT,
  IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT,
  IDE_SOURCE_VIEW_MODE_TYPE_MODAL
} IdeSourceViewModeType;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeSourceView, g_object_unref)

struct _IdeSourceView
{
  GtkSourceView parent;
};

struct _IdeSourceViewClass
{
  GtkSourceViewClass parent_class;

  void (*jump)         (IdeSourceView           *self,
                        const GtkTextIter       *location);
  void (*pop_snippet)  (IdeSourceView           *self,
                        IdeSourceSnippet        *snippet);
  void (*push_snippet) (IdeSourceView           *self,
                        IdeSourceSnippet        *snippet,
                        IdeSourceSnippetContext *context,
                        const GtkTextIter       *location);
  void (*set_mode)     (IdeSourceView           *self,
                        const gchar             *mode,
                        IdeSourceViewModeType    type);
};

void                        ide_source_view_clear_snippets            (IdeSourceView              *self);
IdeBackForwardList         *ide_source_view_get_back_forward_list     (IdeSourceView              *self);
const PangoFontDescription *ide_source_view_get_font_desc             (IdeSourceView              *self);
gboolean                    ide_source_view_get_insert_matching_brace (IdeSourceView              *self);
gboolean                    ide_source_view_get_overwrite_braces      (IdeSourceView              *self);
gboolean                    ide_source_view_get_show_grid_lines       (IdeSourceView              *self);
gboolean                    ide_source_view_get_show_line_changes     (IdeSourceView              *self);
gboolean                    ide_source_view_get_snippet_completion    (IdeSourceView              *self);
GType                       ide_source_view_get_type                  (void);
void                        ide_source_view_pop_snippet               (IdeSourceView              *self);
void                        ide_source_view_push_snippet              (IdeSourceView              *self,
                                                                       IdeSourceSnippet           *snippet);
void                        ide_source_view_set_font_desc             (IdeSourceView              *self,
                                                                       const PangoFontDescription *font_desc);
void                        ide_source_view_set_font_name             (IdeSourceView              *self,
                                                                       const gchar                *font_name);
void                        ide_source_view_set_insert_matching_brace (IdeSourceView              *self,
                                                                       gboolean                    insert_matching_brace);
void                        ide_source_view_set_overwrite_braces      (IdeSourceView              *self,
                                                                       gboolean                    overwrite_braces);
void                        ide_source_view_set_show_grid_lines       (IdeSourceView              *self,
                                                                       gboolean                    show_grid_lines);
void                        ide_source_view_set_show_line_changes     (IdeSourceView              *self,
                                                                       gboolean                    show_line_changes);
void                        ide_source_view_set_snippet_completion    (IdeSourceView              *self,
                                                                       gboolean                    snippet_completion);
void                        ide_source_view_set_back_forward_list     (IdeSourceView              *self,
                                                                       IdeBackForwardList         *back_forward_list);

G_END_DECLS

#endif /* IDE_SOURCE_VIEW_H */
