/* ide-ctags-completion-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-completion-provider"

#include <glib/gi18n.h>

#include "ide-ctags-completion-item.h"
#include "ide-ctags-completion-provider.h"
#include "ide-debug.h"
#include "ide-macros.h"

struct _IdeCtagsCompletionProvider
{
  GObject        parent_instance;

  GPtrArray     *indexes;

  gint           minimum_word_size;
};

static void provider_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeCtagsCompletionProvider, ide_ctags_completion_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                                provider_iface_init))

void
ide_ctags_completion_provider_add_index (IdeCtagsCompletionProvider *self,
                                         IdeCtagsIndex              *index)
{
  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_return_if_fail (!index || IDE_IS_CTAGS_INDEX (index));

  g_ptr_array_add (self->indexes, g_object_ref (index));
}

static void
ide_ctags_completion_provider_finalize (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;

  g_clear_pointer (&self->indexes, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->finalize (object);
}

static void
ide_ctags_completion_provider_class_init (IdeCtagsCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ctags_completion_provider_finalize;
}

static void
ide_ctags_completion_provider_init (IdeCtagsCompletionProvider *self)
{
  self->minimum_word_size = 3;
  self->indexes = g_ptr_array_new_with_free_func (g_object_unref);
}

static gchar *
ide_ctags_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup (_("CTags"));
}

static inline gboolean
is_symbol_char (gunichar ch)
{
  switch (ch)
    {
    case '_':
      return TRUE;

    default:
      return g_unichar_isalnum (ch);
    }
}

static gchar *
get_word_to_cursor (const GtkTextIter *location)
{
  GtkTextIter iter = *location;
  GtkTextIter end = *location;

  if (!gtk_text_iter_backward_char (&end))
    return NULL;

  while (gtk_text_iter_backward_char (&iter))
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (&iter);

      if (!is_symbol_char (ch))
        break;
    }

  if (!is_symbol_char (gtk_text_iter_get_char (&iter)))
    gtk_text_iter_forward_char (&iter);

  if (gtk_text_iter_compare (&iter, &end) >= 0)
    return NULL;

  return gtk_text_iter_get_slice (&iter, location);
}

static gint
sort_wrapper (gconstpointer a,
              gconstpointer b)
{
  IdeCtagsCompletionItem * const *enta = a;
  IdeCtagsCompletionItem * const *entb = b;

  return ide_ctags_completion_item_compare (*enta, *entb);
}

static void
ide_ctags_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  g_autofree gchar *word = NULL;
  const IdeCtagsIndexEntry *entries;
  g_autoptr(GPtrArray) ar = NULL;
  gsize n_entries;
  GtkTextIter iter;
  GList *list = NULL;
  gsize i;
  gsize j;

  IDE_ENTRY;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (self->indexes->len == 0)
    IDE_GOTO (failure);

  if (!gtk_source_completion_context_get_iter (context, &iter))
    IDE_GOTO (failure);

  word = get_word_to_cursor (&iter);
  if (ide_str_empty0 (word) || strlen (word) < self->minimum_word_size)
    IDE_GOTO (failure);

  if (strlen (word) < 3)
    IDE_GOTO (failure);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  g_print ("LOOKING UP WORD: %s\n", word);

  for (j = 0; j < self->indexes->len; j++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (self->indexes, j);

      entries = ide_ctags_index_lookup_prefix (index, word, &n_entries);
      if ((entries == NULL) || (n_entries == 0))
        continue;

      for (i = 0; i < n_entries; i++)
        {
          GtkSourceCompletionProposal *item;

          item = ide_ctags_completion_item_new ((IdeCtagsIndexEntry *)&entries [i]);
          g_ptr_array_add (ar, item);
        }
    }

  g_ptr_array_sort (ar, sort_wrapper);

  for (i = ar->len; i > 0; i--)
    {
      list = g_list_prepend (list, g_ptr_array_index (ar, i - 1));
    }

failure:
  gtk_source_completion_context_add_proposals (context, provider, list, TRUE);
  g_list_free (list);

  IDE_EXIT;
}

static void
provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->get_name = ide_ctags_completion_provider_get_name;
  iface->populate = ide_ctags_completion_provider_populate;
}
