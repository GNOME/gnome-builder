/* ide-ctags-builder.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-code.h>

#include "ide-tags-builder.h"

G_BEGIN_DECLS

#define IDE_TYPE_CTAGS_BUILDER (ide_ctags_builder_get_type())

G_DECLARE_FINAL_TYPE (IdeCtagsBuilder, ide_ctags_builder, IDE, CTAGS_BUILDER, IdeObject)

IdeTagsBuilder *ide_ctags_builder_new (void);

G_END_DECLS
