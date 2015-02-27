/* ide-vcs.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ide-buffer.h"
#include "ide-buffer-change-monitor.h"
#include "ide-vcs.h"

G_DEFINE_ABSTRACT_TYPE (IdeVcs, ide_vcs, IDE_TYPE_OBJECT)

static void
ide_vcs_class_init (IdeVcsClass *klass)
{
}

static void
ide_vcs_init (IdeVcs *self)
{
}

/**
 * ide_vcs_get_working_directory:
 * @self: An #IdeVcs.
 *
 * Retrieves the working directory for the context. This is the root of where
 * the project files exist.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
ide_vcs_get_working_directory (IdeVcs *self)
{
  g_return_val_if_fail (IDE_IS_VCS (self), NULL);

  if (IDE_VCS_GET_CLASS (self)->get_working_directory)
    return IDE_VCS_GET_CLASS (self)->get_working_directory (self);

  return NULL;
}

/**
 * ide_vcs_get_buffer_change_monitor:
 *
 * Gets an #IdeBufferChangeMonitor for the buffer provided. If the #IdeVcs implementation does not
 * support change monitoring, or cannot for the current file, then %NULL is returned.
 *
 * Returns: (transfer full) (nullable): An #IdeBufferChangeMonitor or %NULL.
 */
IdeBufferChangeMonitor *
ide_vcs_get_buffer_change_monitor (IdeVcs    *self,
                                   IdeBuffer *buffer)
{
  IdeBufferChangeMonitor *ret = NULL;

  g_return_val_if_fail (IDE_IS_VCS (self), NULL);
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  if (IDE_VCS_GET_CLASS (self)->get_buffer_change_monitor)
    ret = IDE_VCS_GET_CLASS (self)->get_buffer_change_monitor (self, buffer);

  g_return_val_if_fail (!ret || IDE_IS_BUFFER_CHANGE_MONITOR (ret), NULL);

  return ret;
}

void
ide_vcs_new_async (IdeContext           *context,
                   int                   io_priority,
                   GCancellable         *cancellable,
                   GAsyncReadyCallback   callback,
                   gpointer              user_data)
{
  ide_object_new_async (IDE_VCS_EXTENSION_POINT,
                        io_priority,
                        cancellable,
                        callback,
                        user_data,
                        "context", context,
                        NULL);
}

IdeVcs *
ide_vcs_new_finish (GAsyncResult  *result,
                    GError       **error)
{
  IdeObject *ret;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = ide_object_new_finish (result, error);

  return IDE_VCS (ret);
}
