/* ide-ctags-completion-provider.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-completion-provider"

#include <glib/gi18n.h>
#include <libide-code.h>

#include "ide-ctags-completion-item.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-completion-provider-private.h"
#include "ide-ctags-results.h"
#include "ide-ctags-service.h"
#include "ide-ctags-util.h"

static void provider_iface_init (IdeCompletionProviderInterface *iface);

static GHashTable *reserved;

G_DEFINE_TYPE_WITH_CODE (IdeCtagsCompletionProvider,
                         ide_ctags_completion_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

static const gchar *
get_icon_name (IdeCtagsCompletionItem *item)
{
  const gchar *icon_name = NULL;

  if (item->entry == NULL)
    return NULL;

  switch (item->entry->kind)
    {
    case IDE_CTAGS_INDEX_ENTRY_CLASS_NAME:
      icon_name = "lang-class-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATOR:
      icon_name = "lang-enum-value-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME:
      icon_name = "lang-enum-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_PROTOTYPE:
    case IDE_CTAGS_INDEX_ENTRY_FUNCTION:
      icon_name = "lang-function-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_FILE_NAME:
      icon_name = "text-x-generic-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_IMPORT:
      icon_name = "lang-include-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_MEMBER:
      icon_name = "lang-struct-field-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_UNION:
      icon_name = "lang-union-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_TYPEDEF:
      icon_name = "lang-typedef-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_STRUCTURE:
      icon_name = "lang-struct-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_VARIABLE:
      icon_name = "lang-variable-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_DEFINE:
      icon_name = "lang-define-symbolic";
      break;

    case IDE_CTAGS_INDEX_ENTRY_ANCHOR:
    default:
      break;
    }

  return icon_name;
}


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
  g_autoptr(IdeCtagsService) service = NULL;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CONTEXT (context));

  if ((service = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_CTAGS_SERVICE)))
    ide_ctags_service_register_completion (service, self);
}

static void
ide_ctags_completion_provider_dispose (GObject *object)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)object;
  g_autoptr(IdeCtagsService) service = NULL;
  IdeContext *context;

  if ((context = ide_object_get_context (IDE_OBJECT (self))) &&
      (service = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_CTAGS_SERVICE)))
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
  static const gchar *reserved_keywords[] = {
    "break", "continue", "default", "do", "elif", "else", "enum", "for",
    "goto", "if", "pass", "return", "struct", "sizeof", "switch", "typedef",
    "union", "while",
  };

  object_class->dispose = ide_ctags_completion_provider_dispose;
  object_class->finalize = ide_ctags_completion_provider_finalize;

  reserved = g_hash_table_new (g_str_hash, g_str_equal);
  for (guint i = 0; i < G_N_ELEMENTS (reserved_keywords); i++)
    g_hash_table_insert (reserved, (gchar *)reserved_keywords[i], NULL);
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
ide_ctags_completion_provider_populate_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeCtagsResults *results = (IdeCtagsResults *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CTAGS_RESULTS (results));

  if (!ide_ctags_results_populate_finish (results, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, g_object_ref (results));
}

static void
ide_ctags_completion_provider_populate_async (IdeCompletionProvider  *provider,
                                              IdeCompletionContext   *context,
                                              GCancellable           *cancellable,
                                              GAsyncReadyCallback     callback,
                                              gpointer                user_data)
{
  IdeCtagsCompletionProvider *self = (IdeCtagsCompletionProvider *)provider;
  g_autoptr(IdeCtagsResults) model = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *word = NULL;
  GtkTextIter begin, end;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_ctags_completion_provider_populate_async);

  if (!self->enabled)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Completion not currently enabled");
      return;
    }

  ide_completion_context_get_bounds (context, &begin, &end);
  word = gtk_text_iter_get_slice (&begin, &end);

  model = ide_ctags_results_new ();
  ide_ctags_results_set_suffixes (model, get_allowed_suffixes (context));
  ide_ctags_results_set_word (model, word);
  for (guint i = 0; i < self->indexes->len; i++)
    ide_ctags_results_add_index (model, g_ptr_array_index (self->indexes, i));

  ide_ctags_results_populate_async (model,
                                    cancellable,
                                    ide_ctags_completion_provider_populate_cb,
                                    g_steal_pointer (&task));
}

static GListModel *
ide_ctags_completion_provider_populate_finish (IdeCompletionProvider  *provider,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  g_return_val_if_fail (IDE_IS_CTAGS_COMPLETION_PROVIDER (provider), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static gint
ide_ctags_completion_provider_get_priority (IdeCompletionProvider *provider,
                                            IdeCompletionContext  *context)
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
  IdeCtagsCompletionItem *item = (IdeCtagsCompletionItem *)proposal;
  g_autofree gchar *slice = NULL;
  g_autoptr(IdeSnippet) snippet = NULL;
  IdeFileSettings *file_settings;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_CTAGS_COMPLETION_ITEM (item));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));

  ide_completion_context_get_bounds (context, &begin, &end);

  view = ide_completion_context_get_view (context);
  g_assert (IDE_IS_SOURCE_VIEW (view));

  buffer = ide_completion_context_get_buffer (context);
  g_assert (IDE_IS_BUFFER (buffer));

  file_settings = ide_buffer_get_file_settings (IDE_BUFFER (buffer));
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

static gboolean
ide_ctags_completion_provider_refilter (IdeCompletionProvider *self,
                                        IdeCompletionContext  *context,
                                        GListModel            *model)
{
  g_autofree gchar *word = NULL;
  GtkTextIter begin, end;

  g_assert (IDE_IS_CTAGS_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_CTAGS_RESULTS (model));

  ide_completion_context_get_bounds (context, &begin, &end);
  word = gtk_text_iter_get_slice (&begin, &end);
  ide_ctags_results_set_word (IDE_CTAGS_RESULTS (model), word);
  ide_ctags_results_refilter (IDE_CTAGS_RESULTS (model));

  return TRUE;
}

static void
ide_ctags_completion_provider_display_proposal (IdeCompletionProvider   *provider,
                                                IdeCompletionListBoxRow *row,
                                                IdeCompletionContext    *context,
                                                const gchar             *typed_text,
                                                IdeCompletionProposal   *proposal)
{
  IdeCtagsCompletionItem *item = IDE_CTAGS_COMPLETION_ITEM (proposal);
  g_autofree gchar *highlight = NULL;

  highlight = ide_completion_fuzzy_highlight (item->entry->name, typed_text);

  ide_completion_list_box_row_set_icon_name (row, get_icon_name (item));
  ide_completion_list_box_row_set_left (row, NULL);
  ide_completion_list_box_row_set_center_markup (row, highlight);
  ide_completion_list_box_row_set_right (row, NULL);
}


static void
provider_iface_init (IdeCompletionProviderInterface *iface)
{
  iface->load = ide_ctags_completion_provider_load;
  iface->get_priority = ide_ctags_completion_provider_get_priority;
  iface->activate_proposal = ide_ctags_completion_provider_activate_proposal;
  iface->populate_async = ide_ctags_completion_provider_populate_async;
  iface->populate_finish = ide_ctags_completion_provider_populate_finish;
  iface->refilter = ide_ctags_completion_provider_refilter;
  iface->display_proposal = ide_ctags_completion_provider_display_proposal;
}
