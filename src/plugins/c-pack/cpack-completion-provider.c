/* cpack-completion-provider.c
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

#define G_LOG_DOMAIN "cpack-completion-provider"

#include "config.h"

#include <libide-sourceview.h>

#include "cpack-completion-item.h"
#include "cpack-completion-provider.h"
#include "cpack-completion-results.h"

struct _CpackCompletionProvider
{
  IdeObject parent_instance;
};

static void provider_iface_init (IdeCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CpackCompletionProvider, cpack_completion_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

static void
cpack_completion_provider_class_init (CpackCompletionProviderClass *klass)
{
}

static void
cpack_completion_provider_init (CpackCompletionProvider *self)
{
}

#if 0
static void
cpack_completion_provider_populate_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  CpackCompletionResults *results = (CpackCompletionResults *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (CPACK_IS_COMPLETION_RESULTS (results));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!cpack_completion_results_populate_finish (results, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_object (task, g_object_ref (results));
}

static void
cpack_completion_provider_get_build_flags_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(CpackCompletionResults) results = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) build_flags = NULL;
  const gchar *prefix;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(build_flags = ide_buffer_get_build_flags_finish (buffer, result, &error)))
    {
      if (error != NULL)
        ide_task_return_error (task, g_steal_pointer (&error));
      else
        ide_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_NOT_SUPPORTED,
                                   "No build flags, no results to propagate");
      return;
    }

  results = g_object_new (CPACK_TYPE_COMPLETION_RESULTS, NULL);
  prefix = ide_task_get_task_data (task);

  cpack_completion_results_populate_async (results,
                                           (const gchar * const *)build_flags,
                                           prefix,
                                           ide_task_get_cancellable (task),
                                           cpack_completion_provider_populate_cb,
                                           g_object_ref (task));
}
#endif

static void
cpack_completion_provider_populate_async (IdeCompletionProvider *provider,
                                          IdeCompletionContext  *context,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *prefix = NULL;
  GtkTextIter begin, end;
  GtkTextBuffer *buffer;

  g_assert (CPACK_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, cpack_completion_provider_populate_async);

  ide_completion_context_get_bounds (context, &begin, &end);

  buffer = ide_completion_context_get_buffer (context);

  if (gtk_source_buffer_iter_has_context_class (GTK_SOURCE_BUFFER (buffer), &begin, "path"))
    {
      GtkTextIter cur = begin;

      while (gtk_text_iter_backward_char (&cur))
        {
          gunichar ch = gtk_text_iter_get_char (&cur);

          if (ch == '"' || ch == '<')
            {
              gtk_text_iter_forward_char (&cur);
              break;
            }
        }

      prefix = gtk_text_iter_get_slice (&cur, &begin);

      goto query_filesystem;
    }

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "Cannot complete includes here");
  return;

query_filesystem:

  g_assert (IDE_IS_BUFFER (buffer));

  ide_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             "TODO need access to build flags");

#if 0
  /*
   * First step is to get our list of include paths from the CFLAGS for the
   * file. After that, we can start looking for matches on the file-system
   * related to the current word.
   */
  ide_task_set_task_data (task, g_steal_pointer (&prefix), g_free);
  ide_buffer_get_build_flags_async (IDE_BUFFER (buffer),
                                    cancellable,
                                    cpack_completion_provider_get_build_flags_cb,
                                    g_steal_pointer (&task));
#endif
}

static GListModel *
cpack_completion_provider_populate_finish (IdeCompletionProvider  *self,
                                           GAsyncResult           *result,
                                           GError                **error)
{
  g_assert (CPACK_IS_COMPLETION_PROVIDER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_object (IDE_TASK (result), error);
}

static void
cpack_completion_provider_display_proposal (IdeCompletionProvider   *provider,
                                            IdeCompletionListBoxRow *row,
                                            IdeCompletionContext    *context,
                                            const gchar             *typed_text,
                                            IdeCompletionProposal   *proposal)
{
  g_autofree gchar *markup = NULL;

  g_assert (CPACK_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_LIST_BOX_ROW (row));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (CPACK_IS_COMPLETION_ITEM (proposal));

  markup = ide_completion_fuzzy_highlight (CPACK_COMPLETION_ITEM (proposal)->name, typed_text);

  ide_completion_list_box_row_set_icon_name (row, "lang-include-symbolic");
  ide_completion_list_box_row_set_left (row, NULL);
  ide_completion_list_box_row_set_center_markup (row, markup);
  ide_completion_list_box_row_set_right (row, NULL);
}

static void
cpack_completion_provider_activate_proposal (IdeCompletionProvider *provider,
                                             IdeCompletionContext  *context,
                                             IdeCompletionProposal *proposal,
                                             const GdkEventKey     *key)
{
  CpackCompletionItem *item = (CpackCompletionItem *)proposal;
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;
  gsize len;

  g_assert (CPACK_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (CPACK_IS_COMPLETION_ITEM (item));

  buffer = ide_completion_context_get_buffer (context);

  len = strlen (item->name);

  gtk_text_buffer_begin_user_action (buffer);

  if (ide_completion_context_get_bounds (context, &begin, &end))
    gtk_text_buffer_delete (buffer, &begin, &end);

  /* don't insert trailing / so user does that to trigger next popup */
  if (len && item->name[len - 1] == '/')
    gtk_text_buffer_insert (buffer, &begin, item->name, len - 1);
  else
    gtk_text_buffer_insert (buffer, &begin, item->name, len);

  gtk_text_buffer_end_user_action (buffer);
}

static gboolean
cpack_completion_provider_refilter (IdeCompletionProvider *provider,
                                    IdeCompletionContext  *context,
                                    GListModel            *model)
{
  g_autofree gchar *word = NULL;

  g_assert (CPACK_IS_COMPLETION_PROVIDER (provider));
  g_assert (IDE_IS_COMPLETION_CONTEXT (context));
  g_assert (CPACK_IS_COMPLETION_RESULTS (model));

  word = ide_completion_context_get_word (context);
  cpack_completion_results_refilter (CPACK_COMPLETION_RESULTS (model), word);

  return TRUE;
}

static gint
cpack_completion_provider_get_priority (IdeCompletionProvider *provider,
                                        IdeCompletionContext  *context)
{
  /* we only activate when applicable, so high priority */
  return -100;
}

static gboolean
cpack_completion_provider_is_trigger (IdeCompletionProvider *provider,
                                      const GtkTextIter     *iter,
                                      gunichar               ch)
{
  if (ch == '/')
    {
      GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);

      if (gtk_source_buffer_iter_has_context_class (GTK_SOURCE_BUFFER (buffer), iter, "path"))
        return TRUE;
    }

  return FALSE;
}

static void
provider_iface_init (IdeCompletionProviderInterface *iface)
{
  iface->populate_async = cpack_completion_provider_populate_async;
  iface->populate_finish = cpack_completion_provider_populate_finish;
  iface->refilter = cpack_completion_provider_refilter;
  iface->display_proposal = cpack_completion_provider_display_proposal;
  iface->activate_proposal = cpack_completion_provider_activate_proposal;
  iface->get_priority = cpack_completion_provider_get_priority;
  iface->is_trigger = cpack_completion_provider_is_trigger;
}
