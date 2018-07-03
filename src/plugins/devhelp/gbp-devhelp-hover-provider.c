/* gbp-devhelp-hover-provider.c
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-devhelp-hover-provider"

#include "gbp-devhelp-hover-provider.h"

struct _GbpDevhelpHoverProvider
{
  GObject parent_instance;
};

static void
gbp_devhelp_hover_provider_hover_async (IdeHoverProvider    *provider,
                                        IdeHoverContext     *context,
                                        const GtkTextIter   *iter,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpDevhelpHoverProvider *self = (GbpDevhelpHoverProvider *)provider;
  //g_autoptr(IdeMarkedContent) content = NULL;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_DEVHELP_HOVER_PROVIDER (self));
  g_assert (IDE_IS_HOVER_CONTEXT (context));
  g_assert (iter != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_devhelp_hover_provider_hover_async);

  //content = ide_marked_content_new_plaintext ("this is some docs");
  //ide_hover_context_add_content (context, "Devhelp", content);

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_devhelp_hover_provider_hover_finish (IdeHoverProvider  *provider,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  g_assert (GBP_IS_DEVHELP_HOVER_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
hover_provider_iface_init (IdeHoverProviderInterface *iface)
{
  iface->hover_async = gbp_devhelp_hover_provider_hover_async;
  iface->hover_finish = gbp_devhelp_hover_provider_hover_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpDevhelpHoverProvider, gbp_devhelp_hover_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HOVER_PROVIDER,
                                                hover_provider_iface_init))

static void
gbp_devhelp_hover_provider_class_init (GbpDevhelpHoverProviderClass *klass)
{
}

static void
gbp_devhelp_hover_provider_init (GbpDevhelpHoverProvider *self)
{
}
