/* ide-file-transfer.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#pragma once

#if !defined (IDE_IO_INSIDE) && !defined (IDE_IO_COMPILATION)
# error "Only <libide-io.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_FILE_TRANSFER (ide_file_transfer_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeFileTransfer, ide_file_transfer, IDE, FILE_TRANSFER, GObject)

struct _IdeFileTransferClass
{
  GObjectClass parent_class;
};

typedef enum
{
  IDE_FILE_TRANSFER_FLAGS_NONE = 0,
  IDE_FILE_TRANSFER_FLAGS_MOVE = 1 << 0,
} IdeFileTransferFlags;

typedef struct
{
  gint64 n_files_total;
  gint64 n_files;
  gint64 n_dirs_total;
  gint64 n_dirs;
  gint64 n_bytes_total;
  gint64 n_bytes;

  /*< private >*/
  gint64 _padding[10];
} IdeFileTransferStat;

IDE_AVAILABLE_IN_ALL
IdeFileTransfer      *ide_file_transfer_new            (void);
IDE_AVAILABLE_IN_ALL
IdeFileTransferFlags  ide_file_transfer_get_flags      (IdeFileTransfer       *self);
IDE_AVAILABLE_IN_ALL
void                  ide_file_transfer_set_flags      (IdeFileTransfer       *self,
                                                        IdeFileTransferFlags   flags);
IDE_AVAILABLE_IN_ALL
gdouble               ide_file_transfer_get_progress   (IdeFileTransfer       *self);
IDE_AVAILABLE_IN_ALL
void                  ide_file_transfer_stat           (IdeFileTransfer       *self,
                                                        IdeFileTransferStat   *stat_buf);
IDE_AVAILABLE_IN_ALL
void                  ide_file_transfer_add            (IdeFileTransfer       *self,
                                                        GFile                 *src,
                                                        GFile                 *dest);
IDE_AVAILABLE_IN_ALL
void                  ide_file_transfer_execute_async  (IdeFileTransfer       *self,
                                                        gint                   io_priority,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
IDE_AVAILABLE_IN_ALL
gboolean              ide_file_transfer_execute_finish (IdeFileTransfer       *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);
IDE_AVAILABLE_IN_ALL
gboolean              ide_file_transfer_execute        (IdeFileTransfer       *self,
                                                        gint                   io_priority,
                                                        GCancellable          *cancellable,
                                                        GError               **error);

G_END_DECLS
