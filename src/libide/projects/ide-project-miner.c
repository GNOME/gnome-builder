/* ide-project-miner.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-project-miner"

#include "projects/ide-project-miner.h"

G_DEFINE_INTERFACE (IdeProjectMiner, ide_project_miner, G_TYPE_OBJECT)

enum {
  DISCOVERED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void
ide_project_miner_default_init (IdeProjectMinerInterface *iface)
{
  /**
   * IdeProjectMiner::discovered:
   * @self: An #IdeProjectMiner
   * @project_info: An #IdeProjectInfo
   *
   * This signal is emitted when a new project has been discovered by the miner.
   * The signal will always be emitted from the primary thread (Gtk+) as long as
   * ide_project_miner_emit_discovered() was used to emit the signal.
   */
  signals [DISCOVERED] = g_signal_new ("discovered",
                                        G_TYPE_FROM_INTERFACE (iface),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (IdeProjectMinerInterface, discovered),
                                        NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        1,
                                        IDE_TYPE_PROJECT_INFO);
}

static gboolean
emit_discovered_cb (gpointer user_data)
{
  gpointer *data = user_data;
  g_autoptr(IdeProjectMiner) miner = data[0];
  g_autoptr(IdeProjectInfo) project_info = data[1];

  g_signal_emit (miner, signals [DISCOVERED], 0, project_info);

  g_free (data);

  return G_SOURCE_REMOVE;
}

void
ide_project_miner_emit_discovered (IdeProjectMiner *self,
                                   IdeProjectInfo  *project_info)
{
  gpointer *data;

  g_return_if_fail (IDE_IS_PROJECT_MINER (self));
  g_return_if_fail (IDE_IS_PROJECT_INFO (project_info));

  data = g_new0 (gpointer, 2);
  data[0] = g_object_ref (self);
  data[1] = g_object_ref (project_info);

  g_timeout_add (0, emit_discovered_cb, data);
}

void
ide_project_miner_mine_async (IdeProjectMiner     *self,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_return_if_fail (IDE_IS_PROJECT_MINER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_PROJECT_MINER_GET_IFACE (self)->mine_async (self, cancellable, callback, user_data);
}

gboolean
ide_project_miner_mine_finish (IdeProjectMiner  *self,
                               GAsyncResult     *result,
                               GError          **error)
{
  g_return_val_if_fail (IDE_IS_PROJECT_MINER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_PROJECT_MINER_GET_IFACE (self)->mine_finish (self, result, error);
}
