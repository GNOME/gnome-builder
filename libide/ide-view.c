/* ide-view.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "ide-view.h"

G_DEFINE_INTERFACE (IdeView, ide_view, GTK_TYPE_WIDGET)

static const gchar *
ide_view_real_get_title (IdeView *self)
{
  return NULL;
}

static const gchar *
ide_view_real_get_icon_name (IdeView *self)
{
  return NULL;
}

static gboolean
ide_view_real_get_can_save (IdeView *self)
{
  return FALSE;
}

static gboolean
ide_view_real_get_needs_attention (IdeView *self)
{
  return FALSE;
}

static void
ide_view_real_save_async (IdeView             *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_task_report_new_error (self, callback, user_data, ide_view_real_save_async,
                           G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Saving is not supported.");
}

static gboolean
ide_view_real_save_finish (IdeView       *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_view_default_init (IdeViewInterface *iface)
{
  iface->get_title = ide_view_real_get_title;
  iface->get_icon_name = ide_view_real_get_icon_name;
  iface->get_can_save = ide_view_real_get_can_save;
  iface->get_needs_attention = ide_view_real_get_needs_attention;
  iface->save_async = ide_view_real_save_async;
  iface->save_finish = ide_view_real_save_finish;
}

const gchar *
ide_view_get_title (IdeView *self)
{
  g_return_val_if_fail (IDE_IS_VIEW (self), NULL);

  return NULL;
}

const gchar *
ide_view_get_icon_name (IdeView *self)
{
  g_return_val_if_fail (IDE_IS_VIEW (self), NULL);

  return IDE_VIEW_GET_IFACE (self)->get_icon_name (self);
}

gboolean
ide_view_get_can_save (IdeView *self)
{
  g_return_val_if_fail (IDE_IS_VIEW (self), FALSE);

  return IDE_VIEW_GET_IFACE (self)->get_can_save (self);
}

gboolean
ide_view_get_needs_attention (IdeView *self)
{
  g_return_val_if_fail (IDE_IS_VIEW (self), FALSE);

  return IDE_VIEW_GET_IFACE (self)->get_needs_attention (self);
}

void
ide_view_save_async (IdeView             *self,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  g_return_if_fail (IDE_IS_VIEW (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_VIEW_GET_IFACE (self)->save_async (self, cancellable, callback, user_data);
}

gboolean
ide_view_save_finish (IdeView       *self,
                      GAsyncResult  *result,
                      GError       **error)
{
  g_return_val_if_fail (IDE_IS_VIEW (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_VIEW_GET_IFACE (self)->save_finish (self, result, error);
}
