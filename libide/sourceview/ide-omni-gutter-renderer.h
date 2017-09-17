/* ide-omni-gutter-renderer.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_OMNI_GUTTER_RENDERER_H
#define IDE_OMNI_GUTTER_RENDERER_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define IDE_TYPE_OMNI_GUTTER_RENDERER (ide_omni_gutter_renderer_get_type())

G_DECLARE_FINAL_TYPE (IdeOmniGutterRenderer, ide_omni_gutter_renderer, IDE, OMNI_GUTTER_RENDERER, GtkSourceGutterRenderer)

IdeOmniGutterRenderer *ide_omni_gutter_renderer_new                       (void);
gboolean               ide_omni_gutter_renderer_get_show_line_changes     (IdeOmniGutterRenderer *self);
gboolean               ide_omni_gutter_renderer_get_show_line_diagnostics (IdeOmniGutterRenderer *self);
gboolean               ide_omni_gutter_renderer_get_show_line_numbers     (IdeOmniGutterRenderer *self);
void                   ide_omni_gutter_renderer_set_show_line_changes     (IdeOmniGutterRenderer *self,
                                                                           gboolean               show_line_changes);
void                   ide_omni_gutter_renderer_set_show_line_diagnostics (IdeOmniGutterRenderer *self,
                                                                           gboolean               show_line_diagnostics);
void                   ide_omni_gutter_renderer_set_show_line_numbers     (IdeOmniGutterRenderer *self,
                                                                           gboolean               show_line_numbers);

G_END_DECLS

#endif /* IDE_OMNI_GUTTER_RENDERER_H */
