/* ide-snippet-completion-provider.c
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

#define G_LOG_DOMAIN "ide-snippet-completion-provider.h"

#include "config.h"

#include "ide-snippet-completion-provider.h"
#include "ide-snippet-completion-item.h"
#include "ide-snippet-model.h"

struct _IdeSnippetCompletionProvider
{
  IdeObject        parent_instance;
  IdeSnippetModel *model;
};

static void provider_iface_init (IdeCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeSnippetCompletionProvider,
                         ide_snippet_completion_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

static void
ide_snippet_completion_provider_finalize (GObject *object)
{
  IdeSnippetCompletionProvider *self = (IdeSnippetCompletionProvider *)object;

  g_clear_object (&self->model);

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

static void
ide_snippet_completion_provider_load (IdeCompletionProvider *provider,
                                      IdeContext            *context)
{
  IdeSnippetCompletionProvider *self = (IdeSnippetCompletionProvider *)provider;
  IdeSnippetStorage *storage;

  g_assert (IDE_IS_SNIPPET_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_CONTEXT (context));

  storage = ide_snippet_storage_from_context (context);
  self->model = ide_snippet_model_new (storage);
}

static gint
ide_snippet_completion_provider_get_priority (IdeCompletionProvider *provider,
                                              IdeCompletionContext  *context)
{
#if 0
  GtkTextIter begin, end;

  if (ide_completion_context_get_bounds (context, &begin, &end))
    {
      while (!gtk_text_iter_starts_line (&begin) &&
             gtk_text_iter_backward_char (&begin))
        {
          /* Penalize the priority if we aren't at the beginning of the line */
          if (!g_unichar_isspace (gtk_text_iter_get_char (&begin)))
            return 500;
        }
    }
#endif

  return -100;
}

static gchar *
ide_snippet_completion_provider_get_title (IdeCompletionProvider *provider)
{
  return g_strdup ("Snippets");
}

static void
ide_snippet_completion_provider_populate_async (IdeCompletionProvider  *provider,
                                                IdeCompletionContext   *context,
                                                GCancellable           *cancellable,
                                                GAsyncReadyCallback     callback,
                                                gpointer                user_data)
{
  IdeSnippetCompletionProvider *self = (IdeSnippetCompletionProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *prefix = NULL;
  GtkSourceLanguage *lang;
  GtkTextBuffer *buffer;
  const gchar *lang_id = NULL;
  GtkTextIter begin, end;

  g_assert (IDE_IS_SNIPPET_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_snippet_completion_provider_populate_async);

  if (ide_completion_context_get_bounds (context, &begin, &end))
    prefix = gtk_text_iter_get_slice (&begin, &end);

  if ((buffer = ide_completion_context_get_buffer (context)) &&
      GTK_SOURCE_IS_BUFFER (buffer) &&
      (lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    lang_id = gtk_source_language_get_id (lang);

  ide_snippet_model_set_language (self->model, lang_id);
  ide_snippet_model_set_prefix (self->model, prefix);

  ide_task_return_pointer (task, g_object_ref (self->model), g_object_unref);
}

static GListModel *
ide_snippet_completion_provider_populate_finish (IdeCompletionProvider  *provider,
                                                 GAsyncResult           *result,
                                                 GError                **error)
{
  g_assert (IDE_IS_SNIPPET_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static gboolean
ide_snippet_completion_provider_refilter (IdeCompletionProvider *provider,
                                          IdeCompletionContext  *context,
                                          GListModel            *proposals)
{
  g_autofree gchar *prefix = NULL;
  GtkTextIter begin, end;

  g_assert (IDE_IS_SNIPPET_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_SNIPPET_MODEL (proposals));

  if (ide_completion_context_get_bounds (context, &begin, &end))
    prefix = gtk_text_iter_get_slice (&begin, &end);

  ide_snippet_model_set_prefix (IDE_SNIPPET_MODEL (proposals), prefix);

  return TRUE;
}

static void
ide_snippet_completion_provider_display_proposal (IdeCompletionProvider   *provider,
                                                  IdeCompletionListBoxRow *row,
                                                  IdeCompletionContext    *context,
                                                  const gchar             *typed_text,
                                                  IdeCompletionProposal   *proposal)
{
  g_autofree gchar *escaped = NULL;
  g_autofree gchar *highlight = NULL;
  const IdeSnippetInfo *info;

  g_assert (IDE_IS_SNIPPET_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_LIST_BOX_ROW (row));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_SNIPPET_COMPLETION_ITEM (proposal));

  info = ide_snippet_completion_item_get_info (IDE_SNIPPET_COMPLETION_ITEM (proposal));
  escaped = g_markup_escape_text (info->name, -1);
  highlight = ide_completion_fuzzy_highlight (escaped, typed_text);

  ide_completion_list_box_row_set_icon_name (row, "completion-snippet-symbolic");
  ide_completion_list_box_row_set_left (row, NULL);
  ide_completion_list_box_row_set_center_markup (row, highlight);
  ide_completion_list_box_row_set_right (row, NULL);
}

static void
ide_snippet_completion_provider_activate_proposal (IdeCompletionProvider *provider,
                                                   IdeCompletionContext  *context,
                                                   IdeCompletionProposal *proposal,
                                                   const GdkEventKey     *key)
{
  g_autoptr(IdeSnippet) snippet = NULL;
  GtkTextIter begin, end;
  GtkSourceLanguage *lang;
  GtkTextBuffer *buffer;
  GtkTextView *view;
  const gchar *lang_id = NULL;

  g_assert (IDE_IS_SNIPPET_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (IDE_IS_SNIPPET_COMPLETION_ITEM (proposal));

  buffer = ide_completion_context_get_buffer (context);
  view = ide_completion_context_get_view (context);

  if ((lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    lang_id = gtk_source_language_get_id (lang);

  snippet = ide_snippet_completion_item_get_snippet (IDE_SNIPPET_COMPLETION_ITEM (proposal), lang_id);

  gtk_text_buffer_begin_user_action (buffer);
  if (ide_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (buffer, &begin, &end);
  ide_source_view_push_snippet (IDE_SOURCE_VIEW (view), snippet, &begin);
  gtk_text_buffer_end_user_action (buffer);
}

static gchar *
ide_snippet_completion_provider_get_comment (IdeCompletionProvider *provider,
                                             IdeCompletionProposal *proposal)
{
  IdeSnippetCompletionItem *item = IDE_SNIPPET_COMPLETION_ITEM (proposal);
  const IdeSnippetInfo *info = ide_snippet_completion_item_get_info (item);

  return info ? g_strdup (info->desc) : NULL;
}

static void
provider_iface_init (IdeCompletionProviderInterface *iface)
{
  iface->load = ide_snippet_completion_provider_load;
  iface->get_priority = ide_snippet_completion_provider_get_priority;
  iface->get_title = ide_snippet_completion_provider_get_title;
  iface->populate_async = ide_snippet_completion_provider_populate_async;
  iface->populate_finish = ide_snippet_completion_provider_populate_finish;
  iface->refilter = ide_snippet_completion_provider_refilter;
  iface->display_proposal = ide_snippet_completion_provider_display_proposal;
  iface->activate_proposal = ide_snippet_completion_provider_activate_proposal;
  iface->get_comment = ide_snippet_completion_provider_get_comment;
}
