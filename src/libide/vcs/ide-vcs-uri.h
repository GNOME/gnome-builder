/* ide-vcs-uri.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_VCS_URI (ide_vcs_uri_get_type())

typedef struct _IdeVcsUri IdeVcsUri;

GType        ide_vcs_uri_get_type   (void);
IdeVcsUri   *ide_vcs_uri_new        (const gchar     *uri);
IdeVcsUri   *ide_vcs_uri_ref        (IdeVcsUri       *self);
void         ide_vcs_uri_unref      (IdeVcsUri       *self);
const gchar *ide_vcs_uri_get_scheme (const IdeVcsUri *self);
const gchar *ide_vcs_uri_get_user   (const IdeVcsUri *self);
const gchar *ide_vcs_uri_get_host   (const IdeVcsUri *self);
guint        ide_vcs_uri_get_port   (const IdeVcsUri *self);
const gchar *ide_vcs_uri_get_path   (const IdeVcsUri *self);
void         ide_vcs_uri_set_scheme (IdeVcsUri       *self,
                                     const gchar     *scheme);
void         ide_vcs_uri_set_user   (IdeVcsUri       *self,
                                     const gchar     *user);
void         ide_vcs_uri_set_host   (IdeVcsUri       *self,
                                     const gchar     *host);
void         ide_vcs_uri_set_port   (IdeVcsUri       *self,
                                     guint            port);
void         ide_vcs_uri_set_path   (IdeVcsUri       *self,
                                     const gchar     *path);
gchar       *ide_vcs_uri_to_string  (const IdeVcsUri *self);
gboolean     ide_vcs_uri_is_valid   (const gchar     *uri_string);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeVcsUri, ide_vcs_uri_unref)

G_END_DECLS
