/* ide-vcs-monitor.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-vcs-monitor"

#include "config.h"

#include <dazzle.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "vcs/ide-vcs.h"
#include "vcs/ide-vcs-file-info.h"
#include "vcs/ide-vcs-monitor.h"

struct _IdeVcsMonitor
{
  IdeObject                parent_instance;

  GFile                   *root;
  DzlRecursiveFileMonitor *monitor;
  GHashTable              *status_by_file;

  guint                    cache_source;

  guint                    busy : 1;
};

G_DEFINE_TYPE (IdeVcsMonitor, ide_vcs_monitor, IDE_TYPE_OBJECT)

enum {
  CHANGED,
  RELOADED,
  N_SIGNALS
};

enum {
  PROP_0,
  PROP_ROOT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_vcs_monitor_add_parents (GHashTable       *hash,
                             GFile            *file,
                             GFile            *toplevel,
                             IdeVcsFileStatus  status)
{
  GFile *parent;

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
  g_autoptr(GHashTable) status_by_file = NULL;
  GFile *workdir;
  guint n_items;

  g_assert (IDE_IS_VCS (vcs));
  g_assert (IDE_IS_VCS_MONITOR (self));

  self->busy = FALSE;

  model = ide_vcs_list_status_finish (vcs, result, NULL);
  if (model == NULL)
    return;

  n_items = g_list_model_get_n_items (model);
  workdir = ide_vcs_get_working_directory (vcs);
  status_by_file = g_hash_table_new_full (g_file_hash,
                                          (GEqualFunc) g_file_equal,
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
                           g_object_ref (file),
                           g_steal_pointer (&info));

      ide_vcs_monitor_add_parents (status_by_file, file, workdir, status);
    }

  dzl_clear_pointer (&self->status_by_file, g_hash_table_unref);
  self->status_by_file = g_steal_pointer (&status_by_file);

  g_signal_emit (self, signals[RELOADED], 0);
}

static gboolean
ide_vcs_monitor_cache_cb (gpointer data)
{
  IdeVcsMonitor *self = data;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (IDE_IS_VCS_MONITOR (self));

  self->cache_source = 0;

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  self->busy = TRUE;

  ide_vcs_list_status_async (vcs,
                             workdir,
                             TRUE,
                             G_PRIORITY_LOW,
                             NULL,
                             ide_vcs_monitor_list_status_cb,
                             g_object_ref (self));

  return G_SOURCE_REMOVE;
}

static void
ide_vcs_monitor_queue_reload (IdeVcsMonitor *self)
{
  g_assert (IDE_IS_VCS_MONITOR (self));

  if (self->cache_source == 0 && !self->busy)
    self->cache_source = g_idle_add_full (G_PRIORITY_LOW,
                                          ide_vcs_monitor_cache_cb,
                                          g_object_ref (self),
                                          g_object_unref);
}

static void
ide_vcs_monitor_changed_cb (IdeVcsMonitor           *self,
                            GFile                   *file,
                            GFile                   *other_file,
                            GFileMonitorEvent        event,
                            DzlRecursiveFileMonitor *monitor)
{
  IDE_ENTRY;

  g_assert (IDE_IS_VCS_MONITOR (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (DZL_IS_RECURSIVE_FILE_MONITOR (monitor));

  g_signal_emit (self, signals[CHANGED], 0, file, other_file, event);

  ide_vcs_monitor_queue_reload (self);

  IDE_EXIT;
}

static void
ide_vcs_monitor_vcs_changed_cb (IdeVcsMonitor *self,
                                IdeVcs        *vcs)
{
  IDE_ENTRY;

  g_assert (IDE_IS_VCS_MONITOR (self));
  g_assert (IDE_IS_VCS (vcs));

  /* Everything is invalidated by new VCS index, reload now */
  dzl_clear_pointer (&self->status_by_file, g_hash_table_unref);
  ide_vcs_monitor_queue_reload (self);

  IDE_EXIT;
}

static gboolean
ide_vcs_monitor_ignore_func (GFile    *file,
                             gpointer  data)
{
  IdeVcsMonitor *self = data;
  IdeContext *context;
  IdeVcs *vcs;

  g_assert (IDE_IS_VCS_MONITOR (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);

  return ide_vcs_is_ignored (vcs, file, NULL);
}

static void
ide_vcs_monitor_start_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  DzlRecursiveFileMonitor *monitor = (DzlRecursiveFileMonitor *)object;
  g_autoptr(IdeVcsMonitor) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DZL_IS_RECURSIVE_FILE_MONITOR (monitor));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_VCS_MONITOR (self));

  if (!dzl_recursive_file_monitor_start_finish (monitor, result, &error))
    g_warning ("%s", error->message);

  ide_vcs_monitor_queue_reload (self);
}

static void
ide_vcs_monitor_constructed (GObject *object)
{
  IdeVcsMonitor *self = (IdeVcsMonitor *)object;
  IdeContext *context;
  IdeVcs *vcs;

  G_OBJECT_CLASS (ide_vcs_monitor_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (ide_vcs_monitor_vcs_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->monitor = dzl_recursive_file_monitor_new (self->root);

  dzl_recursive_file_monitor_set_ignore_func (self->monitor,
                                              ide_vcs_monitor_ignore_func,
                                              self, NULL);

  g_signal_connect_object (self->monitor,
                           "changed",
                           G_CALLBACK (ide_vcs_monitor_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  dzl_recursive_file_monitor_start_async (self->monitor,
                                          NULL,
                                          ide_vcs_monitor_start_cb,
                                          g_object_ref (self));
}

static void
ide_vcs_monitor_dispose (GObject *object)
{
  IdeVcsMonitor *self = (IdeVcsMonitor *)object;

  dzl_clear_source (&self->cache_source);
  dzl_clear_pointer (&self->status_by_file, g_hash_table_unref);

  if (self->monitor != NULL)
    {
      dzl_recursive_file_monitor_set_ignore_func (self->monitor, NULL, NULL, NULL);
      dzl_recursive_file_monitor_cancel (self->monitor);
      g_clear_object (&self->monitor);
    }

  G_OBJECT_CLASS (ide_vcs_monitor_parent_class)->dispose (object);
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
      g_value_set_object (value, self->root);
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
      self->root = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_vcs_monitor_class_init (IdeVcsMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_vcs_monitor_constructed;
  object_class->dispose = ide_vcs_monitor_dispose;
  object_class->get_property = ide_vcs_monitor_get_property;
  object_class->set_property = ide_vcs_monitor_set_property;

  properties [PROP_ROOT] =
    g_param_spec_object ("root",
                         "Root",
                         "The root of the directory tree",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_FILE_MONITOR_EVENT);

  signals [RELOADED] =
    g_signal_new ("reloaded",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
ide_vcs_monitor_init (IdeVcsMonitor *self)
{
}

/**
 * ide_vcs_monitor_get_info:
 * @self: a #IdeVcsMonitor
 * @file: a #GFile
 *
 * Gets an #IdeVcsFileInfo for the given @file.
 *
 * If the file information has not been loaded, %NULL is returned. You
 * can wait for #IdeVcsMonitor::reloaded and query again if you expect
 * the info to be there.
 * 
 * Returns: (transfer none) (nullable): an #IdeVcsFileInfo or %NULL
 *
 * Since: 3.28
 */
IdeVcsFileInfo *
ide_vcs_monitor_get_info (IdeVcsMonitor *self,
                          GFile         *file)
{
  IdeVcsFileInfo *info;

  g_return_val_if_fail (IDE_IS_VCS_MONITOR (self), NULL);

  if (self->status_by_file == NULL)
    return NULL;

  info = g_hash_table_lookup (self->status_by_file, file);
  if (info == NULL)
    return NULL;

  return g_object_ref (info);
}
