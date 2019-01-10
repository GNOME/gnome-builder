/* ide-hover-provider.c
 *
 * Copyright 2018-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-hover-provider"

#include "config.h"

#include "ide-hover-provider.h"
#include "ide-source-view.h"

G_DEFINE_INTERFACE (IdeHoverProvider, ide_hover_provider, G_TYPE_OBJECT)

static void
ide_hover_provider_real_hover_async (IdeHoverProvider    *self,
                                     IdeHoverContext     *context,
                                     const GtkTextIter   *location,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data,
                           ide_hover_provider_real_hover_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Hovering not supported");
}

static gboolean
ide_hover_provider_real_hover_finish (IdeHoverProvider  *self,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_hover_provider_default_init (IdeHoverProviderInterface *iface)
{
  iface->hover_async = ide_hover_provider_real_hover_async;
  iface->hover_finish = ide_hover_provider_real_hover_finish;
}

/**
 * ide_hover_provider_load:
 * @self: an #IdeHoverProvider
 * @view: an #IdeSourceView
 *
 * This method is used to load an #IdeHoverProvider.
 * Providers should perform any startup work from here.
 *
 * Since: 3.32
 */
void
ide_hover_provider_load (IdeHoverProvider *self,
                         IdeSourceView    *view)
{
  g_return_if_fail (IDE_IS_HOVER_PROVIDER (self));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (view));

  if (IDE_HOVER_PROVIDER_GET_IFACE (self)->load)
    IDE_HOVER_PROVIDER_GET_IFACE (self)->load (self, view);
}

/**
 * ide_hover_provider_unload:
 * @self: an #IdeHoverProvider
 * @view: an #IdeSourceView
 *
 * This method is used to unload an #IdeHoverProvider.
 * Providers should cleanup any state they've allocated.
 *
 * Since: 3.32
 */
void
ide_hover_provider_unload (IdeHoverProvider *self,
                           IdeSourceView    *view)
{
  g_return_if_fail (IDE_IS_HOVER_PROVIDER (self));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (view));

  if (IDE_HOVER_PROVIDER_GET_IFACE (self)->unload)
    IDE_HOVER_PROVIDER_GET_IFACE (self)->unload (self, view);
}

/**
 * ide_hover_provider_hover_async:
 * @self: an #IdeHoverProvider
 * @location: a #GtkTextIter
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 *
 * Since: 3.32
 */
void
ide_hover_provider_hover_async (IdeHoverProvider    *self,
                                IdeHoverContext     *context,
                                const GtkTextIter   *location,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_HOVER_PROVIDER (self));
  g_return_if_fail (IDE_IS_HOVER_CONTEXT (context));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_HOVER_PROVIDER_GET_IFACE (self)->hover_async (self, context, location, cancellable, callback, user_data);
}

/**
 * ide_hover_provider_hover_finish:
 * @self: an #IdeHoverProvider
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.32
 */
gboolean
ide_hover_provider_hover_finish (IdeHoverProvider  *self,
                                 GAsyncResult      *result,
                                 GError           **error)
{
  g_return_val_if_fail (IDE_IS_HOVER_PROVIDER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_HOVER_PROVIDER_GET_IFACE (self)->hover_finish (self, result, error);
}
