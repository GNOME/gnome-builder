/* ide-snippet-completion-provider.c
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-snippet-completion-provider.h"

#include "ide-snippet-completion-provider.h"

struct _IdeSnippetCompletionProvider
{
  IdeObject parent_instance;
};

G_DEFINE_TYPE (IdeSnippetCompletionProvider, ide_snippet_completion_provider, IDE_TYPE_OBJECT)

static void
ide_snippet_completion_provider_finalize (GObject *object)
{
  IdeSnippetCompletionProvider *self = (IdeSnippetCompletionProvider *)object;

  G_OBJECT_CLASS (ide_snippet_completion_provider_parent_class)->finalize (object);
}

static void
ide_snippet_completion_provider_class_init (IdeSnippetCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_snippet_completion_provider_finalize;
}

static void
ide_snippet_completion_provider_init (IdeSnippetCompletionProvider *self)
{
}

void
ide_snippet_completion_provider_set_language (IdeSnippetCompletionProvider *self,
                                              const gchar                  *lang_id)
{
  g_return_if_fail (IDE_IS_SNIPPET_COMPLETION_PROVIDER (self));

  ide_snippet_model_set_language (self->model, lang_id);
}
