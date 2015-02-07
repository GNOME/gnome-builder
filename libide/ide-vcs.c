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
