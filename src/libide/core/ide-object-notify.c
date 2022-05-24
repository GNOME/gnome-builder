/* ide-object-notify.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-object-notify"

#include "config.h"

#include "ide-object.h"
#include "ide-macros.h"

typedef struct
{
  GObject    *object;
  GParamSpec *pspec;
} NotifyInMain;

static gboolean
ide_object_notify_in_main_cb (gpointer data)
{
  NotifyInMain *notify = data;

  g_assert (notify != NULL);
  g_assert (G_IS_OBJECT (notify->object));
  g_assert (notify->pspec != NULL);

  g_object_notify_by_pspec (notify->object, notify->pspec);

  g_object_unref (notify->object);
  g_param_spec_unref (notify->pspec);
  g_slice_free (NotifyInMain, notify);

  return G_SOURCE_REMOVE;
}

/**
 * ide_object_notify_by_pspec:
 * @instance: a #IdeObjectNotify
 * @pspec: a #GParamSpec
 *
 * Like g_object_notify_by_pspec() if the caller is in the main-thread.
 * Otherwise, the request is deferred to the main thread.
 */
void
ide_object_notify_by_pspec (gpointer    instance,
                            GParamSpec *pspec)
{
  NotifyInMain *notify;

  g_return_if_fail (G_IS_OBJECT (instance));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  if G_LIKELY (IDE_IS_MAIN_THREAD ())
    {
      g_object_notify_by_pspec (instance, pspec);
      return;
    }

  notify = g_slice_new0 (NotifyInMain);
  notify->pspec = g_param_spec_ref (pspec);
  notify->object = g_object_ref (instance);

  g_timeout_add (0, ide_object_notify_in_main_cb, g_steal_pointer (&notify));
}

/**
 * ide_object_notify_in_main:
 * @instance: (type GObject.Object): a #GObject
 * @pspec: a #GParamSpec
 *
 * This helper will perform a g_object_notify_by_pspec() with the
 * added requirement that it is run from the applications main thread.
 *
 * You may want to do this when modifying state from a thread, but only
 * notify from the Gtk+ thread.
 *
 * This will *always* return to the default main context, and never
 * emit ::notify immediately.
 */
void
ide_object_notify_in_main (gpointer    instance,
                           GParamSpec *pspec)
{
  NotifyInMain *notify;

  g_return_if_fail (G_IS_OBJECT (instance));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  notify = g_slice_new0 (NotifyInMain);
  notify->pspec = g_param_spec_ref (pspec);
  notify->object = g_object_ref (instance);

  g_timeout_add (0, ide_object_notify_in_main_cb, g_steal_pointer (&notify));
}
