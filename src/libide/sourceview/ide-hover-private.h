/* ide-hover-private.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-source-view.h"

G_BEGIN_DECLS

#define IDE_TYPE_HOVER (ide_hover_get_type())

G_DECLARE_FINAL_TYPE (IdeHover, ide_hover, IDE, HOVER, GObject)

IdeHover *_ide_hover_new          (IdeSourceView     *view);
void      _ide_hover_display      (IdeHover          *self,
                                   const GtkTextIter *iter);
void      _ide_hover_set_context  (IdeHover          *self,
                                   IdeContext        *context);
void      _ide_hover_set_language (IdeHover          *self,
                                   const gchar       *language);

G_END_DECLS
