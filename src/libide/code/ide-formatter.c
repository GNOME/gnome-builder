/* ide-formatter.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-formatter"

#include "config.h"

#include "ide-buffer.h"
#include "ide-formatter.h"
#include "ide-formatter-options.h"

G_DEFINE_INTERFACE (IdeFormatter, ide_formatter, G_TYPE_OBJECT)

static void
ide_formatter_real_format_async (IdeFormatter        *self,
                                 IdeBuffer           *buffer,
                                 IdeFormatterOptions *options,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_assert (IDE_IS_FORMATTER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_FORMATTER_OPTIONS (options));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_formatter_real_format_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "The operation is not supported");
}

static gboolean
ide_formatter_real_format_finish (IdeFormatter  *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (IDE_IS_FORMATTER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_formatter_real_format_range_async (IdeFormatter        *self,
                                       IdeBuffer           *buffer,
                                       IdeFormatterOptions *options,
                                       const GtkTextIter   *begin,
                                       const GtkTextIter   *end,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_assert (IDE_IS_FORMATTER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_FORMATTER_OPTIONS (options));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_formatter_real_format_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "The operation is not supported");
}

static gboolean
ide_formatter_real_format_range_finish (IdeFormatter  *self,
                                        GAsyncResult  *result,
                                        GError       **error)
{
  g_assert (IDE_IS_FORMATTER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_formatter_default_init (IdeFormatterInterface *iface)
{
  iface->format_async = ide_formatter_real_format_async;
  iface->format_finish = ide_formatter_real_format_finish;
  iface->format_range_async = ide_formatter_real_format_range_async;
  iface->format_range_finish = ide_formatter_real_format_range_finish;
}

void
ide_formatter_format_async (IdeFormatter        *self,
                            IdeBuffer           *buffer,
                            IdeFormatterOptions *options,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autofree char *title = NULL;

  g_return_if_fail (IDE_IS_FORMATTER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (IDE_IS_FORMATTER_OPTIONS (options));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  title = ide_buffer_dup_title (buffer);

  g_debug ("Formatting document \"%s\" using %s",
           title,
           G_OBJECT_TYPE_NAME (self));

  IDE_FORMATTER_GET_IFACE (self)->format_async (self, buffer, options, cancellable, callback, user_data);
}

gboolean
ide_formatter_format_finish (IdeFormatter  *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (IDE_IS_FORMATTER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_FORMATTER_GET_IFACE (self)->format_finish (self, result, error);
}

void
ide_formatter_format_range_async (IdeFormatter        *self,
                                  IdeBuffer           *buffer,
                                  IdeFormatterOptions *options,
                                  const GtkTextIter   *begin,
                                  const GtkTextIter   *end,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autofree char *title = NULL;

  g_return_if_fail (IDE_IS_FORMATTER (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (IDE_IS_FORMATTER_OPTIONS (options));
  g_return_if_fail (begin != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  title = ide_buffer_dup_title (buffer);

  g_debug ("Formatting document \"%s\" using %s with range %u-%u",
           title,
           G_OBJECT_TYPE_NAME (self),
           gtk_text_iter_get_offset (begin),
           gtk_text_iter_get_offset (end));

  IDE_FORMATTER_GET_IFACE (self)->format_range_async (self, buffer, options, begin, end, cancellable, callback, user_data);
}

gboolean
ide_formatter_format_range_finish (IdeFormatter  *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (IDE_IS_FORMATTER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_FORMATTER_GET_IFACE (self)->format_range_finish (self, result, error);
}

void
ide_formatter_load (IdeFormatter *self)
{
  g_return_if_fail (IDE_IS_FORMATTER (self));

  if (IDE_FORMATTER_GET_IFACE (self)->load)
    IDE_FORMATTER_GET_IFACE (self)->load (self);
}
