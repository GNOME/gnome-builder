/* ide-snippet-completion-provider.h
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

#include "completion/ide-completion-provider.h"
#include "snippets/ide-snippet.h"

G_BEGIN_DECLS

#define IDE_TYPE_SNIPPET_COMPLETION_PROVIDER (ide_snippet_completion_provider_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeSnippetCompletionProvider, ide_snippet_completion_provider, IDE, SNIPPET_COMPLETION_PROVIDER, IdeObject)

IDE_AVAILABLE_IN_3_30
IdeSnippetCompletionProvider *ide_snippet_completion_provider_new          (void);
IDE_AVAILABLE_IN_3_30
void                          ide_snippet_completion_provider_set_language (IdeSnippetCompletionProvider *self,
                                                                            const gchar                  *lang_id);

G_END_DECLS
