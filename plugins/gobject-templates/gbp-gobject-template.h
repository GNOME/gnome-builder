/* gbp-gobject-template.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef GBP_GOBJECT_TEMPLATE_H
#define GBP_GOBJECT_TEMPLATE_H

#include <ide.h>

#include "gbp-gobject-spec.h"

G_BEGIN_DECLS

typedef enum
{
  GBP_GOBJECT_LANGUAGE_C,
  GBP_GOBJECT_LANGUAGE_CPLUSPLUS,
  GBP_GOBJECT_LANGUAGE_VALA,
  GBP_GOBJECT_LANGUAGE_PYTHON,
} GbpGobjectLanguage;

#define GBP_TYPE_GOBJECT_TEMPLATE (gbp_gobject_template_get_type())

G_DECLARE_FINAL_TYPE (GbpGobjectTemplate, gbp_gobject_template, GBP, GOBJECT_TEMPLATE, IdeTemplateBase)

GbpGobjectTemplate *gbp_gobject_template_new           (void);
void                gbp_gobject_template_set_spec      (GbpGobjectTemplate *self,
                                                        GbpGobjectSpec     *spec);
void                gbp_gobject_template_set_directory (GbpGobjectTemplate *self,
                                                        GFile              *directory);
void                gbp_gobject_template_set_language  (GbpGobjectTemplate *self,
                                                        GbpGobjectLanguage  language);

G_END_DECLS

#endif /* GBP_GOBJECT_TEMPLATE_H */
