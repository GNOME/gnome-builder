/* ide-cancellable.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-cancellable"

#include "config.h"

#include "ide-cancellable.h"

#define CHAINED_INFO_MAGIC 0x81734637

typedef struct
{
  guint         magic;
  volatile gint ref_count;
  GMutex        mutex;
  GWeakRef      self;
  GWeakRef      other;
  gulong        other_handler;
} ChainedInfo;

static void
chained_info_free (gpointer data)
{
  ChainedInfo *info = data;
  g_autoptr(GCancellable) self = NULL;
  g_autoptr(GCancellable) other = NULL;

  g_assert (info != NULL);
  g_assert (info->magic == CHAINED_INFO_MAGIC);
  g_assert (info->ref_count == 0);

  info->magic = 0;

  self = g_weak_ref_get (&info->self);
  other = g_weak_ref_get (&info->other);

  if (other != NULL && info->other_handler != 0)
    g_clear_signal_handler (&info->other_handler, other);
  else
    info->other_handler = 0;

  g_weak_ref_clear (&info->other);
  g_weak_ref_clear (&info->self);

  g_mutex_clear (&info->mutex);

  g_slice_free (ChainedInfo, info);
}

static void
chained_info_unref (ChainedInfo *info)
{
  g_autoptr(GCancellable) other = NULL;

  g_assert (info != NULL);
  g_assert (info->ref_count > 0);
  g_assert (info->magic == CHAINED_INFO_MAGIC);

  if ((other = g_weak_ref_get (&info->other)))
    {
      gulong handler_id;

      g_mutex_lock (&info->mutex);
      handler_id = info->other_handler;
      info->other_handler = 0;
      g_mutex_unlock (&info->mutex);

      if (handler_id)
        g_signal_handler_disconnect (other, handler_id);
    }

  if (g_atomic_int_dec_and_test (&info->ref_count))
    chained_info_free (info);
}

static void
ide_cancellable_cancelled_cb (GCancellable *other,
                              ChainedInfo  *info)
{
  g_autoptr(GCancellable) self = NULL;

  g_assert (G_IS_CANCELLABLE (other));
  g_assert (info != NULL);
  g_assert (info->ref_count > 0);
  g_assert (info->magic == CHAINED_INFO_MAGIC);

  self = g_weak_ref_get (&info->self);

  if (self != NULL)
    {
      if (!g_cancellable_is_cancelled (self))
        g_cancellable_cancel (self);
    }

  g_clear_signal_handler (&info->other_handler, other);
}

static void
ide_cancellable_weak_cb (gpointer  data,
                         GObject  *where_object_was)
{
  ChainedInfo *info = data;

  g_assert (info != NULL);
  g_assert (info->ref_count > 0);
  g_assert (info->magic == CHAINED_INFO_MAGIC);

  chained_info_unref (info);
}

/**
 * ide_cancellable_chain:
 * @self: (nullable): a #GCancellable or %NULL
 * @other: (nullable): a #GCancellable or %NULL
 *
 * If both @self and @other are not %NULL, then the cancellation of
 * @other will be propagated to @self if @other is cancelled.
 *
 * If @self and @other are the same, @self is returned and no additional
 * chaining will occur.
 *
 * If @self and @other are %NULL, then %NULL is returned.
 * If @self is non-%NULL, it will be returned.
 * If @self is %NULL and @other is non-%NULL, other will be
 * returned. This is useful to succinctly chain cancellables like:
 *
 * |[
 * cancellable = ide_cancellable_chain (cancellable, self->cancellable);
 * ]|
 *
 * Returns: (transfer none) (nullable): a #GCancellable or %NULL
 */
GCancellable *
ide_cancellable_chain (GCancellable *self,
                       GCancellable *other)
{
  ChainedInfo *info;

  g_return_val_if_fail (!self || G_IS_CANCELLABLE (self), NULL);
  g_return_val_if_fail (!other || G_IS_CANCELLABLE (other), NULL);

  if (self == other)
    return self;
  else if (self == NULL)
    return other;
  else if (other == NULL)
    return self;

  /*
   * We very much want to avoid taking a reference in the process
   * here because that makes it difficult to know if we've created
   * any sort of reference cycles or cancellable leaks.
   */

  info = g_slice_new0 (ChainedInfo);
  info->magic = CHAINED_INFO_MAGIC;
  info->ref_count = 3;
  g_mutex_init (&info->mutex);
  g_weak_ref_init (&info->self, self);
  g_weak_ref_init (&info->other, other);
  g_object_weak_ref (G_OBJECT (self), ide_cancellable_weak_cb, info);
  g_object_weak_ref (G_OBJECT (other), ide_cancellable_weak_cb, info);
  info->other_handler = g_cancellable_connect (other,
                                               G_CALLBACK (ide_cancellable_cancelled_cb),
                                               info,
                                               (GDestroyNotify)chained_info_unref);

  return self;
}
