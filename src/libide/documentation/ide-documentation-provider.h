/* ide-documentation-provider.h
 *
 * Copyright © 2017 Lucie Charvat <luci.charvat@gmail.com>
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

#include <gtksourceview/gtksource.h>

#include "documentation/ide-documentation-info.h"

G_BEGIN_DECLS

#define IDE_TYPE_DOCUMENTATION_PROVIDER             (ide_documentation_provider_get_type())

G_DECLARE_INTERFACE (IdeDocumentationProvider, ide_documentation_provider, IDE, DOCUMENTATION_PROVIDER, IdeObject)

struct _IdeDocumentationProviderInterface
{
  GTypeInterface       parent_interface;

  void                    (*get_info)        (IdeDocumentationProvider    *self,
                                              IdeDocumentationInfo        *info);
  gchar                  *(*get_name)        (IdeDocumentationProvider    *self);
  IdeDocumentationContext (*get_context)     (IdeDocumentationProvider    *self);
};

gchar                  *ide_documentation_provider_get_name          (IdeDocumentationProvider    *self);
void                    ide_documentation_provider_get_info          (IdeDocumentationProvider    *self,
                                                                      IdeDocumentationInfo        *info);
IdeDocumentationContext ide_documentation_provider_get_context       (IdeDocumentationProvider    *self);


G_END_DECLS
