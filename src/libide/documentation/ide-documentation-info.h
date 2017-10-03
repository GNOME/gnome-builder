/* ide-documentation-info.h
 *
 * Copyright (C) 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#include "documentation/ide-documentation-proposal.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCUMENTATION_INFO (ide_documentation_info_get_type())

G_DECLARE_FINAL_TYPE (IdeDocumentationInfo, ide_documentation_info, IDE, DOCUMENTATION_INFO, GObject)

typedef enum {
  IDE_DOCUMENTATION_CONTEXT_NONE,
  IDE_DOCUMENTATION_CONTEXT_CARD_C,
  IDE_DOCUMENTATION_CONTEXT_LAST,
} IdeDocumentationContext;

IdeDocumentationInfo     *ide_documentation_info_new            (const gchar                 *input,
                                                                 IdeDocumentationContext      context);
void                      ide_documentation_info_take_proposal  (IdeDocumentationInfo        *self,
                                                                 IdeDocumentationProposal    *proposal);
IdeDocumentationContext   ide_documentation_info_get_context    (IdeDocumentationInfo        *self);
gchar                    *ide_documentation_info_get_input      (IdeDocumentationInfo        *self);
guint                     ide_documentation_info_get_size       (IdeDocumentationInfo        *self);
IdeDocumentationProposal *ide_documentation_info_get_proposal   (IdeDocumentationInfo        *self,
                                                                 guint                        index);

G_END_DECLS
