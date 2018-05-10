/* ide-ctags-completion-provider.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "sourceview/ide-source-iter.h"

#include "ide-ctags-completion-item.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-completion-provider-private.h"
#include "ide-ctags-service.h"
#include "ide-ctags-util.h"

static void provider_iface_init (IdeCompletionProviderInterface *iface);

static GHashTable *reserved;

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCtagsCompletionProvider,
                                ide_ctags_completion_provider,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

void
ide_ctags_completion_provider_add_index (IdeCtagsCompletionProvider *self,
                                         IdeCtagsIndex              *index)
{
  GFile *file;

  g_return_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_return_if_fail (!index || IDE_IS_CTAGS_INDEX (index));
  g_return_if_fail (self->indexes != NULL);

  file = ide_ctags_index_get_file (index);

  for (guint i = 0; i < self->indexes->len; i++)
    {
      IdeCtagsIndex *item = g_ptr_array_index (self->indexes, i);
      GFile *item_file = ide_ctags_index_get_file (item);

      if (g_file_equal (item_file, file))
        {
          g_ptr_array_remove_index_fast (self->indexes, i);
          g_ptr_array_add (self->indexes, g_object_ref (index));
          return;
        }
    }

  g_ptr_array_add (self->indexes, g_object_ref (index));
}

static void
ide_ctags_completion_provider_load (IdeCompletionProvider *provider,
                                    IdeContext            *context)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  IdeCtagsService *service;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CONTEXT (context));

  service = ide_context_get_service_typed (context, IDE_TYPE_CTAGS_SERVICE);
  ide_ctags_service_register_completion (service, self);
}

static void
ide_ctags_completion_provider_dispose (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;
  IdeContext *context;
  IdeCtagsService *service;

  if ((context = ide_object_get_context (IDE_OBJECT (self))) &&
      (service = ide_context_get_service_typed (context, IDE_TYPE_CTAGS_SERVICE)))
    ide_ctags_service_unregister_completion (service, self);

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->dispose (object);
}

static void
ide_ctags_completion_provider_finalize (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;

  g_clear_pointer (&self->indexes, g_ptr_array_unref);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_ctags_completion_provider_parent_class)->finalize (object);
}

static void
ide_ctags_completion_provider_class_init (IdeCtagsCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_ctags_completion_provider_dispose;
  object_class->finalize = ide_ctags_completion_provider_finalize;

  reserved = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (reserved, "break", NULL);
  g_hash_table_insert (reserved, "continue", NULL);
  g_hash_table_insert (reserved, "default", NULL);
  g_hash_table_insert (reserved, "do", NULL);
  g_hash_table_insert (reserved, "elif", NULL);
  g_hash_table_insert (reserved, "else", NULL);
  g_hash_table_insert (reserved, "enum", NULL);
  g_hash_table_insert (reserved, "for", NULL);
  g_hash_table_insert (reserved, "goto", NULL);
  g_hash_table_insert (reserved, "if", NULL);
  g_hash_table_insert (reserved, "pass", NULL);
  g_hash_table_insert (reserved, "return", NULL);
  g_hash_table_insert (reserved, "struct", NULL);
  g_hash_table_insert (reserved, "sizeof", NULL);
  g_hash_table_insert (reserved, "switch", NULL);
  g_hash_table_insert (reserved, "typedef", NULL);
  g_hash_table_insert (reserved, "union", NULL);
  g_hash_table_insert (reserved, "while", NULL);
}

static void
ide_ctags_completion_provider_class_finalize (IdeCtagsCompletionProviderClass *klass)
{
}

static void
changed_enabled_cb (IdeCtagsCompletionProvider *self,
                    const gchar                *key,
                    GSettings                  *settings)
{
  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (G_IS_SETTINGS (settings));

  self->enabled = g_settings_get_boolean (settings, "ctags-autocompletion");
}

static void
ide_ctags_completion_provider_init (IdeCtagsCompletionProvider *self)
{
  self->minimum_word_size = 3;
  self->indexes = g_ptr_array_new_with_free_func (g_object_unref);
  self->settings = g_settings_new ("org.gnome.builder.code-insight");

  g_signal_connect_object (self->settings,
                           "changed::ctags-autocompletion",
                           G_CALLBACK (changed_enabled_cb),
                           self,
                           G_CONNECT_SWAPPED);

  changed_enabled_cb (self, NULL, self->settings);
}

static const gchar * const *
get_allowed_suffixes (IdeCompletionContext *context)
{
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;
  const gchar *lang_id = NULL;

  g_assert (IDE_IS_COMPLETION_CONTEXT (context));

  buffer = ide_completion_context_get_buffer (context);
  if ((language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    lang_id = gtk_source_language_get_id (language);

  return ide_ctags_get_allowed_suffixes (lang_id);
}

static void
ide_ctags_completion_provider_populate_async (IdeCompletionProvider  *provider,
                                              IdeCompletionContext   *context,
                                              GCancellable           *cancellable,
                                              GListModel            **results,
                                              GAsyncReadyCallback     callback,
                                              gpointer                user_data)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  const gchar * const *allowed;
  g_autofree gchar *casefold = NULL;
  g_autofree gchar *word = NULL;
  g_autoptr(GHashTable) completions = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GListStore) store = NULL;
  GtkTextIter begin, end;
  gint word_len;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (results != NULL);

  *results = NULL;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_ctags_completion_provider_populate_async);

  if (!self->enabled ||
      !ide_completion_context_get_bounds (context, &begin, &end) ||
      !(word = gtk_text_iter_get_slice (&begin, &end)) ||
      !(word_len = strlen (word)) ||
      strlen (word) < self->minimum_word_size)
    goto word_too_small;

  allowed = get_allowed_suffixes (context);
  casefold = g_utf8_casefold (word, -1);
  store = g_list_store_new (IDE_TYPE_COMPLETION_PROPOSAL);
  completions = g_hash_table_new (g_str_hash, g_str_equal);

  for (guint i = 0; i < self->indexes->len; i++)
    {
      g_autofree gchar *copy = g_strdup (word);
      IdeCtagsIndex *index = g_ptr_array_index (self->indexes, i);
      const IdeCtagsIndexEntry *entries = NULL;
      guint tmp_len = word_len;
      gsize n_entries = 0;
      gchar gdata_key[64];

      /* NOTE: "ctags-%d" is turned into a GQuark and therefore lives the
       *       length of the process. But it's generally under 1000 so not a
       *       really big deal for now.
       */

      /*
       * Make sure we hold a reference to the index for the lifetime of the results.
       * When the results are released, so could our indexes.
       */
      g_snprintf (gdata_key, sizeof gdata_key, "ctags-%d", i);
      g_object_set_data_full (G_OBJECT (store), gdata_key,
                              g_object_ref (index), g_object_unref);

      while (entries == NULL && copy[0])
        {
          if (!(entries = ide_ctags_index_lookup_prefix (index, copy, &n_entries)))
            copy [--tmp_len] = '\0';
        }

      if ((entries == NULL) || (n_entries == 0))
        continue;

      for (guint j = 0; j < n_entries; j++)
        {
          const IdeCtagsIndexEntry *entry = &entries [j];
          guint priority;

          if (g_hash_table_contains (completions, entry->name))
            continue;

          g_hash_table_add (completions, (gchar *)entry->name);

          if (!ide_ctags_is_allowed (entry, allowed))
            continue;

          if (ide_completion_item_fuzzy_match (entry->name, casefold, &priority))
            {
              g_autoptr(IdeCtagsCompletionItem) item = NULL;

              item = ide_ctags_completion_item_new (self, entry);
              g_list_store_append (store, item);
            }
        }
    }

  *results = g_object_ref (G_LIST_MODEL (store));

  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);
  return;

word_too_small:
  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Word too small to query ctags");
  return;
}

static GListModel *
ide_ctags_completion_provider_populate_finish (IdeCompletionProvider  *provider,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  g_return_val_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (provider), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static gint
ide_ctags_completion_provider_get_priority (IdeCompletionProvider *provider)
{
  return IDE_CTAGS_COMPLETION_PROVIDER_PRIORITY;
}

static gboolean
is_reserved_word (const gchar *word)
{
  /* TODO: Check by language */
  return g_hash_table_contains (reserved, word);
}

static void
ide_ctags_completion_provider_activate_proposal (IdeCompletionProvider *provider,
                                                 IdeCompletionContext  *context,
                                                 IdeCompletionProposal *proposal,
                                                 const GdkEventKey     *key)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  IdeCtagsCompletionItem *item = (IdeCtagsCompletionItem *)proposal;
  g_autofree gchar *slice = NULL;
  g_autoptr(IdeSourceSnippet) snippet = NULL;
  IdeFileSettings *file_settings = NULL;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  GtkTextIter begin;
  GtkTextIter end;
  IdeFile *file;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CTAGS_COMPLETION_ITEM (item));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));

  ide_completion_context_get_bounds (context, &begin, &end);

  view = ide_completion_context_get_view (context);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  buffer = ide_completion_context_get_buffer (context);
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  g_assert (IDE_IS_FILE (file));

  file_settings = ide_file_peek_settings (file);
  g_assert (!file_settings || IDE_IS_FILE_SETTINGS (file_settings));

  slice = gtk_text_iter_get_slice (&begin, &end);

  if (is_reserved_word (slice))
    return;

  snippet = ide_ctags_completion_item_get_snippet (item, file_settings);

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &begin, &end);
  ide_source_view_push_snippet (IDE_SOURCE_VIEW (view), snippet, &begin);
  gtk_text_buffer_end_user_action (buffer);
}

static void
provider_iface_init (IdeCompletionProviderInterface *iface)
{
  iface->load = ide_ctags_completion_provider_load;
  iface->get_priority = ide_ctags_completion_provider_get_priority;
  iface->activate_proposal = ide_ctags_completion_provider_activate_proposal;
  iface->populate_async = ide_ctags_completion_provider_populate_async;
  iface->populate_finish = ide_ctags_completion_provider_populate_finish;
}

void
_ide_ctags_completion_provider_register_type (GTypeModule *module)
{
  ide_ctags_completion_provider_register_type (module);
}
