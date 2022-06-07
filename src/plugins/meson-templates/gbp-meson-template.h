/* gbp-meson-template.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-projects.h>

G_BEGIN_DECLS

typedef struct _GbpMesonTemplateExpansion
{
  const char         *input;
  const char         *output_pattern;
  const char * const *languages;
  gboolean            executable;
} GbpMesonTemplateExpansion;

typedef struct _GbpMesonTemplateLanguageScope
{
  const char *language;
  const char * const *extra_scope;
} GbpMesonTemplateLanguageScope;

#define GBP_TYPE_MESON_TEMPLATE (gbp_meson_template_get_type())

G_DECLARE_FINAL_TYPE (GbpMesonTemplate, gbp_meson_template, GBP, MESON_TEMPLATE, IdeProjectTemplate)

void gbp_meson_template_set_expansions     (GbpMesonTemplate                    *self,
                                            const GbpMesonTemplateExpansion     *expansions,
                                            guint                                n_expansions);
void gbp_meson_template_set_extra_scope    (GbpMesonTemplate                    *self,
                                            const char * const                  *extra_scope);
void gbp_meson_template_set_language_scope (GbpMesonTemplate                    *self,
                                            const GbpMesonTemplateLanguageScope *language_scope,
                                            guint                                n_language_scope);

G_END_DECLS
