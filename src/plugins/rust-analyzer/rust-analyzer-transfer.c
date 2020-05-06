/* rust-analyzer-transfer.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#include "rust-analyzer-transfer.h"
#include <libide-threading.h>
#include <libide-core.h>
#include <libsoup/soup.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <sys/stat.h>

struct _RustAnalyzerTransfer
{
  IdeTransfer parent_instance;
};

G_DEFINE_TYPE (RustAnalyzerTransfer, rust_analyzer_transfer, IDE_TYPE_TRANSFER)

RustAnalyzerTransfer *
rust_analyzer_transfer_new (void)
{
  return g_object_new (RUST_TYPE_ANALYZER_TRANSFER, NULL);
}

typedef struct {
  gchar buffer[6*1024];
  gsize count;
  guint64 total_bytes;
  gchar *filepath;
  GOutputStream *filestream;
  IdeTransfer *transfer;
  IdeTask *task;
} DownloadData;

static void
_downloaded_chunk (GObject      *source_object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autofree gchar *statusmsg = NULL;
  g_autoptr(GError) error = NULL;
  GInputStream *stream = G_INPUT_STREAM (source_object);
  DownloadData *data = user_data;

  gsize count = g_input_stream_read_finish (stream, result, &error);
  if (error != NULL)
    {
      ide_task_return_error (data->task, g_steal_pointer (&error));
      return;
    }

  if (count == 0)
    {
      g_output_stream_close (data->filestream, NULL, NULL);
      g_input_stream_close (stream, NULL, NULL);
      ide_task_return_boolean (data->task, TRUE);
      g_object_unref (data->task);
      g_chmod (data->filepath, S_IRWXU);
      g_free (data->filepath);
      g_slice_free (DownloadData, data);
      return;
    }

  data->count += count;
  statusmsg = g_strdup_printf ("%.2f MB / %.2f MB", data->count / 1048576., data->total_bytes / 1048576.);
  ide_transfer_set_status (data->transfer, statusmsg);
  ide_transfer_set_progress (data->transfer, (gdouble) data->count / data->total_bytes);

  g_output_stream_write_all (data->filestream, &data->buffer, count, NULL, ide_task_get_cancellable (data->task), &error);
  if (error != NULL)
    {
      ide_task_return_error (data->task, g_steal_pointer (&error));
      return;
    }
  g_input_stream_read_async (stream, &data->buffer, sizeof (data->buffer), G_PRIORITY_DEFAULT, ide_task_get_cancellable (data->task), _downloaded_chunk, data);
}

static void
_download_lsp (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
  g_autoptr(IdeTask) task = IDE_TASK (user_data);
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;
  SoupRequest *request = SOUP_REQUEST (source_object);
  GInputStream *stream = NULL;
  DownloadData *data;

  stream = soup_request_send_finish (request, result, NULL);

  data = g_slice_new0 (DownloadData);
  data->filepath = g_build_filename (g_get_home_dir (), ".cargo", "bin", "rust-analyzer", NULL);
  file = g_file_new_for_path (data->filepath);
  data->transfer = IDE_TRANSFER (ide_task_get_task_data (task));
  data->filestream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, ide_task_get_cancellable (data->task), &error));
  if (data->filestream == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }
  data->total_bytes = soup_request_get_content_length (request);
  data->task = g_steal_pointer (&task);

  g_input_stream_read_async (stream, &data->buffer, sizeof (data->buffer), G_PRIORITY_DEFAULT, ide_task_get_cancellable (data->task), _downloaded_chunk, data);
}

static void
rust_analyzer_transfer_execute_async (IdeTransfer         *transfer,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  RustAnalyzerTransfer *self = RUST_ANALYZER_TRANSFER (transfer);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(SoupSession) session = NULL;
  g_autoptr(SoupRequest) request = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, rust_analyzer_transfer_execute_async);
  ide_task_set_task_data (task, transfer, g_object_unref);

  session = soup_session_new ();

  request = soup_session_request (session, "https://github.com/rust-analyzer/rust-analyzer/releases/download/nightly/rust-analyzer-linux", &error);
  if (request == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    soup_request_send_async (request, NULL, _download_lsp, g_steal_pointer (&task));
}

static gboolean
rust_analyzer_transfer_execute_finish (IdeTransfer   *transfer,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  gboolean ret;

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  return ret;
}

static void
rust_analyzer_transfer_class_init (RustAnalyzerTransferClass *klass)
{
  IdeTransferClass *transfer_class = IDE_TRANSFER_CLASS (klass);

  transfer_class->execute_async = rust_analyzer_transfer_execute_async;
  transfer_class->execute_finish = rust_analyzer_transfer_execute_finish;
}

static void
rust_analyzer_transfer_init (RustAnalyzerTransfer *self)
{
  ide_transfer_set_title (IDE_TRANSFER (self), "Installing Rust Analyzer...");
}
