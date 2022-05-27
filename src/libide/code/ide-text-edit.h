/* ide-text-edit.h
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_TEXT_EDIT (ide_text_edit_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTextEdit, ide_text_edit, IDE, TEXT_EDIT, IdeObject)

struct _IdeTextEditClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
IdeTextEdit *ide_text_edit_new              (IdeRange    *range,
                                             const gchar *text);
IDE_AVAILABLE_IN_ALL
IdeTextEdit *ide_text_edit_new_from_variant (GVariant    *variant);
IDE_AVAILABLE_IN_ALL
const gchar *ide_text_edit_get_text         (IdeTextEdit *self);
IDE_AVAILABLE_IN_ALL
void         ide_text_edit_set_text         (IdeTextEdit *self,
                                             const gchar *text);
IDE_AVAILABLE_IN_ALL
IdeRange    *ide_text_edit_get_range        (IdeTextEdit *self);
IDE_AVAILABLE_IN_ALL
void         ide_text_edit_set_range        (IdeTextEdit *self,
                                             IdeRange    *range);
IDE_AVAILABLE_IN_ALL
GVariant    *ide_text_edit_to_variant       (IdeTextEdit *self);

G_END_DECLS
