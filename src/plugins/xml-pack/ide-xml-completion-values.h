/* ide-xml-completion-values.h
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include <libide-code.h>

#include "ide-xml-rng-define.h"
#include "ide-xml-symbol-node.h"

G_BEGIN_DECLS

typedef struct _ValueMatchItem
{
  gchar    *name;
} ValueMatchItem;

GPtrArray           *ide_xml_completion_values_get_matches       (IdeXmlRngDefine *define,
                                                                  const gchar     *values,
                                                                  const gchar     *prefix);

G_END_DECLS
