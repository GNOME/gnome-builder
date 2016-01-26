/* tmpl-template.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (TMPL_GLIB_INSIDE) && !defined (TMPL_GLIB_COMPILATION)
# error "Only <tmpl-glib.h> can be included directly."
#endif

#ifndef TMPL_TEMPLATE_H
#define TMPL_TEMPLATE_H

#include <gio/gio.h>

#include "tmpl-scope.h"
#include "tmpl-template-locator.h"

G_BEGIN_DECLS

#define TMPL_TYPE_TEMPLATE (tmpl_template_get_type())

G_DECLARE_DERIVABLE_TYPE (TmplTemplate, tmpl_template, TMPL, TEMPLATE, GObject)

struct _TmplTemplateClass
{
  GObjectClass parent_class;
};

TmplTemplate        *tmpl_template_new            (TmplTemplateLocator  *locator);
TmplTemplateLocator *tmpl_template_get_locator    (TmplTemplate         *self);
void                 tmpl_template_set_locator    (TmplTemplate         *self,
                                                   TmplTemplateLocator  *locator);
gboolean             tmpl_template_parse_file     (TmplTemplate         *self,
                                                   GFile                *file,
                                                   GCancellable         *cancellable,
                                                   GError              **error);
gboolean             tmpl_template_parse_resource (TmplTemplate         *self,
                                                   const gchar          *path,
                                                   GCancellable         *cancellable,
                                                   GError              **error);
gboolean             tmpl_template_parse_path     (TmplTemplate         *self,
                                                   const gchar          *path,
                                                   GCancellable         *cancellable,
                                                   GError              **error);
gboolean             tmpl_template_parse_string   (TmplTemplate         *self,
                                                   const gchar          *input,
                                                   GError              **error);
gboolean             tmpl_template_parse          (TmplTemplate         *self,
                                                   GInputStream         *stream,
                                                   GCancellable         *cancellable,
                                                   GError              **error);
gboolean             tmpl_template_expand         (TmplTemplate         *self,
                                                   GOutputStream        *stream,
                                                   TmplScope            *scope,
                                                   GCancellable         *cancellable,
                                                   GError              **error);
gchar               *tmpl_template_expand_string  (TmplTemplate         *self,
                                                   TmplScope            *scope,
                                                   GError              **error);

G_END_DECLS

#endif /* TMPL_TEMPLATE_H */
