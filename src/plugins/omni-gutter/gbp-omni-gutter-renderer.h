/* gbp-omni-gutter-renderer.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GBP_TYPE_OMNI_GUTTER_RENDERER (gbp_omni_gutter_renderer_get_type())

G_DECLARE_FINAL_TYPE (GbpOmniGutterRenderer, gbp_omni_gutter_renderer, GBP, OMNI_GUTTER_RENDERER, GtkSourceGutterRenderer)

GbpOmniGutterRenderer *gbp_omni_gutter_renderer_new                            (void);
gboolean               gbp_omni_gutter_renderer_get_show_line_changes          (GbpOmniGutterRenderer *self);
gboolean               gbp_omni_gutter_renderer_get_show_line_diagnostics      (GbpOmniGutterRenderer *self);
gboolean               gbp_omni_gutter_renderer_get_show_line_numbers          (GbpOmniGutterRenderer *self);
gboolean               gbp_omni_gutter_renderer_get_show_relative_line_numbers (GbpOmniGutterRenderer *self);
gboolean               gbp_omni_gutter_renderer_get_show_line_selection_styling (GbpOmniGutterRenderer *self);
void                   gbp_omni_gutter_renderer_set_show_line_changes          (GbpOmniGutterRenderer *self,
                                                                                gboolean               show_line_changes);
void                   gbp_omni_gutter_renderer_set_show_line_diagnostics      (GbpOmniGutterRenderer *self,
                                                                                gboolean               show_line_diagnostics);
void                   gbp_omni_gutter_renderer_set_show_line_numbers          (GbpOmniGutterRenderer *self,
                                                                                gboolean               show_line_numbers);
void                   gbp_omni_gutter_renderer_set_show_relative_line_numbers (GbpOmniGutterRenderer *self,
                                                                                gboolean               show_relative_line_numbers);
void                   gbp_omni_gutter_renderer_set_show_line_selection_styling (GbpOmniGutterRenderer *self,
                                                                                 gboolean              show_line_selection_styling);

G_END_DECLS
