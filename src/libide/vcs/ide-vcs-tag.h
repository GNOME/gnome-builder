/* ide-vcs-tag.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_VCS_TAG (ide_vcs_tag_get_type ())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeVcsTag, ide_vcs_tag, IDE, VCS_TAG, GObject)

struct _IdeVcsTagInterface
{
  GTypeInterface parent;

  char *(*dup_name) (IdeVcsTag *self);
};

IDE_AVAILABLE_IN_ALL
char *ide_vcs_tag_dup_name (IdeVcsTag *self);

G_END_DECLS
