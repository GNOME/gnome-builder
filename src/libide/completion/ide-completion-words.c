/* ide-completion-words.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-completion-words"

#include "config.h"

#include "completion/ide-completion-provider.h"
#include "completion/ide-completion-words.h"

struct _IdeCompletionWords
{
  GtkSourceCompletionWords parent_instance;
};

static void completion_provider_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeCompletionWords, ide_completion_words, GTK_SOURCE_TYPE_COMPLETION_WORDS,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_init))

static void
ide_completion_words_class_init (IdeCompletionWordsClass *klass)
{
}

static void
ide_completion_words_init (IdeCompletionWords *self)
{
}

static gboolean
ide_completion_words_match (GtkSourceCompletionProvider *provider,
                            GtkSourceCompletionContext  *context)
{
  GtkSourceCompletionActivation activation;
  GtkTextIter iter;

  g_assert (IDE_IS_COMPLETION_WORDS (provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  activation = gtk_source_completion_context_get_activation (context);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
    {
      if (ide_completion_provider_context_in_comment (context))
        return FALSE;
    }

  if (!gtk_source_completion_context_get_iter (context, &iter))
    return FALSE;

  if (gtk_text_iter_backward_char (&iter))
    {
      gunichar ch = gtk_text_iter_get_char (&iter);

      if (!g_unichar_isalnum (ch) && ch != '_')
        return FALSE;
    }

  return TRUE;
}

static void
completion_provider_init (GtkSourceCompletionProviderIface *iface)
{
  iface->match = ide_completion_words_match;
}
