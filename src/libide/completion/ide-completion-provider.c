/* ide-completion-provider.c
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

#define G_LOG_DOMAIN "ide-completion-provider"

#include "config.h"

#include "ide-completion-context.h"
#include "ide-completion-proposal.h"
#include "ide-completion-provider.h"

G_DEFINE_INTERFACE (IdeCompletionProvider, ide_completion_provider, G_TYPE_OBJECT)

static void
ide_completion_provider_default_init (IdeCompletionProviderInterface *iface)
{
}

/**
 * ide_completion_provider_get_icon:
 * @self: an #IdeCompletionProvider
 *
 * Gets the #GIcon to represent this provider. This may be used in UI
 * to allow the user to filter the results to only those of this
 * completion provider.
 *
 * Returns: (transfer full) (nullable): a #GIcon or %NULL.
 */
GIcon *
ide_completion_provider_get_icon (IdeCompletionProvider *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_PROVIDER (self), NULL);

  if (IDE_COMPLETION_PROVIDER_GET_IFACE (self)->get_icon)
    return IDE_COMPLETION_PROVIDER_GET_IFACE (self)->get_icon (self);

  return NULL;
}

/**
 * ide_completion_provider_get_priority:
 * @self: an #IdeCompletionProvider
 *
 * Gets the priority for the completion provider.
 *
 * This value is used to group all of the providers proposals together
 * when displayed, with relation to other providers.
 *
 * Returns: an integer specific to the provider
 *
 * Since: 3.28
 */
gint
ide_completion_provider_get_priority (IdeCompletionProvider *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_PROVIDER (self), 0);

  if (IDE_COMPLETION_PROVIDER_GET_IFACE (self)->get_priority)
    return IDE_COMPLETION_PROVIDER_GET_IFACE (self)->get_priority (self);

  return 0;
}

/**
 * ide_completion_provider_get_title:
 * @self: an #IdeCompletionProvider
 *
 * Gets the title for the provider. This may be used in UI to give
 * the user context about the type of results that are displayed.
 *
 * Returns: (transfer full) (nullable): a string or %NULL
 */
gchar *
ide_completion_provider_get_title (IdeCompletionProvider *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_PROVIDER (self), NULL);

  if (IDE_COMPLETION_PROVIDER_GET_IFACE (self)->get_title)
    return IDE_COMPLETION_PROVIDER_GET_IFACE (self)->get_title (self);

  return NULL;
}

/**
 * ide_completion_provider_populate_async:
 * @self: an #IdeCompletionProvider
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @proposals: (out) (optional): Optional location for a #GListModel that
 *   will be populated interactively.
 * @callback: (nullable) (scope async) (closure user_data): a #GAsyncReadyCallback
 *   or %NULL. Called when the provider has completed loading proposals.
 * @user_data: closure data for @callback
 *
 * Asynchronously requests the provider populate the contents.
 *
 * This operation should not complete until it has finished loading proposals.
 * If the provider can incrementally update the result set, it should set
 * @proposals and insert items before it completes the asynchronous operation.
 * That allows the UI to backfill the result list.
 *
 * Since: 3.28
 */
void
ide_completion_provider_populate_async (IdeCompletionProvider  *self,
                                        GCancellable           *cancellable,
                                        GListModel            **proposals,
                                        GAsyncReadyCallback     callback,
                                        gpointer                user_data)
{
  g_autoptr(GListModel) results = NULL;

  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_COMPLETION_PROVIDER_GET_IFACE (self)->populate_async (self, cancellable, &results, callback, user_data);

  if (proposals != NULL)
    *proposals = g_steal_pointer (&results);
  else
    *proposals = NULL;
}

/**
 * ide_completion_provider_populate_finish:
 * @self: an #IdeCompletionProvider
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a GError, or %NULL
 *
 * Returns: (transfer full): a #GListModel of #IdeCompletionProposal
 */
GListModel *
ide_completion_provider_populate_finish (IdeCompletionProvider  *self,
                                         GAsyncResult           *result,
                                         GError                **error)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_COMPLETION_PROVIDER_GET_IFACE (self)->populate_finish (self, result, error);
}

void
ide_completion_provider_activate_poposal (IdeCompletionProvider *self,
                                          IdeCompletionContext  *context,
                                          IdeCompletionProposal *proposal,
                                          const GdkEventKey     *key)
{
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (self));
  g_return_if_fail (IDE_IS_COMPLETION_CONTEXT (context));
  g_return_if_fail (IDE_IS_COMPLETION_PROPOSAL (proposal));

  if (IDE_COMPLETION_PROVIDER_GET_IFACE (self)->activate_proposal)
    IDE_COMPLETION_PROVIDER_GET_IFACE (self)->activate_proposal (self, context, proposal, key);
  else
    g_critical ("%s does not implement activate_proposal()!", G_OBJECT_TYPE_NAME (self));
}

/**
 * ide_completion_provider_refilter:
 * @self: an #IdeCompletionProvider
 * @context: an #IdeCompletionContext
 * @proposals: a #GListModel of results previously provided to the context
 *
 * This requests that the completion provider refilter the results based on
 * changes to the #IdeCompletionContext, such as additional text typed by the
 * user. If the provider can refine the results, then the provider should do
 * so and return %TRUE.
 *
 * Otherwise, %FALSE is returned and the context will request a new set of
 * completion results.
 *
 * Returns: %TRUE if refiltered; otherwise %FALSE
 *
 * Since: 3.28
 */
gboolean
ide_completion_provider_refilter (IdeCompletionProvider *self,
                                  IdeCompletionContext  *context,
                                  GListModel            *proposals)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_PROVIDER (self), FALSE);
  g_return_val_if_fail (IDE_IS_COMPLETION_CONTEXT (context), FALSE);
  g_return_val_if_fail (G_IS_LIST_MODEL (proposals), FALSE);

  if (IDE_COMPLETION_PROVIDER_GET_IFACE (self)->refilter)
    return IDE_COMPLETION_PROVIDER_GET_IFACE (self)->refilter (self, context, proposals);

  return FALSE;
}

/**
 * ide_completion_provider_is_trigger:
 * @self: an #IdeCompletionProvider
 * @iter: the current insertion point
 * @ch: the character that was just inserted
 *
 * Completion providers may want to trigger that the completion window is
 * displayed upon insertion of a particular character. For example, a C
 * indenter might want to trigger after -> or . is inserted.
 *
 * @ch is set to the character that was just inserted. If you need something
 * more complex, copy @iter and move it backwards twice to check the character
 * previous to @ch.
 *
 * Returns: %TRUE to request that the completion window is displayed.
 *
 * Since: 3.30
 */
gboolean
ide_completion_provider_is_trigger (IdeCompletionProvider *self,
                                    const GtkTextIter     *iter,
                                    gunichar               ch)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_PROVIDER (self), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  if (IDE_COMPLETION_PROVIDER_GET_IFACE (self)->is_trigger)
    return IDE_COMPLETION_PROVIDER_GET_IFACE (self)->is_trigger (self, iter, ch);

  return FALSE;
}

/**
 * ide_completion_provider_key_activates:
 * @self: a #IdeCompletionProvider
 * @proposal: an #IdeCompletionProposal created by the provider
 * @key: the #GdkEventKey for the current keyboard event
 *
 * This function is called to ask the provider if the key-press event should
 * force activation of the proposal. This is useful for languages where you
 * might want to activate the completion from a language-specific character.
 *
 * For example, in C, you might want to use period (.) to activate the
 * completion and insert either (.) or (->) based on the type.
 *
 * Returns: %TRUE if the proposal should be activated.
 *
 * Since: 3.30
 */
gboolean
ide_completion_provider_key_activates (IdeCompletionProvider *self,
                                       IdeCompletionProposal *proposal,
                                       const GdkEventKey     *key)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_PROVIDER (self), FALSE);
  g_return_val_if_fail (IDE_IS_COMPLETION_PROPOSAL (proposal), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  if (IDE_COMPLETION_PROVIDER_GET_IFACE (self)->key_activates)
    return IDE_COMPLETION_PROVIDER_GET_IFACE (self)->key_activates (self, proposal, key);

  return FALSE;
}

void
_ide_completion_provider_load (IdeCompletionProvider *self,
                               IdeContext            *context)
{
  g_return_if_fail (IDE_IS_COMPLETION_PROVIDER (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  if (IDE_COMPLETION_PROVIDER_GET_IFACE (self)->load)
    IDE_COMPLETION_PROVIDER_GET_IFACE (self)->load (self, context);
}
