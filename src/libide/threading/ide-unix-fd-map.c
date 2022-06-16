/* ide-unix-fd-map.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-unix-fd-map"

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "ide-unix-fd-map.h"

typedef struct
{
  int source_fd;
  int dest_fd;
} IdeUnixFDMapItem;

struct _IdeUnixFDMap
{
  GObject  parent_instance;
  GArray  *map;
};

G_DEFINE_FINAL_TYPE (IdeUnixFDMap, ide_unix_fd_map, G_TYPE_OBJECT)

static void
item_clear (gpointer data)
{
  IdeUnixFDMapItem *item = data;

  item->dest_fd = -1;

  if (item->source_fd != -1)
    {
      close (item->source_fd);
      item->source_fd = -1;
    }
}

static void
ide_unix_fd_map_dispose (GObject *object)
{
  IdeUnixFDMap *self = (IdeUnixFDMap *)object;

  if (self->map->len > 0)
    g_array_remove_range (self->map, 0, self->map->len);

  G_OBJECT_CLASS (ide_unix_fd_map_parent_class)->dispose (object);
}

static void
ide_unix_fd_map_finalize (GObject *object)
{
  IdeUnixFDMap *self = (IdeUnixFDMap *)object;

  g_clear_pointer (&self->map, g_array_unref);

  G_OBJECT_CLASS (ide_unix_fd_map_parent_class)->finalize (object);
}

static void
ide_unix_fd_map_class_init (IdeUnixFDMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_unix_fd_map_dispose;
  object_class->finalize = ide_unix_fd_map_finalize;
}

static void
ide_unix_fd_map_init (IdeUnixFDMap *self)
{
  self->map = g_array_new (FALSE, FALSE, sizeof (IdeUnixFDMapItem));
  g_array_set_clear_func (self->map, item_clear);
}

IdeUnixFDMap *
ide_unix_fd_map_new (void)
{
  return g_object_new (IDE_TYPE_UNIX_FD_MAP, NULL);
}

guint
ide_unix_fd_map_get_length (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), 0);

  return self->map->len;
}

void
ide_unix_fd_map_take (IdeUnixFDMap *self,
                      int           source_fd,
                      int           dest_fd)
{
  IdeUnixFDMapItem insert;

  g_return_if_fail (IDE_IS_UNIX_FD_MAP (self));
  g_return_if_fail (source_fd > -1);
  g_return_if_fail (dest_fd > -1);

  for (guint i = 0; i < self->map->len; i++)
    {
      IdeUnixFDMapItem *item = &g_array_index (self->map, IdeUnixFDMapItem, i);

      if (item->dest_fd == dest_fd)
        {
          if (item->source_fd != -1)
            close (item->source_fd);
          item->source_fd = source_fd;
          return;
        }
    }

  insert.source_fd = source_fd;
  insert.dest_fd = dest_fd;
  g_array_append_val (self->map, insert);
}

int
ide_unix_fd_map_steal (IdeUnixFDMap *self,
                       guint         index,
                       int          *dest_fd)
{
  IdeUnixFDMapItem *steal;

  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);
  g_return_val_if_fail (index < self->map->len, -1);

  steal = &g_array_index (self->map, IdeUnixFDMapItem, index);

  if (dest_fd != NULL)
    *dest_fd = steal->dest_fd;

  return ide_steal_fd (&steal->source_fd);
}

int
ide_unix_fd_map_get (IdeUnixFDMap  *self,
                     guint          index,
                     int           *dest_fd,
                     GError       **error)
{
  IdeUnixFDMapItem *item;
  int ret = -1;

  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);
  g_return_val_if_fail (index < self->map->len, -1);

  item = &g_array_index (self->map, IdeUnixFDMapItem, index);

  if (item->source_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CLOSED,
                   "File-descriptor at index %u already stolen",
                   index);
      return -1;
    }

  ret = dup (item->source_fd);

  if (ret == -1)
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      return -1;
    }

  return ret;
}

int
ide_unix_fd_map_peek (IdeUnixFDMap *self,
                      guint         index,
                      int          *dest_fd)
{
  const IdeUnixFDMapItem *item;

  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);
  g_return_val_if_fail (index < self->map->len, -1);

  item = &g_array_index (self->map, IdeUnixFDMapItem, index);

  if (dest_fd != NULL)
    *dest_fd = item->dest_fd;

  return item->source_fd;
}

static int
ide_unix_fd_map_peek_for_dest_fd (IdeUnixFDMap *self,
                                  int           dest_fd)
{
  g_assert (IDE_IS_UNIX_FD_MAP (self));
  g_assert (dest_fd != -1);

  for (guint i = 0; i < self->map->len; i++)
    {
      const IdeUnixFDMapItem *item = &g_array_index (self->map, IdeUnixFDMapItem, i);

      if (item->dest_fd == dest_fd)
        return item->source_fd;
    }

  return -1;
}

int
ide_unix_fd_map_peek_stdin (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);

  return ide_unix_fd_map_peek_for_dest_fd (self, STDIN_FILENO);
}

int
ide_unix_fd_map_peek_stdout (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);

  return ide_unix_fd_map_peek_for_dest_fd (self, STDOUT_FILENO);
}

int
ide_unix_fd_map_peek_stderr (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);

  return ide_unix_fd_map_peek_for_dest_fd (self, STDERR_FILENO);
}

static int
ide_unix_fd_map_steal_for_dest_fd (IdeUnixFDMap *self,
                                   int           dest_fd)
{
  g_assert (IDE_IS_UNIX_FD_MAP (self));
  g_assert (dest_fd != -1);

  for (guint i = 0; i < self->map->len; i++)
    {
      IdeUnixFDMapItem *item = &g_array_index (self->map, IdeUnixFDMapItem, i);

      if (item->dest_fd == dest_fd)
        return ide_steal_fd (&item->source_fd);
    }

  return -1;
}

int
ide_unix_fd_map_steal_stdin (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);

  return ide_unix_fd_map_steal_for_dest_fd (self, STDIN_FILENO);
}

int
ide_unix_fd_map_steal_stdout (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);

  return ide_unix_fd_map_steal_for_dest_fd (self, STDOUT_FILENO);
}

int
ide_unix_fd_map_steal_stderr (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);

  return ide_unix_fd_map_steal_for_dest_fd (self, STDERR_FILENO);
}

static gboolean
ide_unix_fd_map_isatty (IdeUnixFDMap *self,
                        int           dest_fd)
{
  g_assert (IDE_IS_UNIX_FD_MAP (self));
  g_assert (dest_fd != -1);

  for (guint i = 0; i < self->map->len; i++)
    {
      const IdeUnixFDMapItem *item = &g_array_index (self->map, IdeUnixFDMapItem, i);

      if (item->dest_fd == dest_fd)
        return item->source_fd != -1 && isatty (item->source_fd);
    }

  return FALSE;
}

gboolean
ide_unix_fd_map_stdin_isatty (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), FALSE);

  return ide_unix_fd_map_isatty (self, STDIN_FILENO);
}

gboolean
ide_unix_fd_map_stdout_isatty (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), FALSE);

  return ide_unix_fd_map_isatty (self, STDOUT_FILENO);
}

gboolean
ide_unix_fd_map_stderr_isatty (IdeUnixFDMap *self)
{
  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), FALSE);

  return ide_unix_fd_map_isatty (self, STDERR_FILENO);
}

int
ide_unix_fd_map_get_max_dest_fd (IdeUnixFDMap *self)
{
  int max_dest_fd = 2;

  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), -1);

  for (guint i = 0; i < self->map->len; i++)
    {
      const IdeUnixFDMapItem *item = &g_array_index (self->map, IdeUnixFDMapItem, i);

      if (item->dest_fd > max_dest_fd)
        max_dest_fd = item->dest_fd;
    }

  return max_dest_fd;
}

gboolean
ide_unix_fd_map_open_file (IdeUnixFDMap  *self,
                           const char    *filename,
                           int            dest_fd,
                           int            mode,
                           GError       **error)
{
  int fd;

  g_return_val_if_fail (IDE_IS_UNIX_FD_MAP (self), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (dest_fd > -1, FALSE);

  if (-1 == (fd = open (filename, mode)))
    {
      int errsv = errno;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      return FALSE;
    }

  ide_unix_fd_map_take (self, fd, dest_fd);

  return TRUE;
}
