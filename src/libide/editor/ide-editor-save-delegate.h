/* ide-editor-save-delegate.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <libpanel.h>

#include "ide-editor-page.h"

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_SAVE_DELEGATE (ide_editor_save_delegate_get_type())

G_DECLARE_FINAL_TYPE (IdeEditorSaveDelegate, ide_editor_save_delegate, IDE, EDITOR_SAVE_DELEGATE, PanelSaveDelegate)

PanelSaveDelegate *ide_editor_save_delegate_new (IdeEditorPage *page);

G_END_DECLS
