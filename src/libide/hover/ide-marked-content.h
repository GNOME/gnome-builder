/* ide-marked-content.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <glib-object.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_MARKED_CONTENT (ide_marked_content_get_type())

typedef struct _IdeMarkedContent IdeMarkedContent;

typedef enum
{
  IDE_MARKED_KIND_PLAINTEXT = 0,
  IDE_MARKED_KIND_MARKDOWN  = 1,
  IDE_MARKED_KIND_HTML      = 2,
  IDE_MARKED_KIND_PANGO     = 3,
} IdeMarkedKind;

IDE_AVAILABLE_IN_3_30
GType             ide_marked_content_get_type      (void);
IDE_AVAILABLE_IN_3_30
IdeMarkedContent *ide_marked_content_new           (GBytes           *content,
                                                    IdeMarkedKind     kind);
IDE_AVAILABLE_IN_3_30
IdeMarkedContent *ide_marked_content_new_plaintext (const gchar      *plaintext);
IDE_AVAILABLE_IN_3_30
IdeMarkedContent *ide_marked_content_new_from_data (const gchar      *data,
                                                    gssize            len,
                                                    IdeMarkedKind     kind);
IDE_AVAILABLE_IN_3_30
GBytes           *ide_marked_content_get_bytes     (IdeMarkedContent *self);
IDE_AVAILABLE_IN_3_30
IdeMarkedKind     ide_marked_content_get_kind      (IdeMarkedContent *self);
IDE_AVAILABLE_IN_3_30
gchar            *ide_marked_content_as_string     (IdeMarkedContent *self);
IDE_AVAILABLE_IN_3_30
IdeMarkedContent *ide_marked_content_ref           (IdeMarkedContent *self);
IDE_AVAILABLE_IN_3_30
void              ide_marked_content_unref         (IdeMarkedContent *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeMarkedContent, ide_marked_content_unref)

G_END_DECLS
