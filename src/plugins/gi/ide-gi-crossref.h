/* ide-gi-crossref.h
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib.h>

#include "ide-gi-blob.h"

G_BEGIN_DECLS

/* TODO: pack type */
typedef struct
{
  IdeGiBlobType type;
  guint8        is_local    : 1;
  guint8        is_resolved : 1;
  guint16       offset;
  guint32       qname;
  guint8        ns_major_version;
  guint8        ns_minor_version;
} IdeGiCrossRef;

G_STATIC_ASSERT (IS_64B_MULTIPLE (sizeof (IdeGiCrossRef)));

G_END_DECLS
