/* ide-build-log.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-build-log"

#include "config.h"

#include <libide-core.h>
#include <string.h>

#include "ide-build-log.h"
#include "ide-build-log-private.h"

#define POINTER_MARK(p)   GSIZE_TO_POINTER(GPOINTER_TO_SIZE(p)|1)
#define POINTER_UNMARK(p) GSIZE_TO_POINTER(GPOINTER_TO_SIZE(p)&~(gsize)1)
#define POINTER_MARKED(p) (GPOINTER_TO_SIZE(p)&1)
#define DISPATCH_MAX      20

struct _IdeBuildLog
{
  GObject      parent_instance;

  GArray      *observers;
  GAsyncQueue *log_queue;
  GSource     *log_source;

  guint        sequence;
};

typedef struct
{
  IdeBuildLogObserver callback;
  gpointer            data;
  GDestroyNotify      destroy;
  guint               id;
} Observer;

G_DEFINE_FINAL_TYPE (IdeBuildLog, ide_build_log, G_TYPE_OBJECT)

static gboolean
emit_log_from_main (gpointer user_data)
{
  IdeBuildLog *self = user_data;
  g_autoptr(GPtrArray) ar = g_ptr_array_new ();
  gpointer item;

  g_assert (IDE_IS_BUILD_LOG (self));

  /*
   * Pull up to DISPATCH_MAX items from the log queue. We have an upper
   * bound here so that we don't stall the main loop. Additionally, we
   * update the ready-time when we run out of items while holding the
   * async queue lock to synchronize with the caller for further wakeups.
   */
  g_async_queue_lock (self->log_queue);
  for (guint i = 0; i < DISPATCH_MAX; i++)
    {
      if (NULL == (item = g_async_queue_try_pop_unlocked (self->log_queue)))
        {
          g_source_set_ready_time (self->log_source, -1);
          break;
        }
      g_ptr_array_add (ar, item);
    }
  g_async_queue_unlock (self->log_queue);

  for (guint i = 0; i < ar->len; i++)
    {
      IdeBuildLogStream stream = IDE_BUILD_LOG_STDOUT;
      gchar *message;
      gsize message_len;

      item = g_ptr_array_index (ar, i);
      message = POINTER_UNMARK (item);
      message_len = strlen (message);

      if (POINTER_MARKED (item))
        stream = IDE_BUILD_LOG_STDERR;

      for (guint j = 0; j < self->observers->len; j++)
        {
          const Observer *observer = &g_array_index (self->observers, Observer, j);

          observer->callback (stream, message, message_len, observer->data);
        }

      g_free (message);
    }

  return G_SOURCE_CONTINUE;
}

static void
ide_build_log_finalize (GObject *object)
{
  IdeBuildLog *self = (IdeBuildLog *)object;

  g_clear_pointer (&self->log_queue, g_async_queue_unref);
  g_clear_pointer (&self->log_source, g_source_destroy);
  g_clear_pointer (&self->observers, g_array_unref);

  G_OBJECT_CLASS (ide_build_log_parent_class)->finalize (object);
}

static void
ide_build_log_class_init (IdeBuildLogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_log_finalize;
}

static void
ide_build_log_init (IdeBuildLog *self)
{
  self->observers = g_array_new (FALSE, FALSE, sizeof (Observer));

  self->log_queue = g_async_queue_new ();

  self->log_source = g_timeout_source_new (G_MAXINT);
  g_source_set_priority (self->log_source, G_PRIORITY_LOW);
  g_source_set_ready_time (self->log_source, -1);
  g_source_set_name (self->log_source, "[ide] IdeBuildLog");
  g_source_set_callback (self->log_source, emit_log_from_main, self, NULL);
  g_source_attach (self->log_source, g_main_context_default ());
}

static void
ide_build_log_via_main (IdeBuildLog       *self,
                        IdeBuildLogStream  stream,
                        const gchar       *message,
                        gsize              message_len)
{
  gchar *copied = g_strndup (message, message_len);

  if G_UNLIKELY (stream == IDE_BUILD_LOG_STDERR)
    copied = POINTER_MARK (copied);

  /*
   * Add the log entry to our queue to be dispatched in the main thread.
   * However, we hold the async queue lock while updating the source ready
   * time so we are synchronized with the main thread for setting the
   * ready time. This is needed because the main thread may not dispatch
   * all available items in a single dispatch (to avoid stalling the
   * main loop).
   */

  g_async_queue_lock (self->log_queue);
  g_async_queue_push_unlocked (self->log_queue, copied);
  g_source_set_ready_time (self->log_source, 0);
  g_async_queue_unlock (self->log_queue);
}

void
ide_build_log_observer (IdeBuildLogStream  stream,
                        const gchar       *message,
                        gssize             message_len,
                        gpointer           user_data)
{
  IdeBuildLog *self = user_data;

  g_assert (message != NULL);

  if (message_len < 0)
    message_len = strlen (message);

  g_assert (message[message_len] == '\0');

  if G_LIKELY (IDE_IS_MAIN_THREAD ())
    {
      for (guint i = 0; i < self->observers->len; i++)
        {
          const Observer *observer = &g_array_index (self->observers, Observer, i);

          observer->callback (stream, message, message_len, observer->data);
        }
    }
  else
    {
      ide_build_log_via_main (self, stream, message, message_len);
    }
}

guint
ide_build_log_add_observer (IdeBuildLog         *self,
                            IdeBuildLogObserver  observer,
                            gpointer             observer_data,
                            GDestroyNotify       observer_data_destroy)
{
  Observer ele;

  g_return_val_if_fail (IDE_IS_BUILD_LOG (self), 0);
  g_return_val_if_fail (observer != NULL, 0);

  ele.id = ++self->sequence;
  ele.callback = observer;
  ele.data = observer_data;
  ele.destroy = observer_data_destroy;

  g_array_append_val (self->observers, ele);

  return ele.id;
}

gboolean
ide_build_log_remove_observer (IdeBuildLog *self,
                               guint        observer_id)
{
  g_return_val_if_fail (IDE_IS_BUILD_LOG (self), FALSE);
  g_return_val_if_fail (observer_id > 0, FALSE);

  for (guint i = 0; i < self->observers->len; i++)
    {
      const Observer *observer = &g_array_index (self->observers, Observer, i);

      if (observer->id == observer_id)
        {
          g_array_remove_index (self->observers, i);
          return TRUE;
        }
    }

  return FALSE;
}

IdeBuildLog *
ide_build_log_new (void)
{
  return g_object_new (IDE_TYPE_BUILD_LOG, NULL);
}
