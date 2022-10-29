/* ide-debugger.c
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

#define G_LOG_DOMAIN "ide-debugger"

#include "config.h"

#include "ide-debugger.h"
#include "ide-debugger-address-map-private.h"
#include "ide-debugger-private.h"

/**
 * SECTION:ide-debugger
 * @title: IdeDebugger
 * @short_description: Base class for debugger implementations
 *
 * The IdeDebugger abstract base class is used by debugger implementations.
 * They should bridge their backend-specific features into those supported
 * by the API using the series of "emit" functions provided as part of
 * this class.
 *
 * For example, when the inferior creates a new thread, the debugger
 * implementation should call ide_debugger_emit_thread_added().
 */

typedef struct
{
  gchar                 *display_name;
  GListStore            *breakpoints;
  GListStore            *threads;
  GListStore            *thread_groups;
  IdeDebuggerThread     *selected;
  IdeDebuggerAddressMap *map;
  guint                  has_started : 1;
  guint                  is_running : 1;
} IdeDebuggerPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeDebugger, ide_debugger, IDE_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeDebugger)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP,
                                                         _ide_debugger_class_init_actions))

enum {
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_SELECTED_THREAD,
  N_PROPS
};

enum {
  LOG,

  THREAD_GROUP_ADDED,
  THREAD_GROUP_EXITED,
  THREAD_GROUP_REMOVED,
  THREAD_GROUP_STARTED,

  THREAD_ADDED,
  THREAD_REMOVED,
  THREAD_SELECTED,

  BREAKPOINT_ADDED,
  BREAKPOINT_MODIFIED,
  BREAKPOINT_REMOVED,

  RUNNING,
  STOPPED,

  LIBRARY_LOADED,
  LIBRARY_UNLOADED,

  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_debugger_real_breakpoint_added (IdeDebugger           *self,
                                    IdeDebuggerBreakpoint *breakpoint)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  g_debug ("Added breakpoint %s",
           ide_debugger_breakpoint_get_id (breakpoint));

  g_list_store_insert_sorted (priv->breakpoints,
                              breakpoint,
                              (GCompareDataFunc)ide_debugger_breakpoint_compare,
                              NULL);
}

static void
ide_debugger_real_breakpoint_removed (IdeDebugger           *self,
                                      IdeDebuggerBreakpoint *breakpoint)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);
  guint n_items;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  g_debug ("Removed breakpoint %s",
           ide_debugger_breakpoint_get_id (breakpoint));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->breakpoints));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeDebuggerBreakpoint) element = NULL;

      element = g_list_model_get_item (G_LIST_MODEL (priv->breakpoints), i);

      g_assert (element != NULL);
      g_assert (IDE_IS_DEBUGGER_BREAKPOINT (element));

      if (breakpoint == element)
        break;

      if (ide_debugger_breakpoint_compare (breakpoint, element) == 0)
        {
          g_list_store_remove (priv->breakpoints, i);
          break;
        }
    }
}

static void
ide_debugger_real_breakpoint_modified (IdeDebugger           *self,
                                       IdeDebuggerBreakpoint *breakpoint)
{
  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  g_debug ("Modified breakpoint %s (%s)",
           ide_debugger_breakpoint_get_id (breakpoint),
           ide_debugger_breakpoint_get_enabled (breakpoint) ?  "enabled" : "disabled");

  /*
   * If we add API to GListStore, we could make this a single
   * operation instead of 2 signal emissions.
   */
  ide_debugger_real_breakpoint_removed (self, breakpoint);
  ide_debugger_real_breakpoint_added (self, breakpoint);
}

static void
ide_debugger_real_thread_group_added (IdeDebugger            *self,
                                      IdeDebuggerThreadGroup *thread_group)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));

  g_debug ("Added thread group %s",
           ide_debugger_thread_group_get_id (thread_group));

  g_list_store_insert_sorted (priv->thread_groups,
                              thread_group,
                              (GCompareDataFunc)ide_debugger_thread_group_compare,
                              NULL);
}

static void
ide_debugger_real_thread_group_removed (IdeDebugger            *self,
                                        IdeDebuggerThreadGroup *thread_group)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);
  guint n_items;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));

  g_debug ("Removed thread group %s",
           ide_debugger_thread_group_get_id (thread_group));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->thread_groups));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeDebuggerThreadGroup) element = NULL;

      element = g_list_model_get_item (G_LIST_MODEL (priv->thread_groups), i);

      g_assert (element != NULL);
      g_assert (IDE_IS_DEBUGGER_THREAD_GROUP (element));

      if (thread_group == element)
        break;

      if (ide_debugger_thread_group_compare (thread_group, element) == 0)
        {
          g_list_store_remove (priv->thread_groups, i);
          break;
        }
    }
}

static void
ide_debugger_real_thread_added (IdeDebugger       *self,
                                IdeDebuggerThread *thread)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));

  g_debug ("Added thread %s", ide_debugger_thread_get_id (thread));

  g_list_store_insert_sorted (priv->threads,
                              thread,
                              (GCompareDataFunc)ide_debugger_thread_compare,
                              NULL);
}

static void
ide_debugger_real_thread_removed (IdeDebugger       *self,
                                  IdeDebuggerThread *thread)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);
  guint n_items;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));

  g_debug ("Removed thread %s", ide_debugger_thread_get_id (thread));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (priv->threads));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeDebuggerThread) element = NULL;

      element = g_list_model_get_item (G_LIST_MODEL (priv->threads), i);

      g_assert (element != NULL);
      g_assert (IDE_IS_DEBUGGER_THREAD (element));

      if (thread == element)
        break;

      if (ide_debugger_thread_compare (thread, element) == 0)
        {
          g_list_store_remove (priv->threads, i);
          break;
        }
    }
}

static void
ide_debugger_real_running (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_assert (IDE_IS_DEBUGGER (self));

  priv->is_running = TRUE;
  priv->has_started = TRUE;

  _ide_debugger_update_actions (self);
}

static void
ide_debugger_real_stopped (IdeDebugger           *self,
                           IdeDebuggerStopReason  stop_reason,
                           IdeDebuggerBreakpoint *breakpoint)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_STOP_REASON (stop_reason));
  g_assert (!breakpoint || IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  /* We might need to eventually track this by thread group */
  priv->is_running = FALSE;

  if (IDE_DEBUGGER_STOP_IS_TERMINAL (stop_reason))
    priv->has_started = FALSE;

  _ide_debugger_update_actions (self);
}

static void
ide_debugger_real_thread_selected (IdeDebugger       *self,
                                   IdeDebuggerThread *thread)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));

  if (g_set_object (&priv->selected, thread))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTED_THREAD]);
}

static void
ide_debugger_finalize (GObject *object)
{
  IdeDebugger *self = (IdeDebugger *)object;
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_clear_pointer (&priv->map, ide_debugger_address_map_free);
  g_clear_pointer (&priv->display_name, g_free);
  g_clear_object (&priv->breakpoints);
  g_clear_object (&priv->threads);
  g_clear_object (&priv->thread_groups);
  g_clear_object (&priv->selected);

  G_OBJECT_CLASS (ide_debugger_parent_class)->finalize (object);
}

static gboolean
ide_debugger_real_get_can_move (IdeDebugger         *self,
                                IdeDebuggerMovement  movement)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_DEBUGGER_MOVEMENT (movement), FALSE);

  switch (movement)
    {
    case IDE_DEBUGGER_MOVEMENT_START:
      return !ide_debugger_get_is_running (self);

    case IDE_DEBUGGER_MOVEMENT_FINISH:
    case IDE_DEBUGGER_MOVEMENT_CONTINUE:
    case IDE_DEBUGGER_MOVEMENT_STEP_IN:
    case IDE_DEBUGGER_MOVEMENT_STEP_OVER:
      return _ide_debugger_get_has_started (self) &&
             !ide_debugger_get_is_running (self);

    default:
      g_return_val_if_reached (FALSE);
    }
}

static void
ide_debugger_real_library_loaded (IdeDebugger        *self,
                                  IdeDebuggerLibrary *library)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);
  IdeDebuggerAddressMapEntry entry = { 0 };
  GPtrArray *ranges;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_LIBRARY (library));

  /* We don't yet have this information */
  entry.offset = 0;
  entry.filename = ide_debugger_library_get_target_name (library);

  ranges = ide_debugger_library_get_ranges (library);

  if (ranges != NULL)
    {
      for (guint i = 0; i < ranges->len; i++)
        {
          const IdeDebuggerAddressRange *range = g_ptr_array_index (ranges, i);

          entry.start = range->from;
          entry.end = range->to;

          ide_debugger_address_map_insert (priv->map, &entry);
        }
    }
}

static void
ide_debugger_real_library_unloaded (IdeDebugger        *self,
                                    IdeDebuggerLibrary *library)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);
  GPtrArray *ranges;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_LIBRARY (library));

  ranges = ide_debugger_library_get_ranges (library);

  if (ranges != NULL)
    {
      for (guint i = 0; i < ranges->len; i++)
        {
          const IdeDebuggerAddressRange *range = g_ptr_array_index (ranges, i);

          ide_debugger_address_map_remove (priv->map, range->from);
        }
    }
}

static void
ide_debugger_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeDebugger *self = IDE_DEBUGGER (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_debugger_get_display_name (self));
      break;

    case PROP_SELECTED_THREAD:
      g_value_set_object (value, ide_debugger_get_selected_thread (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeDebugger *self = IDE_DEBUGGER (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      ide_debugger_set_display_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_class_init (IdeDebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_finalize;
  object_class->get_property = ide_debugger_get_property;
  object_class->set_property = ide_debugger_set_property;

  klass->breakpoint_added = ide_debugger_real_breakpoint_added;
  klass->breakpoint_modified = ide_debugger_real_breakpoint_modified;
  klass->breakpoint_removed = ide_debugger_real_breakpoint_removed;
  klass->disassemble_async = _ide_debugger_real_disassemble_async;
  klass->disassemble_finish = _ide_debugger_real_disassemble_finish;
  klass->get_can_move = ide_debugger_real_get_can_move;
  klass->interrupt_async = _ide_debugger_real_interrupt_async;
  klass->interrupt_finish = _ide_debugger_real_interrupt_finish;
  klass->library_loaded = ide_debugger_real_library_loaded;
  klass->library_unloaded = ide_debugger_real_library_unloaded;
  klass->list_frames_async = _ide_debugger_real_list_frames_async;
  klass->list_frames_finish = _ide_debugger_real_list_frames_finish;
  klass->list_locals_async = _ide_debugger_real_list_locals_async;
  klass->list_locals_finish = _ide_debugger_real_list_locals_finish;
  klass->list_params_async = _ide_debugger_real_list_params_async;
  klass->list_params_finish = _ide_debugger_real_list_params_finish;
  klass->list_registers_async = _ide_debugger_real_list_registers_async;
  klass->list_registers_finish = _ide_debugger_real_list_registers_finish;
  klass->modify_breakpoint_async = _ide_debugger_real_modify_breakpoint_async;
  klass->modify_breakpoint_finish = _ide_debugger_real_modify_breakpoint_finish;
  klass->running = ide_debugger_real_running;
  klass->send_signal_async = _ide_debugger_real_send_signal_async;
  klass->send_signal_finish = _ide_debugger_real_send_signal_finish;
  klass->stopped = ide_debugger_real_stopped;
  klass->thread_added = ide_debugger_real_thread_added;
  klass->thread_group_added = ide_debugger_real_thread_group_added;
  klass->thread_group_removed = ide_debugger_real_thread_group_removed;
  klass->thread_removed = ide_debugger_real_thread_removed;
  klass->thread_selected = ide_debugger_real_thread_selected;
  klass->interpret_async = _ide_debugger_real_interpret_async;
  klass->interpret_finish = _ide_debugger_real_interpret_finish;

  /**
   * IdeDebugger:display-name:
   *
   * The "display-name" property is used by UI to when it is necessary
   * to display the name of the debugger. You might set this to "GNU Debugger"
   * or "Python Debugger", etc.
   */
  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "The name of the debugger to use in various UI components",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebugger:selected-thread:
   *
   * The currently selected thread.
   */
  properties [PROP_SELECTED_THREAD] =
    g_param_spec_object ("selected-thread",
                         "Selected Thread",
                         "The currently selected thread",
                         IDE_TYPE_DEBUGGER_THREAD,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeDebugger::log:
   * @self: An #IdeDebugger.
   * @stream: the stream to append to.
   * @content: the contents for the stream.
   *
   * The "log" signal is emitted when there is new content to be
   * appended to one of the streams.
   */
  signals [LOG] =
    g_signal_new ("log",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, log),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_DEBUGGER_STREAM,
                  G_TYPE_BYTES | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * IdeDebugger::thread-group-added:
   * @self: an #IdeDebugger
   * @thread_group: an #IdeDebuggerThreadGroup
   *
   * This signal is emitted when a thread-group has been added.
   */
  signals [THREAD_GROUP_ADDED] =
    g_signal_new ("thread-group-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, thread_group_added),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_DEBUGGER_THREAD_GROUP);

  /**
   * IdeDebugger::thread-group-removed:
   * @self: an #IdeDebugger
   * @thread_group: an #IdeDebuggerThreadGroup
   *
   * This signal is emitted when a thread-group has been removed.
   */
  signals [THREAD_GROUP_REMOVED] =
    g_signal_new ("thread-group-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, thread_group_removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_DEBUGGER_THREAD_GROUP);

  /**
   * IdeDebugger::thread-group-started:
   * @self: an #IdeDebugger
   * @thread_group: an #IdeDebuggerThreadGroup
   *
   * This signal is emitted when a thread-group has been started.
   */
  signals [THREAD_GROUP_STARTED] =
    g_signal_new ("thread-group-started",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, thread_group_started),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_DEBUGGER_THREAD_GROUP);

  /**
   * IdeDebugger::thread-group-exited:
   * @self: an #IdeDebugger
   * @thread_group: an #IdeDebuggerThreadGroup
   *
   * This signal is emitted when a thread-group has exited.
   */
  signals [THREAD_GROUP_EXITED] =
    g_signal_new ("thread-group-exited",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, thread_group_exited),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_THREAD_GROUP);

  /**
   * IdeDebugger::thread-added:
   * @self: an #IdeDebugger
   * @thread: an #IdeDebuggerThread
   *
   * The signal is emitted when a thread is added to the inferior.
   */
  signals [THREAD_ADDED] =
    g_signal_new ("thread-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, thread_added),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_THREAD);

  /**
   * IdeDebugger::thread-removed:
   * @self: an #IdeDebugger
   * @thread: an #IdeDebuggerThread
   *
   * The signal is emitted when a thread is removed from the inferior.
   */
  signals [THREAD_REMOVED] =
    g_signal_new ("thread-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, thread_removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_THREAD);

  /**
   * IdeDebugger::thread-selected:
   * @self: an #IdeDebugger
   * @thread: an #IdeDebuggerThread
   *
   * The signal is emitted when a thread is selected in the debugger.
   */
  signals [THREAD_SELECTED] =
    g_signal_new ("thread-selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, thread_selected),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_THREAD);

  /**
   * IdeDebugger::breakpoint-added:
   * @self: an #IdeDebugger
   * @breakpoint: an #IdeDebuggerBreakpoint
   *
   * The "breakpoint-added" signal is emitted when a breakpoint has been
   * added to the debugger.
   */
  signals [BREAKPOINT_ADDED] =
    g_signal_new ("breakpoint-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, breakpoint_added),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_BREAKPOINT);

  /**
   * IdeDebugger::breakpoint-removed:
   * @self: an #IdeDebugger
   * @breakpoint: an #IdeDebuggerBreakpoint
   *
   * The "breakpoint-removed" signal is emitted when a breakpoint has been
   * removed from the debugger.
   */
  signals [BREAKPOINT_REMOVED] =
    g_signal_new ("breakpoint-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, breakpoint_removed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_BREAKPOINT);

  /**
   * IdeDebugger::breakpoint-modified:
   * @self: an #IdeDebugger
   * @breakpoint: an #IdeDebuggerBreakpoint
   *
   * The "breakpoint-modified" signal is emitted when a breakpoint has been
   * modified by the debugger.
   */
  signals [BREAKPOINT_MODIFIED] =
    g_signal_new ("breakpoint-modified",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, breakpoint_modified),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_BREAKPOINT);

  /**
   * IdeDebugger::running:
   * @self: a #IdeDebugger
   *
   * This signal is emitted when the debugger starts or resumes executing
   * the inferior.
   */
  signals [RUNNING] =
    g_signal_new ("running",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, running),
                  NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * IdeDebugger::stopped:
   * @self: a #IdeDebugger
   * @stop_reason: An #IdeDebuggerStopReason
   * @breakpoint: (nullable): An #IdeDebuggerBreakpoint if any
   *
   * This signal is emitted when the debugger has stopped executing the
   * inferior for a variety of reasons.
   *
   * If possible, the debugger implementation will provide the breakpoint of
   * the location the debugger stopped. That location may not always be
   * representable by source in the project (such as memory address based
   * breakpoints).
   */
  signals [STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, stopped),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_DEBUGGER_STOP_REASON,
                  IDE_TYPE_DEBUGGER_BREAKPOINT);

  /**
   * IdeDebugger::library-loaded:
   * @self: An #IdeDebugger
   * @library: An #IdeDebuggerLibrary
   *
   * This signal is emitted when a library has been loaded by the debugger.
   */
  signals [LIBRARY_LOADED] =
    g_signal_new ("library-loaded",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, library_loaded),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_LIBRARY);

  /**
   * IdeDebugger::library-unloaded:
   * @self: An #IdeDebugger
   * @library: An #IdeDebuggerLibrary
   *
   * This signal is emitted when a library has been unloaded by the debugger.
   * Generally, this means that the library was a module and loaded in such a
   * way that allowed unloading.
   */
  signals [LIBRARY_UNLOADED] =
    g_signal_new ("library-unloaded",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerClass, library_unloaded),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_LIBRARY);
}

static void
ide_debugger_init (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  priv->breakpoints = g_list_store_new (IDE_TYPE_DEBUGGER_BREAKPOINT);
  priv->map = ide_debugger_address_map_new ();
  priv->thread_groups = g_list_store_new (IDE_TYPE_DEBUGGER_THREAD_GROUP);
  priv->threads = g_list_store_new (IDE_TYPE_DEBUGGER_THREAD);
}

/**
 * ide_debugger_get_display_name:
 * @self: a #IdeDebugger
 *
 * Gets the display name for the debugger as the user should see it in various
 * UI components.
 *
 * Returns: The display name for the debugger
 */
const gchar *
ide_debugger_get_display_name (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);

  return priv->display_name;
}

/**
 * ide_debugger_set_display_name:
 * @self: a #IdeDebugger
 *
 * Sets the #IdeDebugger:display-name property.
 */
void
ide_debugger_set_display_name (IdeDebugger *self,
                               const gchar *display_name)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_return_if_fail (IDE_IS_DEBUGGER (self));

  if (g_set_str (&priv->display_name, display_name))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
    }
}

/**
 * ide_debugger_get_can_move:
 * @self: a #IdeDebugger
 * @movement: the movement to check
 *
 * Checks to see if the debugger can make the movement matching @movement.
 *
 * Returns: %TRUE if @movement can be performed.
 */
gboolean
ide_debugger_get_can_move (IdeDebugger         *self,
                           IdeDebuggerMovement  movement)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);

  if (IDE_DEBUGGER_GET_CLASS (self)->get_can_move)
    return IDE_DEBUGGER_GET_CLASS (self)->get_can_move (self, movement);

  return FALSE;
}

/**
 * ide_debugger_move_async:
 * @self: a #IdeDebugger
 * @movement: An #IdeDebuggerMovement
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (scope async) (closure user_data): A callback to call upon
 *   completion of the operation.
 * @user_data: user data for @callback
 *
 * Advances the debugger to the next breakpoint or until the debugger stops.
 * @movement should describe the type of movement to perform.
 */
void
ide_debugger_move_async (IdeDebugger         *self,
                         IdeDebuggerMovement  movement,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_MOVEMENT (movement));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->move_async (self, movement, cancellable, callback, user_data);
}

/**
 * ide_debugger_move_finish:
 * @self: a #IdeDebugger
 * @result: a #GAsyncResult provided to the callback
 * @error: A location for a #GError, or %NULL
 *
 * Notifies that the movement request has been submitted to the debugger.
 *
 * Note that this does not indicate that the movement has completed successfully,
 * only that the command has be submitted.
 *
 * Returns: %TRUE if successful, otherwise %FALSE
 */
gboolean
ide_debugger_move_finish (IdeDebugger   *self,
                          GAsyncResult  *result,
                          GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEBUGGER_GET_CLASS (self)->move_finish (self, result, error);
}

/**
 * ide_debugger_emit_log:
 * @self: a #IdeDebugger
 *
 * Emits the "log" signal.
 *
 * Debugger implementations should use this to notify any listeners
 * that incoming log information has been recieved.
 *
 * Use the #IdeDebuggerStream to denote the particular stream.
 */
void
ide_debugger_emit_log (IdeDebugger       *self,
                       IdeDebuggerStream  stream,
                       GBytes            *content)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_STREAM (stream));
  g_return_if_fail (content != NULL);

  g_signal_emit (self, signals [LOG], 0, stream, content);
}

/**
 * ide_debugger_thread_group_added:
 * @self: an #IdeDebugger
 * @thread_group: an #IdeDebuggerThreadGroup
 *
 * Debugger implementations should call this to notify that a thread group has
 * been added to the inferior.
 */
void
ide_debugger_emit_thread_group_added (IdeDebugger            *self,
                                      IdeDebuggerThreadGroup *thread_group)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));

  g_signal_emit (self, signals [THREAD_GROUP_ADDED], 0, thread_group);
}

/**
 * ide_debugger_thread_group_removed:
 * @self: an #IdeDebugger
 * @thread_group: an #IdeDebuggerThreadGroup
 *
 * Debugger implementations should call this to notify that a thread group has
 * been removed from the inferior.
 */
void
ide_debugger_emit_thread_group_removed (IdeDebugger            *self,
                                        IdeDebuggerThreadGroup *thread_group)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));

  g_signal_emit (self, signals [THREAD_GROUP_REMOVED], 0, thread_group);
}

/**
 * ide_debugger_thread_group_started:
 * @self: an #IdeDebugger
 * @thread_group: an #IdeDebuggerThreadGroup
 *
 * Debugger implementations should call this to notify that a thread group has
 * started executing.
 */
void
ide_debugger_emit_thread_group_started (IdeDebugger            *self,
                                        IdeDebuggerThreadGroup *thread_group)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));

  g_signal_emit (self, signals [THREAD_GROUP_STARTED], 0, thread_group);
}

/**
 * ide_debugger_thread_group_exited:
 * @self: an #IdeDebugger
 * @thread_group: an #IdeDebuggerThreadGroup
 *
 * Debugger implementations should call this to notify that a thread group has
 * exited.
 */
void
ide_debugger_emit_thread_group_exited (IdeDebugger            *self,
                                       IdeDebuggerThreadGroup *thread_group)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));

  g_signal_emit (self, signals [THREAD_GROUP_EXITED], 0, thread_group);
}

/**
 * ide_debugger_emit_thread_added:
 * @self: an #IdeDebugger
 * @thread: an #IdeDebuggerThread
 *
 * Emits the #IdeDebugger::thread-added signal notifying that a new thread
 * has been added to the inferior.
 */
void
ide_debugger_emit_thread_added (IdeDebugger       *self,
                                IdeDebuggerThread *thread)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD (thread));

  g_signal_emit (self, signals [THREAD_ADDED], 0, thread);
}

/**
 * ide_debugger_emit_thread_removed:
 * @self: an #IdeDebugger
 * @thread: an #IdeDebuggerThread
 *
 * Emits the #IdeDebugger::thread-removed signal notifying that a thread has
 * been removed to the inferior.
 */
void
ide_debugger_emit_thread_removed (IdeDebugger       *self,
                                  IdeDebuggerThread *thread)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD (thread));

  g_signal_emit (self, signals [THREAD_REMOVED], 0, thread);
}

/**
 * ide_debugger_emit_thread_selected:
 * @self: an #IdeDebugger
 * @thread: an #IdeDebuggerThread
 *
 * Emits the #IdeDebugger::thread-selected signal notifying that a thread
 * has been set as the current debugging thread.
 */
void
ide_debugger_emit_thread_selected (IdeDebugger       *self,
                                   IdeDebuggerThread *thread)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD (thread));

  g_signal_emit (self, signals [THREAD_SELECTED], 0, thread);
}

/**
 * ide_debugger_emit_breakpoint_added:
 * @self: an #IdeDebugger
 * @breakpoint: an #IdeDebuggerBreakpoint
 *
 * Emits the #IdeDebugger::breakpoint-added signal.
 *
 * Debugger implementations should call this when a new breakpoint
 * has been registered with the debugger.
 *
 * If a breakpoint has changed, you should use
 * ide_debugger_emit_breakpoint_modified() to notify of the modification.
 */
void
ide_debugger_emit_breakpoint_added (IdeDebugger           *self,
                                    IdeDebuggerBreakpoint *breakpoint)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  g_signal_emit (self, signals [BREAKPOINT_ADDED], 0, breakpoint);
}

/**
 * ide_debugger_emit_breakpoint_removed:
 * @self: an #IdeDebugger
 * @breakpoint: an #IdeDebuggerBreakpoint
 *
 * Emits the #IdeDebugger::breakpoint-removed signal.
 *
 * Debugger implementations should call this when a breakpoint has been removed
 * either manually or automatically by the debugger.
 *
 * If a breakpoint has changed, you should use
 * ide_debugger_emit_breakpoint_modified() to notify of the modification.
 */
void
ide_debugger_emit_breakpoint_removed (IdeDebugger           *self,
                                      IdeDebuggerBreakpoint *breakpoint)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  g_signal_emit (self, signals [BREAKPOINT_REMOVED], 0, breakpoint);
}

/**
 * ide_debugger_emit_breakpoint_modified:
 * @self: an #IdeDebugger
 * @breakpoint: an #IdeDebuggerBreakpoint
 *
 * Emits the #IdeDebugger::breakpoint-modified signal.
 *
 * Debugger implementations should call this when a breakpoint has changed
 * in the underlying debugger.
 */
void
ide_debugger_emit_breakpoint_modified (IdeDebugger           *self,
                                       IdeDebuggerBreakpoint *breakpoint)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  g_signal_emit (self, signals [BREAKPOINT_MODIFIED], 0, breakpoint);
}

/**
 * ide_debugger_emit_running:
 * @self: an #IdeDebugger
 *
 * Emits the "running" signal.
 *
 * Debugger implementations should call this when the debugger has started
 * or restarted executing the inferior.
 */
void
ide_debugger_emit_running (IdeDebugger *self)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));

  g_signal_emit (self, signals [RUNNING], 0);
}

/**
 * ide_debugger_emit_stopped:
 * @self: an #IdeDebugger
 * @stop_reason: the reason the debugger stopped
 * @breakpoint: the breakpoint representing the stop location
 *
 * Emits the "stopped" signal.
 *
 * Debugger implementations should call this when the debugger has stopped
 * and include the reason and location of the stop.
 */
void
ide_debugger_emit_stopped (IdeDebugger           *self,
                           IdeDebuggerStopReason  stop_reason,
                           IdeDebuggerBreakpoint *breakpoint)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_STOP_REASON (stop_reason));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  g_signal_emit (self, signals [STOPPED], 0, stop_reason, breakpoint);
}

/**
 * ide_debugger_emit_library_loaded:
 * @self: an #IdeDebugger
 * @library: an #IdeDebuggerLibrary
 *
 * Emits the "library-loaded" signal.
 *
 * Debugger implementations should call this when the debugger has loaded
 * a new library.
 */
void
ide_debugger_emit_library_loaded (IdeDebugger        *self,
                                  IdeDebuggerLibrary *library)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_LIBRARY (library));

  g_signal_emit (self, signals [LIBRARY_LOADED], 0, library);
}

/**
 * ide_debugger_emit_library_unloaded:
 * @self: an #IdeDebugger
 * @library: an #IdeDebuggerLibrary
 *
 * Emits the "library-unloaded" signal.
 *
 * Debugger implementations should call this when the debugger has unloaded a
 * library.
 */
void
ide_debugger_emit_library_unloaded (IdeDebugger        *self,
                                    IdeDebuggerLibrary *library)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_LIBRARY (library));

  g_signal_emit (self, signals [LIBRARY_UNLOADED], 0, library);
}

/**
 * ide_debugger_list_breakpoints_async:
 * @self: An #IdeDebugger
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a callback to call upon completion
 * @user_data: user data for @callback
 *
 * Asynchronously requests the list of current breakpoints from the debugger.
 *
 * #IdeDebugger implementations must implement the virtual function
 * for this method.
 */
void
ide_debugger_list_breakpoints_async (IdeDebugger         *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->list_breakpoints_async (self, cancellable, callback, user_data);
}

/**
 * ide_debugger_list_breakpoints_finish:
 * @self: An #IdeDebugger
 * @result: a #GAsyncResult provided to the async callback
 * @error: a location for a #GError or %NULL
 *
 * Gets the list of breakpoints from the debugger.
 *
 * Returns: (transfer full) (element-type Ide.DebuggerBreakpoint): a #GPtrArray
 *   of breakpoints that are registered with the debugger.
 */
GPtrArray *
ide_debugger_list_breakpoints_finish (IdeDebugger   *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_DEBUGGER_GET_CLASS (self)->list_breakpoints_finish (self, result, error);
}

/**
 * ide_debugger_insert_breakpoint_async:
 * @self: An #IdeDebugger
 * @breakpoint: An #IdeDebuggerBreakpoint
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: an async callback to complete the operation
 * @user_data: user data for @callback
 *
 * Asynchronously requests that a breakpoint is added to the debugger.
 *
 * This asynchronous function may complete before the breakpoint has been
 * registered in the debugger. Debugger implementations will emit
 * #IdeDebugger::breakpoint-added when a breakpoint has been registered.
 */
void
ide_debugger_insert_breakpoint_async (IdeDebugger             *self,
                                      IdeDebuggerBreakpoint   *breakpoint,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->insert_breakpoint_async (self,
                                                          breakpoint,
                                                          cancellable,
                                                          callback,
                                                          user_data);
}

/**
 * ide_debugger_insert_breakpoint_finish:
 * @self: An #IdeDebugger
 * @result: a #GAsyncResult or %NULL
 * @error: a #GError, or %NULL
 *
 * Completes a request to asynchronously insert a breakpoint.
 *
 * See also: ide_debugger_insert_breakpoint_async()
 *
 * Returns: %TRUE if the command was submitted successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
ide_debugger_insert_breakpoint_finish (IdeDebugger   *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEBUGGER_GET_CLASS (self)->insert_breakpoint_finish (self, result, error);
}

/**
 * ide_debugger_remove_breakpoint_async:
 * @self: An #IdeDebugger
 * @breakpoint: An #IdeDebuggerBreakpoint
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: an async callback to complete the operation
 * @user_data: user data for @callback
 *
 * Asynchronously requests that a breakpoint is removed from the debugger.
 *
 * This asynchronous function may complete before the breakpoint has been
 * removed by the debugger. Debugger implementations will emit
 * #IdeDebugger::breakpoint-removed when a breakpoint has been removed.
 */
void
ide_debugger_remove_breakpoint_async (IdeDebugger             *self,
                                      IdeDebuggerBreakpoint   *breakpoint,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->remove_breakpoint_async (self,
                                                          breakpoint,
                                                          cancellable,
                                                          callback,
                                                          user_data);
}

/**
 * ide_debugger_remove_breakpoint_finish:
 * @self: An #IdeDebugger
 * @result: a #GAsyncResult or %NULL
 * @error: a #GError, or %NULL
 *
 * Completes a request to asynchronously remove a breakpoint.
 *
 * See also: ide_debugger_remove_breakpoint_async()
 *
 * Returns: %TRUE if the command was submitted successfully; otherwise %FALSE and @error is set.
 */
gboolean
ide_debugger_remove_breakpoint_finish (IdeDebugger   *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEBUGGER_GET_CLASS (self)->remove_breakpoint_finish (self, result, error);
}

/**
 * ide_debugger_modify_breakpoint_async:
 * @self: An #IdeDebugger
 * @change: An #IdeDebuggerBreakpointChange
 * @breakpoint: An #IdeDebuggerBreakpoint
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: an async callback to complete the operation
 * @user_data: user data for @callback
 *
 * Asynchronously requests that a breakpoint is modified by the debugger backend.
 *
 * Specify @change for how to modify the breakpoint.
 *
 * This asynchronous function may complete before the breakpoint has been
 * modified by the debugger. Debugger implementations will emit
 * #IdeDebugger::breakpoint-modified when a breakpoint has been removed.
 */
void
ide_debugger_modify_breakpoint_async (IdeDebugger                 *self,
                                      IdeDebuggerBreakpointChange  change,
                                      IdeDebuggerBreakpoint       *breakpoint,
                                      GCancellable                *cancellable,
                                      GAsyncReadyCallback          callback,
                                      gpointer                     user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT_CHANGE (change));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->modify_breakpoint_async (self,
                                                          change,
                                                          breakpoint,
                                                          cancellable,
                                                          callback,
                                                          user_data);
}

/**
 * ide_debugger_modify_breakpoint_finish:
 * @self: a #IdeDebugger
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to modify a breakpoint.
 *
 * Note that this only completes the submission of the request, if you need to
 * know when the breakpoint has been modified, listen to the
 * #IdeDebugger::breakpoint-modified signal.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_debugger_modify_breakpoint_finish (IdeDebugger   *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEBUGGER_GET_CLASS (self)->modify_breakpoint_finish (self, result, error);
}

/**
 * ide_debugger_get_breakpoints:
 * @self: An #IdeDebugger
 *
 * Gets the breakpoints for the #IdeDebugger.
 *
 * Contrast this to ide_debugger_list_breakpoints_async() which will query
 * the debugger backend for breakpoints. This #GListModel containing
 * #IdeDebuggerBreakpoint instances is updated as necessary by listening
 * to various breakpoint related signals on the #IdeDebugger instance.
 *
 * This is primarily out of convenience to be used by UI which wants to
 * display information on breakpoints.
 *
 * Returns: (transfer none) (not nullable): a #GListModel of #IdeDebuggerBreakpoint
 */
GListModel *
ide_debugger_get_breakpoints (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);

  return G_LIST_MODEL (priv->breakpoints);
}

/**
 * ide_debugger_get_thread_groups:
 * @self: a #IdeDebugger
 *
 * Gets the thread groups that have been registered by the debugger.
 *
 * The resulting #GListModel accuracy is based on the #IdeDebugger
 * implementation emitting varous thread-group modification signals correctly.
 *
 * Returns: (transfer none) (not nullable): a #GListModel of #IdeDebuggerThreadGroup
 */
GListModel *
ide_debugger_get_thread_groups (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);

  return G_LIST_MODEL (priv->thread_groups);
}

/**
 * ide_debugger_get_threads:
 * @self: a #IdeDebugger
 *
 * Gets the threads that have been registered by the debugger.
 *
 * The resulting #GListModel accuracy is based on the #IdeDebugger
 * implementation emitting varous thread modification signals correctly.
 *
 * Returns: (transfer none) (not nullable): a #GListModel of #IdeDebuggerThread
 */
GListModel *
ide_debugger_get_threads (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);

  return G_LIST_MODEL (priv->threads);
}

void
ide_debugger_list_frames_async (IdeDebugger         *self,
                                IdeDebuggerThread   *thread,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD (thread));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->list_frames_async (self, thread, cancellable, callback, user_data);
}

/**
 * ide_debugger_list_frames_finish:
 *
 *
 *
 * Returns: (transfer full) (element-type Ide.DebuggerFrame) (nullable): An
 *   array of debugger frames or %NULL and @error is set.
 */
GPtrArray *
ide_debugger_list_frames_finish (IdeDebugger   *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_DEBUGGER_GET_CLASS (self)->list_frames_finish (self, result, error);
}

/**
 * ide_debugger_get_selected_thread:
 * @self: An #IdeDebugger
 *
 * Gets the current selected thread by the debugger.
 *
 * Returns: (transfer none) (nullable): An #IdeDebuggerThread or %NULL
 */
IdeDebuggerThread *
ide_debugger_get_selected_thread (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);

  return priv->selected;
}

/**
 * ide_debugger_interrupt_async:
 * @self: a #IdeDebugger
 * @thread_group: (nullable): An #IdeDebuggerThreadGroup
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (closure user_data): a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the debugger interrupts execution of a thread
 * group. Thread groups are a collection of threads that are executed or
 * stopped together and on gdb on Linux, this is the default for all threads in
 * the process.
 */
void
ide_debugger_interrupt_async (IdeDebugger            *self,
                              IdeDebuggerThreadGroup *thread_group,
                              GCancellable           *cancellable,
                              GAsyncReadyCallback     callback,
                              gpointer                user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (!thread_group || IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->interrupt_async (self, thread_group, cancellable, callback, user_data);
}

gboolean
ide_debugger_interrupt_finish (IdeDebugger   *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEBUGGER_GET_CLASS (self)->interrupt_finish (self, result, error);
}

gboolean
ide_debugger_get_is_running (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);

  return priv->is_running;
}

gboolean
_ide_debugger_get_has_started (IdeDebugger *self)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);

  return priv->has_started;
}

void
ide_debugger_send_signal_async (IdeDebugger         *self,
                                gint                 signum,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->send_signal_async (self, signum, cancellable, callback, user_data);
}

gboolean
ide_debugger_send_signal_finish (IdeDebugger   *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEBUGGER_GET_CLASS (self)->send_signal_finish (self, result, error);
}

/**
 * ide_debugger_locate_binary_at_address:
 * @self: a #IdeDebugger
 * @address: the address within the inferior process space
 *
 * Attempts to locate the binary that contains a given address.
 *
 * @address should be an address within the inferiors process space.
 *
 * This works by keeping track of libraries as they are loaded and unloaded and
 * their associated file mappings.
 *
 * Currently, the filename will match the name in the inferior mount namespace,
 * but that may change based on future design changes.
 *
 * Returns: the filename of the binary or %NULL
 */
const gchar *
ide_debugger_locate_binary_at_address (IdeDebugger        *self,
                                       IdeDebuggerAddress  address)
{
  IdeDebuggerPrivate *priv = ide_debugger_get_instance_private (self);
  const IdeDebuggerAddressMapEntry *entry;

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);

  entry = ide_debugger_address_map_lookup (priv->map, address);

  if (entry != NULL)
    return entry->filename;

  return NULL;
}

/**
 * ide_debugger_list_locals_async:
 * @self: an #IdeDebugger
 * @thread: an #IdeDebuggerThread
 * @frame: an #IdeDebuggerFrame
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: A callback to call once the operation has finished
 * @user_data: user data for @callback
 *
 * Requests the debugger backend to list the locals that are available to the
 * given @frame of @thread.
 */
void
ide_debugger_list_locals_async (IdeDebugger         *self,
                                IdeDebuggerThread   *thread,
                                IdeDebuggerFrame    *frame,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD (thread));
  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (frame));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->list_locals_async (self,
                                                    thread,
                                                    frame,
                                                    cancellable,
                                                    callback,
                                                    user_data);
}

/**
 * ide_debugger_list_locals_finish:
 * @self: a #IdeDebugger
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to ide_debugger_list_locals_async().
 *
 * Returns: (transfer full) (element-type Ide.DebuggerVariable): a #GPtrArray of
 *   #IdeDebuggerVariable if successful; otherwise %NULL and error is set.
 */
GPtrArray *
ide_debugger_list_locals_finish (IdeDebugger   *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_DEBUGGER_GET_CLASS (self)->list_locals_finish (self, result, error);
}

/**
 * ide_debugger_list_params_async:
 * @self: an #IdeDebugger
 * @thread: an #IdeDebuggerThread
 * @frame: an #IdeDebuggerFrame
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: A callback to call once the operation has finished
 * @user_data: user data for @callback
 *
 * Requests the debugger backend to list the parameters to the given stack
 * frame.
 */
void
ide_debugger_list_params_async (IdeDebugger         *self,
                                IdeDebuggerThread   *thread,
                                IdeDebuggerFrame    *frame,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_THREAD (thread));
  g_return_if_fail (IDE_IS_DEBUGGER_FRAME (frame));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->list_params_async (self,
                                                    thread,
                                                    frame,
                                                    cancellable,
                                                    callback,
                                                    user_data);
}

/**
 * ide_debugger_list_params_finish:
 * @self: a #IdeDebugger
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to ide_debugger_list_params_async().
 *
 * Returns: (transfer full) (element-type Ide.DebuggerVariable): a #GPtrArray of
 *   #IdeDebuggerVariable if successful; otherwise %NULL and error is set.
 */
GPtrArray *
ide_debugger_list_params_finish (IdeDebugger   *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_DEBUGGER_GET_CLASS (self)->list_params_finish (self, result, error);
}

/**
 * ide_debugger_list_registers_async:
 * @self: an #IdeDebugger
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: A callback to call once the operation has finished
 * @user_data: user data for @callback
 *
 * Requests the list of registers and their values.
 */
void
ide_debugger_list_registers_async (IdeDebugger         *self,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->list_registers_async (self, cancellable, callback, user_data);
}

/**
 * ide_debugger_list_registers_finish:
 * @self: a #IdeDebugger
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to ide_debugger_list_registers_async().
 *
 * Returns: (transfer full) (element-type Ide.DebuggerRegister): a #GPtrArray of
 *   #IdeDebuggerRegister if successful; otherwise %NULL and error is set.
 */
GPtrArray *
ide_debugger_list_registers_finish (IdeDebugger   *self,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_DEBUGGER_GET_CLASS (self)->list_registers_finish (self, result, error);
}

/**
 * ide_debugger_disassemble_async:
 * @self: an #IdeDebugger
 * @range: an #IdeDebuggerAddressRange to disassemble
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: A callback to call once the operation has finished
 * @user_data: user data for @callback
 *
 * Disassembles the address range requested.
 */
void
ide_debugger_disassemble_async (IdeDebugger                   *self,
                                const IdeDebuggerAddressRange *range,
                                GCancellable                  *cancellable,
                                GAsyncReadyCallback            callback,
                                gpointer                       user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (range != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_CLASS (self)->disassemble_async (self, range, cancellable, callback, user_data);
}

/**
 * ide_debugger_disassemble_finish:
 * @self: a #IdeDebugger
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous request to ide_debugger_disassemble_async().
 *
 * Returns: (transfer full) (element-type Ide.DebuggerInstruction): a #GPtrArray
 *   of #IdeDebuggerInstruction if successful; otherwise %NULL and error is set.
 */
GPtrArray *
ide_debugger_disassemble_finish (IdeDebugger   *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_DEBUGGER_GET_CLASS (self)->disassemble_finish (self, result, error);
}

/**
 * ide_debugger_supports_runner:
 * @self: an #IdeDebugger
 * @pipeline: an #IdePipeline
 * #run_command: an #IdeRunCommand
 * @priority: (out): A location for a priority
 *
 * Checks if the debugger supports a given runner. The debugger may need
 * to check if the binary type matches it's expectation.
 *
 * Returns: %TRUE if the #IdeDebugger supports the runner.
 */
gboolean
ide_debugger_supports_run_command (IdeDebugger   *self,
                                   IdePipeline   *pipeline,
                                   IdeRunCommand *run_command,
                                   int           *priority)
{
  int dummy = 0;

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_PIPELINE (pipeline), FALSE);
  g_return_val_if_fail (IDE_IS_RUN_COMMAND (run_command), FALSE);

  if (priority == NULL)
    priority = &dummy;
  else
    *priority = 0;

  return IDE_DEBUGGER_GET_CLASS (self)->supports_run_command (self, pipeline, run_command, priority);
}

/**
 * ide_debugger_prepare_for_run:
 * @self: an #IdeDebugger
 * @pipeline: an #IdePipeline
 * @run_context: an #IdeRunContext
 *
 * Prepares the runner to launch a debugger and target process.
 */
void
ide_debugger_prepare_for_run (IdeDebugger   *self,
                              IdePipeline   *pipeline,
                              IdeRunContext *run_context)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (IDE_IS_RUN_CONTEXT (run_context));
  g_return_if_fail (IDE_DEBUGGER_GET_CLASS (self)->prepare_for_run != NULL);

  IDE_DEBUGGER_GET_CLASS (self)->prepare_for_run (self, pipeline, run_context);
}

/**
 * ide_debugger_interpret_async:
 * @self: an #IdeDebugger
 * @command: a command to execute
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute, or %NULL
 * @user_data: user data for @callback
 *
 * Asynchronously requests that the debugger interpret the command.
 *
 * This is used by the interactive-console to submit commands to the debugger
 * that are in the native syntax of that debugger.
 *
 * The debugger is expected to return any textual output via the
 * IdeDebugger::log signal.
 *
 * Call ide_debugger_interpret_finish() from @callback to determine if the
 * command was interpreted.
 */
void
ide_debugger_interpret_async (IdeDebugger         *self,
                              const gchar         *command,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (command != NULL);

  return IDE_DEBUGGER_GET_CLASS (self)->interpret_async (self, command, cancellable, callback, user_data);
}

/**
 * ide_debugger_interpret_finish:
 * @self: an #IdeDebugger
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Retrieves the result of the asynchronous operation to interpret a debugger
 * command.
 *
 * Returns: %TRUE if the command was interpreted, otherwise %FALSE and
 *    @error is set.
 */
gboolean
ide_debugger_interpret_finish (IdeDebugger   *self,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_DEBUGGER_GET_CLASS (self)->interpret_finish (self, result, error);
}
