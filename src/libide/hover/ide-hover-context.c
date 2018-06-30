/* ide-hover-context.c
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#define G_LOG_DOMAIN "ide-hover-context"

#include "ide-hover-context.h"
#include "ide-hover-context-private.h"
#include "ide-hover-provider.h"

struct _IdeHoverContext
{
  GObject    parent_instance;
  GPtrArray *providers;
  GArray    *content;
};

typedef struct
{
  gchar            *title;
  IdeMarkedContent *content;
  GtkWidget        *widget;
} Item;

typedef struct
{
  guint active;
} Query;

G_DEFINE_TYPE (IdeHoverContext, ide_hover_context, G_TYPE_OBJECT)

static void
clear_item (Item *item)
{
  g_clear_pointer (&item->title, g_free);
  g_clear_object (&item->content);
  g_clear_pointer (&item->widget, gtk_widget_destroy);
}

static void
ide_hover_context_dispose (GObject *object)
{
  IdeHoverContext *self = (IdeHoverContext *)object;

  g_clear_pointer (&self->content, g_array_unref);

  G_OBJECT_CLASS (ide_hover_context_parent_class)->dispose (object);
}

static void
ide_hover_context_class_init (IdeHoverContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_hover_context_dispose;
}

static void
ide_hover_context_init (IdeHoverContext *self)
{
  self->providers = g_ptr_array_new_with_free_func (g_object_unref);

  self->content = g_array_new (FALSE, FALSE, sizeof (Item));
  g_array_set_clear_func (self->content, (GDestroyNotify) clear_item);
}

void
ide_hover_context_add_content (IdeHoverContext  *self,
                               const gchar      *title,
                               IdeMarkedContent *content)
{
  Item item = {0};

  g_return_if_fail (IDE_IS_HOVER_CONTEXT (self));
  g_return_if_fail (content != NULL);

  item.title = g_strdup (title);
  item.content = ide_marked_content_ref (content);
  item.widget = NULL;

  g_array_append_val (self->content, item);
}

void
_ide_hover_context_add_provider (IdeHoverContext  *self,
                                 IdeHoverProvider *provider)
{
  g_return_if_fail (IDE_IS_HOVER_CONTEXT (self));
  g_return_if_fail (IDE_IS_HOVER_PROVIDER (provider));

  g_ptr_array_add (self->providers, g_object_ref (provider));
}

static void
query_free (Query *q)
{
  g_slice_free (Query, q);
}

static void
ide_hover_context_query_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeHoverProvider *provider = (IdeHoverProvider *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Query *q;

  g_assert (IDE_IS_HOVER_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  q = g_task_get_task_data (task);
  g_assert (q != NULL);
  g_assert (q->active > 0);

  if (!ide_hover_provider_hover_finish (provider, result, &error))
    g_debug ("%s: %s", G_OBJECT_TYPE_NAME (provider), error->message);

  q->active--;

  if (q->active == 0)
    g_task_return_boolean (task, TRUE);
}

void
_ide_hover_context_query_async (IdeHoverContext     *self,
                                const GtkTextIter   *iter,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  Query *q;

  g_return_if_fail (IDE_IS_HOVER_CONTEXT (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, _ide_hover_context_query_async);

  q = g_slice_new0 (Query);
  q->active = self->providers->len;
  g_task_set_task_data (task, q, (GDestroyNotify)query_free);

  for (guint i = 0; i < self->providers->len; i++)
    {
      IdeHoverProvider *provider = g_ptr_array_index (self->providers, i);

      ide_hover_provider_hover_async (provider,
                                      self,
                                      iter,
                                      cancellable,
                                      ide_hover_context_query_cb,
                                      g_object_ref (task));
    }

  if (q->active == 0)
    g_task_return_boolean (task, TRUE);
}

/**
 * ide_hover_context_query_finish:
 * @self: an #IdeHoverContext
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a request to query providers.
 *
 * Returns: %TRUE if successful, otherwise %FALSE and @error.
 *
 * Since: 3.30
 */
gboolean
_ide_hover_context_query_finish (IdeHoverContext  *self,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  g_return_val_if_fail (IDE_IS_HOVER_CONTEXT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
ide_hover_context_has_content (IdeHoverContext *self)
{
  g_return_val_if_fail (IDE_IS_HOVER_CONTEXT (self), FALSE);

  return self->content != NULL && self->content->len > 0;
}
