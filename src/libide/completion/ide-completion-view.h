/* ide-completion-view.h
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
 */

#pragma once

#include <dazzle.h>

#include "ide-version-macros.h"

#include "completion/ide-completion-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_VIEW (ide_completion_view_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeCompletionView, ide_completion_view, IDE, COMPLETION_VIEW, DzlBin)

IDE_AVAILABLE_IN_3_30
IdeCompletionContext *ide_completion_view_get_context (IdeCompletionView    *self);
IDE_AVAILABLE_IN_3_30
void                  ide_completion_view_set_context (IdeCompletionView    *self,
                                                       IdeCompletionContext *context);

G_END_DECLS
