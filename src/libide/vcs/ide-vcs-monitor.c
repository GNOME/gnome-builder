/* ide-vcs-monitor.c
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

#define G_LOG_DOMAIN "ide-vcs-monitor"

#include "config.h"

#include <libide-core.h>
#include <libide-io.h>

#include "ide-marshal.h"

#include "ide-vcs.h"
#include "ide-vcs-file-info.h"
#include "ide-vcs-monitor.h"

struct _IdeVcsMonitor
{
  IdeObject                parent_instance;

  GFile                   *root;
  IdeVcs                  *vcs;
  GSignalGroup            *vcs_signals;
  IdeRecursiveFileMonitor *monitor;
  GSignalGroup            *monitor_signals;
  GHashTable              *status_by_file;

  guint                    cache_source;

  guint64                  last_change_seq;

  guint                    busy : 1;
};

G_DEFINE_FINAL_TYPE (IdeVcsMonitor, ide_vcs_monitor, IDE_TYPE_OBJECT)

enum {
  CHANGED,
  RELOADED,
  N_SIGNALS
};

enum {
  PROP_0,
  PROP_ROOT,
  PROP_VCS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_vcs_monitor_add_parents_locked (GHashTable       *hash,
                                    GFile            *file,
                                    GFile            *toplevel,
                                    IdeVcsFileStatus  status)
{
  GFile *parent;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (hash != NULL);
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_FILE (toplevel));

  parent = g_file_get_parent (file);

  while (g_file_has_prefix (parent, toplevel))
    {
      GFile *tmp = g_file_get_parent (parent);
      IdeVcsFileInfo *info;

      info = g_hash_table_lookup (hash, parent);

      if (info == NULL)
        {
          info = ide_vcs_file_info_new (parent);
          ide_vcs_file_info_set_status (info, status);
          g_hash_table_insert (hash, g_object_ref (parent), info);
        }
      else
        {
          /* Higher numeric values are more important */
          if (status > ide_vcs_file_info_get_status (info))
            ide_vcs_file_info_set_status (info, status);
        }

      g_object_unref (parent);
      parent = tmp;
    }

  g_object_unref (parent);
}

static void
ide_vcs_monitor_list_status_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeVcs *vcs = (IdeVcs *)object;
  g_autoptr(IdeVcsMonitor) self = user_data;
  g_autoptr(GListModel) model = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS (vcs));
  g_assert (IDE_IS_VCS_MONITOR (self));

  ide_object_lock (IDE_OBJECT (self));

  self->busy = FALSE;
  self->last_change_seq++;

  if ((model = ide_vcs_list_status_finish (vcs, result, NULL)))
    {
      g_autoptr(GHashTable) status_by_file = NULL;
      guint n_items;

      n_items = g_list_model_get_n_items (model);
      status_by_file = g_hash_table_new_full (g_file_hash,
                                              (GEqualFunc)g_file_equal,
                                              g_object_unref,
                                              g_object_unref);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeVcsFileInfo) info = NULL;
          IdeVcsFileStatus status;
          GFile *file;

          info = g_list_model_get_item (model, i);
          file = ide_vcs_file_info_get_file (info);
          status = ide_vcs_file_info_get_status (info);

          g_hash_table_insert (status_by_file,
                               g_file_dup (file),
                               g_steal_pointer (&info));

          ide_vcs_monitor_add_parents_locked (status_by_file, file, self->root, status);
        }

      g_clear_pointer (&self->status_by_file, g_hash_table_unref);
      self->status_by_file = g_steal_pointer (&status_by_file);

      g_signal_emit (self, signals [RELOADED], 0);
    }

  ide_object_unlock (IDE_OBJECT (self));

  IDE_EXIT;
}

static gboolean
ide_vcs_monitor_cache_cb (gpointer data)
{
  IdeVcsMonitor *self = data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS_MONITOR (self));

  ide_object_lock (IDE_OBJECT (self));

  self->cache_source = 0;

  if (self->vcs != NULL)
    {
      self->busy = TRUE;
      ide_vcs_list_status_async (self->vcs,
                                 self->root,
                                 TRUE,
                                 G_PRIORITY_LOW,
                                 NULL,
                                 ide_vcs_monitor_list_status_cb,
                                 g_object_ref (self));
    }

  ide_object_unlock (IDE_OBJECT (self));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_vcs_monitor_queue_reload (IdeVcsMonitor *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS_MONITOR (self));

  ide_object_lock (IDE_OBJECT (self));
  if (self->cache_source == 0 && !self->busy)
    self->cache_source = g_idle_add_full (G_PRIORITY_LOW,
                                          ide_vcs_monitor_cache_cb,
                                          g_object_ref (self),
                                          g_object_unref);
  ide_object_unlock (IDE_OBJECT (self));

  IDE_EXIT;
}

static void
ide_vcs_monitor_changed_cb (IdeVcsMonitor           *self,
                            GFile                   *file,
                            GFile                   *other_file,
                            GFileMonitorEvent        event,
                            IdeRecursiveFileMonitor *monitor)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS_MONITOR (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (monitor));

  self->last_change_seq++;

  g_signal_emit (self, signals[CHANGED], 0, file, other_file, event);

  ide_vcs_monitor_queue_reload (self);

  IDE_EXIT;
}

static void
ide_vcs_monitor_vcs_changed_cb (IdeVcsMonitor *self,
                                IdeVcs        *vcs)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS_MONITOR (self));
  g_assert (IDE_IS_VCS (vcs));

  /* Everything is invalidated by new VCS index, reload now */
  ide_object_lock (IDE_OBJECT (self));
  g_clear_pointer (&self->status_by_file, g_hash_table_unref);
  ide_vcs_monitor_queue_reload (self);
  ide_object_unlock (IDE_OBJECT (self));

  IDE_EXIT;
}

static DexFuture *
ide_vcs_monitor_ignore_func (GFile    *file,
                             gpointer  data)
{
  IdeVcsMonitor *self = data;
  DexFuture *ret;

  g_assert (IDE_IS_VCS_MONITOR (self));

  ide_object_lock (IDE_OBJECT (self));
  ret = ide_vcs_query_ignored (self->vcs, file);
  ide_object_unlock (IDE_OBJECT (self));

  return ret;
}

static void
ide_vcs_monitor_start_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  IdeRecursiveFileMonitor *monitor = (IdeRecursiveFileMonitor *)object;
  g_autoptr(IdeVcsMonitor) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RECURSIVE_FILE_MONITOR (monitor));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_VCS_MONITOR (self));

  if (!ide_recursive_file_monitor_start_finish (monitor, result, &error))
    g_warning ("%s", error->message);

  ide_vcs_monitor_queue_reload (self);
}

static void
ide_vcs_monitor_maybe_reload_locked (IdeVcsMonitor *self)
{
  IDE_ENTRY;

  g_assert (IDE_IS_VCS_MONITOR (self));

  g_clear_pointer (&self->status_by_file, g_hash_table_unref);
  g_clear_handle_id (&self->cache_source, g_source_remove);

  if (self->monitor)
    {
      g_signal_group_set_target (self->monitor_signals, NULL);
      ide_recursive_file_monitor_set_ignore_func (self->monitor, NULL, NULL, NULL);
      ide_recursive_file_monitor_cancel (self->monitor);
      g_clear_object (&self->monitor);
    }

  if (G_IS_FILE (self->root))
    {
      self->monitor = ide_recursive_file_monitor_new (self->root);

      if (self->vcs != NULL)
        ide_recursive_file_monitor_set_ignore_func (self->monitor,
                                                    ide_vcs_monitor_ignore_func,
                                                    self, NULL);

      g_signal_group_set_target (self->monitor_signals, self->monitor);

      ide_recursive_file_monitor_start_async (self->monitor,
                                              NULL,
                                              ide_vcs_monitor_start_cb,
                                              g_object_ref (self));
    }

  IDE_EXIT;
}

static void
ide_vcs_monitor_destroy (IdeObject *object)
{
  IdeVcsMonitor *self = (IdeVcsMonitor *)object;

  g_clear_handle_id (&self->cache_source, g_source_remove);
  g_clear_pointer (&self->status_by_file, g_hash_table_unref);

  if (self->monitor != NULL)
    {
      ide_recursive_file_monitor_set_ignore_func (self->monitor, NULL, NULL, NULL);
      ide_recursive_file_monitor_cancel (self->monitor);
      g_clear_object (&self->monitor);
    }

  g_signal_group_set_target (self->monitor_signals, NULL);
  g_signal_group_set_target (self->vcs_signals, NULL);

  g_clear_object (&self->vcs);

  IDE_OBJECT_CLASS (ide_vcs_monitor_parent_class)->destroy (object);
}

static void
ide_vcs_monitor_finalize (GObject *object)
{
  IdeVcsMonitor *self = (IdeVcsMonitor *)object;

  g_clear_pointer (&self->status_by_file, g_hash_table_unref);
  g_clear_object (&self->root);
  g_clear_object (&self->monitor_signals);
  g_clear_object (&self->vcs_signals);

  G_OBJECT_CLASS (ide_vcs_monitor_parent_class)->finalize (object);
}

static void
ide_vcs_monitor_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeVcsMonitor *self = IDE_VCS_MONITOR (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      g_value_take_object (value, ide_vcs_monitor_ref_root (self));
      break;

    case PROP_VCS:
      g_value_take_object (value, ide_vcs_monitor_ref_vcs (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_vcs_monitor_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeVcsMonitor *self = IDE_VCS_MONITOR (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      ide_vcs_monitor_set_root (self, g_value_get_object (value));
      break;

    case PROP_VCS:
      ide_vcs_monitor_set_vcs (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_vcs_monitor_class_init (IdeVcsMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_vcs_monitor_finalize;
  object_class->get_property = ide_vcs_monitor_get_property;
  object_class->set_property = ide_vcs_monitor_set_property;

  i_object_class->destroy = ide_vcs_monitor_destroy;

  /**
   * IdeVcsMonitor:root:
   *
   * The "root" property is the root of the file-system to begin
   * monitoring for changes.
   */
  properties [PROP_ROOT] =
    g_param_spec_object ("root",
                         "Root",
                         "The root of the directory tree",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeVcsMonitor:vcs:
   *
   * The "vcs" property is the version control system to be queried for
   * additional status information when a file has been discovered to
   * have been changed.
   */
  properties [PROP_VCS] =
    g_param_spec_object ("vcs",
                         "VCS",
                         "The version control system in use",
                         IDE_TYPE_VCS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeVcsMonitor::changed:
   * @self: an #IdeVcsMonitor
   * @file: a #GFile
   * @other_file: (nullable): a #GFile or %NULL
   * @event: a #GFileMonitorEvent
   *
   * The "changed" signal is emitted when a file has been discovered to
   * have been changed on disk.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT_OBJECT_ENUM,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_FILE_MONITOR_EVENT);
  g_signal_set_va_marshaller (signals [CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECT_OBJECT_ENUMv);

  /**
   * IdeVcsMonitor::reloaded:
   * @self: an #IdeVcsMonitor
   *
   * The "reloaded" signal is emitted when the monitor has been reloaded.
   */
  signals [RELOADED] =
    g_signal_new ("reloaded",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
ide_vcs_monitor_init (IdeVcsMonitor *self)
{
  self->last_change_seq = 1;

  self->monitor_signals = g_signal_group_new (IDE_TYPE_RECURSIVE_FILE_MONITOR);

  g_signal_group_connect_object (self->monitor_signals,
                                   "changed",
                                   G_CALLBACK (ide_vcs_monitor_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->vcs_signals = g_signal_group_new (IDE_TYPE_VCS);

  g_signal_group_connect_object (self->vcs_signals,
                                   "changed",
                                   G_CALLBACK (ide_vcs_monitor_vcs_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
}

/**
 * ide_vcs_monitor_ref_info:
 * @self: a #IdeVcsMonitor
 * @file: a #GFile
 *
 * Gets an #IdeVcsFileInfo for the given @file.
 *
 * If the file information has not been loaded, %NULL is returned. You
 * can wait for #IdeVcsMonitor::reloaded and query again if you expect
 * the info to be there.
 *
 * Returns: (transfer full) (nullable): an #IdeVcsFileInfo or %NULL
 */
IdeVcsFileInfo *
ide_vcs_monitor_ref_info (IdeVcsMonitor *self,
                          GFile         *file)
{
  IdeVcsFileInfo *info = NULL;

  g_return_val_if_fail (IDE_IS_VCS_MONITOR (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  if (self->status_by_file != NULL)
    {
      if ((info = g_hash_table_lookup (self->status_by_file, file)))
        g_object_ref (info);
    }
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&info);
}

/**
 * ide_vcs_monitor_ref_vcs:
 * @self: a #IdeVcsMonitor
 *
 * Increments the reference count of the #IdeVcs monitored using the
 * #IdeVcsMonitor and returns it.
 *
 * Returns: (transfer full) (nullable): an #IdeVcs or %NULL
 */
IdeVcs *
ide_vcs_monitor_ref_vcs (IdeVcsMonitor *self)
{
  IdeVcs *ret;

  g_return_val_if_fail (IDE_IS_VCS_MONITOR (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = self->vcs ? g_object_ref (self->vcs) : NULL;
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

/**
 * ide_vcs_monitor_ref_root:
 * @self: a #IdeVcsMonitor
 *
 * Gets the #IdeVcsMonitor:root property and increments the reference
 * count of the #GFile by one.
 *
 * Returns: (transfer full) (nullable): a #GFile or %NULL
 */
GFile *
ide_vcs_monitor_ref_root (IdeVcsMonitor *self)
{
  GFile *ret;

  g_return_val_if_fail (IDE_IS_VCS_MONITOR (self), NULL);

  ide_object_lock (IDE_OBJECT (self));
  ret = self->root ? g_object_ref (self->root) : NULL;
  ide_vcs_monitor_maybe_reload_locked (self);
  ide_object_unlock (IDE_OBJECT (self));

  return g_steal_pointer (&ret);
}

void
ide_vcs_monitor_set_root (IdeVcsMonitor *self,
                          GFile         *root)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_VCS_MONITOR (self));
  g_return_if_fail (G_IS_FILE (root));

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_object (&self->root, root))
    {
      ide_object_notify_by_pspec (self, properties [PROP_ROOT]);
      ide_vcs_monitor_maybe_reload_locked (self);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

void
ide_vcs_monitor_set_vcs (IdeVcsMonitor *self,
                         IdeVcs        *vcs)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_VCS_MONITOR (self));
  g_return_if_fail (!vcs || IDE_IS_VCS (vcs));

  ide_object_lock (IDE_OBJECT (self));
  if (g_set_object (&self->vcs, vcs))
    {
      g_signal_group_set_target (self->vcs_signals, vcs);
      ide_object_notify_by_pspec (self, properties [PROP_VCS]);
      ide_vcs_monitor_maybe_reload_locked (self);
    }
  ide_object_unlock (IDE_OBJECT (self));
}

guint64
ide_vcs_monitor_get_sequence (IdeVcsMonitor *self)
{
  g_return_val_if_fail (IDE_IS_VCS_MONITOR (self), 0);

  return self->last_change_seq;
}

/**
 * ide_vcs_monitor_from_context:
 * @context: an #IdeContext
 *
 * Gets the #IdeVcsMonitor for a context.
 *
 * Returns: (nullable) (transfer none): an #IdeVcsMonitor
 */
IdeVcsMonitor *
ide_vcs_monitor_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ide_context_peek_child_typed (context, IDE_TYPE_VCS_MONITOR);
}
