/* ide-vcs-uri.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#if !defined (IDE_VCS_INSIDE) && !defined (IDE_VCS_COMPILATION)
# error "Only <libide-vcs.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_VCS_URI (ide_vcs_uri_get_type())

typedef struct _IdeVcsUri IdeVcsUri;

IDE_AVAILABLE_IN_ALL
GType        ide_vcs_uri_get_type       (void);
IDE_AVAILABLE_IN_ALL
IdeVcsUri   *ide_vcs_uri_new            (const gchar     *uri);
IDE_AVAILABLE_IN_ALL
IdeVcsUri   *ide_vcs_uri_ref            (IdeVcsUri       *self);
IDE_AVAILABLE_IN_ALL
void         ide_vcs_uri_unref          (IdeVcsUri       *self);
IDE_AVAILABLE_IN_ALL
const gchar *ide_vcs_uri_get_scheme     (const IdeVcsUri *self);
IDE_AVAILABLE_IN_ALL
const gchar *ide_vcs_uri_get_user       (const IdeVcsUri *self);
IDE_AVAILABLE_IN_ALL
const gchar *ide_vcs_uri_get_host       (const IdeVcsUri *self);
IDE_AVAILABLE_IN_ALL
guint        ide_vcs_uri_get_port       (const IdeVcsUri *self);
IDE_AVAILABLE_IN_ALL
const gchar *ide_vcs_uri_get_path       (const IdeVcsUri *self);
IDE_AVAILABLE_IN_ALL
void         ide_vcs_uri_set_scheme     (IdeVcsUri       *self,
                                         const gchar     *scheme);
IDE_AVAILABLE_IN_ALL
void         ide_vcs_uri_set_user       (IdeVcsUri       *self,
                                         const gchar     *user);
IDE_AVAILABLE_IN_ALL
void         ide_vcs_uri_set_host       (IdeVcsUri       *self,
                                         const gchar     *host);
IDE_AVAILABLE_IN_ALL
void         ide_vcs_uri_set_port       (IdeVcsUri       *self,
                                         guint            port);
IDE_AVAILABLE_IN_ALL
void         ide_vcs_uri_set_path       (IdeVcsUri       *self,
                                         const gchar     *path);
IDE_AVAILABLE_IN_ALL
gchar       *ide_vcs_uri_to_string      (const IdeVcsUri *self);
IDE_AVAILABLE_IN_ALL
gboolean     ide_vcs_uri_is_valid       (const gchar     *uri_string);
IDE_AVAILABLE_IN_ALL
gchar       *ide_vcs_uri_get_clone_name (const IdeVcsUri *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeVcsUri, ide_vcs_uri_unref)

G_END_DECLS
