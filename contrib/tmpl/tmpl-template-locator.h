/* tmpl-template-locator.h
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

#ifndef TMPL_TEMPLATE_LOCATOR_H
#define TMPL_TEMPLATE_LOCATOR_H

#include <gio/gio.h>

#include "tmpl-template-locator.h"

G_BEGIN_DECLS

#define TMPL_TYPE_TEMPLATE_LOCATOR (tmpl_template_locator_get_type())

G_DECLARE_DERIVABLE_TYPE (TmplTemplateLocator, tmpl_template_locator, TMPL, TEMPLATE_LOCATOR, GObject)

struct _TmplTemplateLocatorClass
{
  GObjectClass parent_instance;

  GInputStream *(*locate) (TmplTemplateLocator  *self,
                           const gchar          *path,
                           GError              **error);
};

TmplTemplateLocator  *tmpl_template_locator_new                 (void);
void                  tmpl_template_locator_append_search_path  (TmplTemplateLocator  *self,
                                                                 const gchar          *path);
void                  tmpl_template_locator_prepend_search_path (TmplTemplateLocator  *self,
                                                                 const gchar          *path);
GInputStream         *tmpl_template_locator_locate              (TmplTemplateLocator  *self,
                                                                 const gchar          *path,
                                                                 GError              **error);
gchar               **tmpl_template_locator_get_search_path     (TmplTemplateLocator  *self);

G_END_DECLS

#endif /* TMPL_TEMPLATE_LOCATOR_H */
