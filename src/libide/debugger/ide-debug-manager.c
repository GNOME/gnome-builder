/* ide-debug-manager.c
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

#define G_LOG_DOMAIN "ide-debug-manager"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "buildsystem/ide-build-target.h"
#include "debugger/ide-debug-manager.h"
#include "debugger/ide-debugger.h"
#include "debugger/ide-debugger-private.h"
#include "files/ide-file.h"
#include "plugins/ide-extension-util.h"
#include "runner/ide-runner.h"

#define TAG_CURRENT_BKPT "debugger::current-breakpoint"

struct _IdeDebugManager
{
  IdeObject           parent_instance;

  GHashTable         *breakpoints;
  IdeDebugger        *debugger;
  DzlSignalGroup     *debugger_signals;
  IdeRunner          *runner;
  GQueue              pending_breakpoints;
  GPtrArray          *supported_languages;

  guint               active : 1;
};

typedef struct
{
  IdeDebugger *debugger;
  IdeRunner   *runner;
  gint         priority;
} DebuggerLookup;

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_DEBUGGER,
  N_PROPS
};

enum {
  BREAKPOINT_ADDED,
  BREAKPOINT_REMOVED,
  BREAKPOINT_REACHED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

G_DEFINE_TYPE (IdeDebugManager, ide_debug_manager, IDE_TYPE_OBJECT)

static gint
compare_language_id (gconstpointer a,
                     gconstpointer b)
{
  const gchar * const *astr = a;
  const gchar * const *bstr = b;

  return strcmp (*astr, *bstr);
}

/**
 * ide_debug_manager_supports_language:
 * @self: a #IdeDebugManager
 * @language_id: (nullable): #GtkSourceView based language identifier or %NULL
 *
 * This checks to see if there is a debugger that can possibly support a given
 * language id. This is used to determine if space for breakpoints should be
 * reserved in the gutter of source code editor.
 *
 * This function accepts %NULL for @language_id out of convenience and will
 * return %NULL in this case.
 *
 * Returns: %TRUE if the language is supported; otherwise %FALSE.
 *
 * Since: 3.26
 */
gboolean
ide_debug_manager_supports_language (IdeDebugManager *self,
                                     const gchar     *language_id)
{
  const gchar *ret;

  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), FALSE);
  g_return_val_if_fail (self->supported_languages != NULL, FALSE);

  if (language_id == NULL)
    return FALSE;

  ret = bsearch (&language_id,
                 (gpointer)self->supported_languages->pdata,
                 self->supported_languages->len,
                 sizeof (gchar *),
                 compare_language_id);

  return ret != NULL;
}

static void
ide_debug_manager_plugin_loaded (IdeDebugManager *self,
                                 PeasPluginInfo  *plugin_info,
                                 PeasEngine      *engine)
{
  const gchar *supported;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (self->supported_languages != NULL);

  supported = peas_plugin_info_get_external_data (plugin_info, "Debugger-Languages");

  if (supported != NULL)
    {
      gchar **languages = g_strsplit (supported, ",", 0);

      for (guint i = 0; languages[i] != NULL; i++)
        g_ptr_array_add (self->supported_languages, g_steal_pointer (&languages[i]));
      g_ptr_array_sort (self->supported_languages, compare_language_id);
      g_free (languages);
    }
}

static void
ide_debug_manager_remove_supported_language (IdeDebugManager *self,
                                             const gchar     *language_id)
{
  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (language_id != NULL);
  g_assert (self->supported_languages != NULL);

  for (guint i = 0; i < self->supported_languages->len; i++)
    {
      const gchar *ele = g_ptr_array_index (self->supported_languages, i);

      if (g_strcmp0 (ele, language_id) == 0)
        {
          g_ptr_array_remove_index (self->supported_languages, i);
          break;
        }
    }
}

static void
ide_debug_manager_plugin_unloaded (IdeDebugManager *self,
                                   PeasPluginInfo  *plugin_info,
                                   PeasEngine      *engine)
{
  const gchar *supported;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (plugin_info != NULL);
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (self->supported_languages != NULL);

  supported = peas_plugin_info_get_external_data (plugin_info, "Debugger-Languages");

  if (supported != NULL)
    {
      g_auto(GStrv) languages = g_strsplit (supported, ",", 0);

      for (guint i = 0; languages[i] != NULL; i++)
        ide_debug_manager_remove_supported_language (self, languages[i]);
    }
}

static void
ide_debug_manager_load_supported_languages (IdeDebugManager *self)
{
  g_auto(GStrv) loaded_plugins = NULL;
  PeasEngine *engine;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (self->supported_languages != NULL);

  engine = peas_engine_get_default ();

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (ide_debug_manager_plugin_loaded),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (ide_debug_manager_plugin_unloaded),
                           self,
                           G_CONNECT_SWAPPED);

  loaded_plugins = peas_engine_get_loaded_plugins (engine);

  for (guint i = 0; loaded_plugins[i] != NULL; i++)
    {
      const gchar *module_name = loaded_plugins[i];
      PeasPluginInfo *plugin_info = peas_engine_get_plugin_info (engine, module_name);

      ide_debug_manager_plugin_loaded (self, plugin_info, engine);
    }
}

static void
ide_debug_manager_set_active (IdeDebugManager *self,
                              gboolean         active)
{
  g_assert (IDE_IS_DEBUG_MANAGER (self));

  active = !!active;

  if (active != self->active)
    {
      self->active = active;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE]);
    }
}

static void
ide_debug_manager_debugger_stopped (IdeDebugManager       *self,
                                    IdeDebuggerStopReason  stop_reason,
                                    IdeDebuggerBreakpoint *breakpoint,
                                    IdeDebugger           *debugger)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_DEBUGGER_STOP_REASON (stop_reason));
  g_assert (!breakpoint || IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  switch (stop_reason)
    {
    case IDE_DEBUGGER_STOP_EXITED:
    case IDE_DEBUGGER_STOP_EXITED_NORMALLY:
    case IDE_DEBUGGER_STOP_EXITED_SIGNALED:
      /* Cleanup any lingering debugger process */
      if (self->runner != NULL)
        ide_runner_force_quit (self->runner);
      break;

    case IDE_DEBUGGER_STOP_BREAKPOINT_HIT:
    case IDE_DEBUGGER_STOP_FUNCTION_FINISHED:
    case IDE_DEBUGGER_STOP_LOCATION_REACHED:
    case IDE_DEBUGGER_STOP_SIGNAL_RECEIVED:
    case IDE_DEBUGGER_STOP_CATCH:
    case IDE_DEBUGGER_STOP_UNKNOWN:
      if (breakpoint != NULL)
        {
          IDE_TRACE_MSG ("Emitting breakpoint-reached");
          g_signal_emit (self, signals [BREAKPOINT_REACHED], 0, breakpoint);
        }
      break;

    default:
      g_assert_not_reached ();
    }

  IDE_EXIT;
}

static void
ide_debug_manager_breakpoint_added (IdeDebugManager       *self,
                                    IdeDebuggerBreakpoint *breakpoint,
                                    IdeDebugger           *debugger)
{
  IdeDebuggerBreakpoints *breakpoints;
  g_autoptr(GFile) file = NULL;
  const gchar *path;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  /* If there is no file, then there is nothing to cache */
  path = ide_debugger_breakpoint_get_file (breakpoint);
  if (path == NULL)
    return;

  file = g_file_new_for_path (path);
  breakpoints = g_hash_table_lookup (self->breakpoints, file);
  if (breakpoints == NULL)
    {
      breakpoints = g_object_new (IDE_TYPE_DEBUGGER_BREAKPOINTS,
                                  "file", file,
                                  NULL);
      g_hash_table_insert (self->breakpoints, g_steal_pointer (&file), breakpoints);
    }

  _ide_debugger_breakpoints_add (breakpoints, breakpoint);
}

static void
ide_debug_manager_breakpoint_removed (IdeDebugManager       *self,
                                      IdeDebuggerBreakpoint *breakpoint,
                                      IdeDebugger           *debugger)
{
  IdeDebuggerBreakpoints *breakpoints;
  g_autoptr(GFile) file = NULL;
  const gchar *path;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  path = ide_debugger_breakpoint_get_file (breakpoint);
  if (path == NULL)
    return;

  file = g_file_new_for_path (path);
  breakpoints = g_hash_table_lookup (self->breakpoints, file);
  if (breakpoints == NULL)
    return;

  _ide_debugger_breakpoints_remove (breakpoints, breakpoint);
}

static void
ide_debug_manager_breakpoint_modified (IdeDebugManager       *self,
                                       IdeDebuggerBreakpoint *breakpoint,
                                       IdeDebugger           *debugger)
{
  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  ide_debug_manager_breakpoint_removed (self, breakpoint, debugger);
  ide_debug_manager_breakpoint_added (self, breakpoint, debugger);
}

static void
ide_debug_manager_unmark_stopped (IdeDebugManager *self,
                                  IdeBuffer       *buffer)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_buffer_remove_tag_by_name (GTK_TEXT_BUFFER (buffer),
                                      TAG_CURRENT_BKPT,
                                      &begin, &end);
}

static void
ide_debug_manager_clear_stopped (IdeDebugManager *self)
{
  IdeBufferManager *bufmgr;
  IdeContext *context;
  guint n_items;

  g_assert (IDE_IS_DEBUG_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  bufmgr = ide_context_get_buffer_manager (context);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (bufmgr));

  /*
   * This might be bordering on "too much work to do at once" if there is a
   * sufficient number of buffers open. I'm not sure how much btree scanning is
   * required to clear the tags.
   *
   * Alternatively, we could store the buffers we've touched and then just clear
   * them, which would be strictly better (but annoying from object life-cycle
   * standpoint).
   */

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = NULL;

      buffer = g_list_model_get_item (G_LIST_MODEL (bufmgr), i);
      g_assert (IDE_IS_BUFFER (buffer));

      ide_debug_manager_unmark_stopped (self, buffer);
    }
}

static void
ide_debug_manager_debugger_running (IdeDebugManager *self,
                                    IdeDebugger     *debugger)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  ide_debug_manager_clear_stopped (self);

  IDE_EXIT;
}

static void
ide_debug_manager_mark_stopped (IdeDebugManager       *self,
                                IdeBuffer             *buffer,
                                IdeDebuggerBreakpoint *breakpoint)
{
  GtkTextIter iter;
  GtkTextIter end;
  guint line;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  line = ide_debugger_breakpoint_get_line (breakpoint);

  if (line > 0)
    line--;

  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &iter, line);
  end = iter;
  gtk_text_iter_forward_line (&iter);

  gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (buffer),
                                     TAG_CURRENT_BKPT,
                                     &iter, &end);
}

static void
ide_debug_manager_load_file_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;
  IdeDebuggerBreakpoint *breakpoint;
  IdeDebugManager *self;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error);

  if (buffer == NULL)
    {
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_DEBUG_MANAGER (self));

  breakpoint = g_task_get_task_data (task);
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  ide_debug_manager_mark_stopped (self, buffer, breakpoint);

  IDE_EXIT;
}

static void
ide_debug_manager_real_breakpoint_reached (IdeDebugManager       *self,
                                           IdeDebuggerBreakpoint *breakpoint)
{
  const gchar *path;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  path = ide_debugger_breakpoint_get_file (breakpoint);

  if (path != NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeBufferManager *bufmgr = ide_context_get_buffer_manager (context);
      g_autoptr(IdeFile) file = ide_file_new_for_path (context, path);
      g_autoptr(GTask) task = NULL;

      task = g_task_new (self, NULL, NULL, NULL);
      g_task_set_task_data (task, g_object_ref (breakpoint), g_object_unref);

      ide_buffer_manager_load_file_async (bufmgr,
                                          file,
                                          FALSE,
                                          IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                          NULL,
                                          NULL,
                                          ide_debug_manager_load_file_cb,
                                          g_steal_pointer (&task));
    }
}

static void
ide_debug_manager_dispose (GObject *object)
{
  IdeDebugManager *self = (IdeDebugManager *)object;

  g_queue_foreach (&self->pending_breakpoints, (GFunc)g_object_unref, NULL);
  g_queue_clear (&self->pending_breakpoints);

  g_hash_table_remove_all (self->breakpoints);
  dzl_signal_group_set_target (self->debugger_signals, NULL);
  g_clear_object (&self->debugger);
  g_clear_object (&self->runner);

  G_OBJECT_CLASS (ide_debug_manager_parent_class)->dispose (object);
}

static void
ide_debug_manager_finalize (GObject *object)
{
  IdeDebugManager *self = (IdeDebugManager *)object;

  g_clear_object (&self->debugger_signals);
  g_clear_pointer (&self->breakpoints, g_hash_table_unref);

  G_OBJECT_CLASS (ide_debug_manager_parent_class)->finalize (object);
}

static void
ide_debug_manager_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeDebugManager *self = IDE_DEBUG_MANAGER (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, self->active);
      break;

    case PROP_DEBUGGER:
      g_value_set_object (value, self->debugger);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debug_manager_class_init (IdeDebugManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_debug_manager_dispose;
  object_class->finalize = ide_debug_manager_finalize;
  object_class->get_property = ide_debug_manager_get_property;

  /**
   * IdeDebugManager:active:
   *
   * If the debugger is active.
   *
   * This can be used to determine if the controls should be made visible
   * in the workbench.
   */
  properties [PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          "Active",
                          "If the debugger is running",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "The current debugger being used",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeDebugManager::breakpoint-added:
   * @self: An #IdeDebugManager
   * @breakpoint: an #IdeDebuggerBreakpoint
   *
   * The "breakpoint-added" signal is emitted when a new breakpoint has
   * been registered by the debugger.
   *
   * Since: 3.26
   */
  signals [BREAKPOINT_ADDED] =
    g_signal_new_class_handler ("breakpoint-added",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL,
                                g_cclosure_marshal_VOID__OBJECT,
                                G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_BREAKPOINT);

  /**
   * IdeDebugManager::breakpoint-removed:
   * @self: An #IdeDebugManager
   * @breakpoint: an #IdeDebuggerBreakpoint
   *
   * The "breakpoint-removed" signal is emitted when a new breakpoint has been
   * removed by the debugger.
   *
   * Since: 3.26
   */
  signals [BREAKPOINT_REMOVED] =
    g_signal_new_class_handler ("breakpoint-removed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL,
                                g_cclosure_marshal_VOID__OBJECT,
                                G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_BREAKPOINT);

  /**
   * IdeDebugManager::breakpoint-reached:
   * @self: An #IdeDebugManager
   * @breakpoint: An #IdeDebuggerBreakpoint
   *
   * The "breakpoint-reached" signal is emitted when the debugger has reached
   * a breakpoint and execution has stopped.
   *
   * If you need the stop reason, you should connect to #IdeDebugger::stopped
   * on the #IdeDebugger itself.
   *
   * See also: #IdeDebugManager:debugger
   *
   * Since: 3.26
   */
  signals [BREAKPOINT_REACHED] =
    g_signal_new_class_handler ("breakpoint-reached",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_debug_manager_real_breakpoint_reached),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__OBJECT,
                                G_TYPE_NONE, 1, IDE_TYPE_DEBUGGER_BREAKPOINT);
}

static void
ide_debug_manager_init (IdeDebugManager *self)
{
  g_queue_init (&self->pending_breakpoints);

  self->supported_languages = g_ptr_array_new_with_free_func (g_free);

  ide_debug_manager_load_supported_languages (self);

  self->breakpoints = g_hash_table_new_full ((GHashFunc)g_file_hash,
                                             (GEqualFunc)g_file_equal,
                                             g_object_unref,
                                             g_object_unref);

  self->debugger_signals = dzl_signal_group_new (IDE_TYPE_DEBUGGER);

  dzl_signal_group_connect_swapped (self->debugger_signals,
                                    "stopped",
                                    G_CALLBACK (ide_debug_manager_debugger_stopped),
                                    self);

  dzl_signal_group_connect_swapped (self->debugger_signals,
                                    "running",
                                    G_CALLBACK (ide_debug_manager_debugger_running),
                                    self);

  dzl_signal_group_connect_swapped (self->debugger_signals,
                                    "breakpoint-added",
                                    G_CALLBACK (ide_debug_manager_breakpoint_added),
                                    self);

  dzl_signal_group_connect_swapped (self->debugger_signals,
                                    "breakpoint-modified",
                                    G_CALLBACK (ide_debug_manager_breakpoint_modified),
                                    self);

  dzl_signal_group_connect_swapped (self->debugger_signals,
                                    "breakpoint-removed",
                                    G_CALLBACK (ide_debug_manager_breakpoint_removed),
                                    self);
}

static gboolean
debugger_supports_language (PeasPluginInfo *plugin_info,
                            const gchar    *language)
{
  const gchar *supported;

  supported = peas_plugin_info_get_external_data (plugin_info, "Debugger-Languages");

  if (supported != NULL)
    {
      g_auto(GStrv) languages = g_strsplit (supported, ",", 0);
      for (guint i = 0; languages[i]; i++)
        {
          if (g_strcmp0 (languages[i], language) == 0)
            return TRUE;
        }
    }

  return FALSE;
}

static void
debugger_lookup (PeasExtensionSet *set,
                 PeasPluginInfo   *plugin_info,
                 PeasExtension    *exten,
                 gpointer          user_data)
{
  DebuggerLookup *lookup = user_data;
  IdeDebugger *debugger = (IdeDebugger *)exten;
  gint priority = G_MAXINT;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (lookup != NULL);

  if (ide_debugger_supports_runner (debugger, lookup->runner, &priority))
    {
      IdeBuildTarget *build_target = ide_runner_get_build_target (lookup->runner);

      if (build_target != NULL)
        {
          g_autofree gchar *language = ide_build_target_get_language (build_target);

          if (!debugger_supports_language (plugin_info, language))
            return;
        }

      if (lookup->debugger == NULL || priority < lookup->priority)
        {
          g_set_object (&lookup->debugger, debugger);
          lookup->priority = priority;
        }
    }
}

/**
 * ide_debug_manager_find_debugger:
 * @self: a #IdeDebugManager
 * @runner: An #IdeRunner
 *
 * Locates a debugger for the given runner, or %NULL if no debugger
 * supports the runner.
 *
 * Returns: (transfer full) (nullable): An #IdeDebugger or %NULL
 */
IdeDebugger *
ide_debug_manager_find_debugger (IdeDebugManager *self,
                                 IdeRunner       *runner)
{
  g_autoptr(PeasExtensionSet) set = NULL;
  IdeContext *context;
  DebuggerLookup lookup;

  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), NULL);
  g_return_val_if_fail (IDE_IS_RUNNER (runner), NULL);

  context = ide_object_get_context (IDE_OBJECT (runner));

  lookup.debugger = NULL;
  lookup.runner = runner;
  lookup.priority = G_MAXINT;

  set = ide_extension_set_new (peas_engine_get_default (),
                               IDE_TYPE_DEBUGGER,
                               "context", context,
                               NULL);

  peas_extension_set_foreach (set, debugger_lookup, &lookup);

  return lookup.debugger;
}

static void
ide_debug_manager_reset_breakpoints_cb (gpointer item,
                                        gpointer user_data)
{
  IdeDebuggerBreakpoint *breakpoint = item;

  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUG_MANAGER (user_data));

  _ide_debugger_breakpoint_reset (breakpoint);
}

static void
ide_debug_manager_reset_breakpoints (IdeDebugManager *self)
{
  IdeDebuggerBreakpoints *breakpoints;
  GHashTableIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUG_MANAGER (self));

  g_hash_table_iter_init (&iter, self->breakpoints);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&breakpoints))
    ide_debugger_breakpoints_foreach (breakpoints,
                                      ide_debug_manager_reset_breakpoints_cb,
                                      self);

  IDE_EXIT;
}

static void
ide_debug_manager_sync_breakpoints_cb (gpointer item,
                                       gpointer user_data)
{
  IdeDebugManager *self = user_data;
  IdeDebuggerBreakpoint *breakpoint = item;

  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (self->debugger != NULL);

  /*
   * If the breakpoint does not yet have an id, then we need to register
   * it with the debugger. We clear all id's when the debugger exits so
   * that we can replay them when starting the next session.
   */
  if (!ide_debugger_breakpoint_get_id (breakpoint))
    ide_debugger_insert_breakpoint_async (self->debugger, breakpoint, NULL, NULL, NULL);
}

static void
ide_debug_manager_sync_breakpoints (IdeDebugManager *self)
{
  GHashTableIter hiter;
  gpointer value;
  GList *pending;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (self->debugger != NULL);

  /* Register all our breakpoints known by file */

  g_hash_table_iter_init (&hiter, self->breakpoints);

  while (g_hash_table_iter_next (&hiter, NULL, &value))
    {
      IdeDebuggerBreakpoints *breakpoints = value;

      g_assert (IDE_IS_DEBUGGER_BREAKPOINTS (breakpoints));

      /*
       * TODO: We probably want to steal the breakpoint so that
       *       then "breakpoint-added" signal will populate the
       *       breakpoint with more information.
       */

      ide_debugger_breakpoints_foreach (breakpoints,
                                        ide_debug_manager_sync_breakpoints_cb,
                                        self);
    }

  /* Steal the pending breakpoints and register them */

  pending = self->pending_breakpoints.head;
  self->pending_breakpoints.head = NULL;
  self->pending_breakpoints.tail = NULL;
  self->pending_breakpoints.length = 0;

  for (const GList *iter = pending; iter != NULL; iter = iter->next)
    {
      g_autoptr(IdeDebuggerBreakpoint) breakpoint = iter->data;

      ide_debugger_insert_breakpoint_async (self->debugger, breakpoint, NULL, NULL, NULL);
    }

  g_list_free (pending);

  IDE_EXIT;
}

static void
ide_debug_manager_runner_spawned (IdeDebugManager *self,
                                  const gchar     *identifier,
                                  IdeRunner       *runner)
{
  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (identifier != NULL);
  g_assert (IDE_IS_RUNNER (runner));

  ide_debug_manager_sync_breakpoints (self);
}

static void
ide_debug_manager_runner_exited (IdeDebugManager *self,
                                 IdeRunner       *runner)
{
  g_autoptr(IdeDebugger) debugger = NULL;
  g_autoptr(IdeRunner) hold_runner = NULL;

  g_assert (IDE_IS_DEBUG_MANAGER (self));
  g_assert (IDE_IS_RUNNER (runner));

  /*
   * Keep debugger alive so that listeners to :debugger property can
   * properly disconnect signals when we clear the debugger instance.
   */
  debugger = g_steal_pointer (&self->debugger);
  hold_runner = g_steal_pointer (&self->runner);

  ide_debug_manager_set_active (self, FALSE);
  ide_debug_manager_reset_breakpoints (self);
  ide_debug_manager_clear_stopped (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);
}

/**
 * ide_debug_manager_start:
 * @self: an #IdeDebugManager
 * @runner: an #IdeRunner
 * @error: A location for an @error
 *
 * Attempts to start a runner using a discovered debugger backend.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
ide_debug_manager_start (IdeDebugManager  *self,
                         IdeRunner        *runner,
                         GError          **error)
{
  g_autoptr(IdeDebugger) debugger = NULL;
  gboolean ret = FALSE;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_RUNNER (runner), FALSE);

  debugger = ide_debug_manager_find_debugger (self, runner);

  if (debugger == NULL)
    {
      ide_runner_set_failed (runner, TRUE);
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   _("A suitable debugger could not be found."));
      IDE_GOTO (failure);
    }

  ide_debugger_prepare (debugger, runner);

  g_signal_connect_object (runner,
                           "spawned",
                           G_CALLBACK (ide_debug_manager_runner_spawned),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (runner,
                           "exited",
                           G_CALLBACK (ide_debug_manager_runner_exited),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  self->runner = g_object_ref (runner);
  self->debugger = g_steal_pointer (&debugger);

  dzl_signal_group_set_target (self->debugger_signals, self->debugger);

  ide_debug_manager_set_active (self, TRUE);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);

  ret = TRUE;

failure:
  IDE_RETURN (ret);
}

void
ide_debug_manager_stop (IdeDebugManager *self)
{
  g_return_if_fail (IDE_IS_DEBUG_MANAGER (self));

  dzl_signal_group_set_target (self->debugger_signals, NULL);

  if (self->runner != NULL)
    {
      ide_runner_force_quit (self->runner);
      g_clear_object (&self->runner);
    }

  g_clear_object (&self->debugger);
  ide_debug_manager_reset_breakpoints (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);
}

gboolean
ide_debug_manager_get_active (IdeDebugManager *self)
{
  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), FALSE);

  return self->active;
}

/**
 * ide_debug_manager_get_debugger:
 * @self: a #IdeDebugManager
 *
 * Gets the debugger instance, if it is loaded.
 *
 * Returns: (transfer none) (nullable): An #IdeDebugger or %NULL
 */
IdeDebugger *
ide_debug_manager_get_debugger (IdeDebugManager *self)
{
  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), NULL);

  return self->debugger;
}

/**
 * ide_debug_manager_get_breakpoints_for_file:
 *
 * This returns an #IdeDebuggerBreakpoints that represents the breakpoints
 * within a given file.
 *
 * This inderect breakpoints container provides a very fast way to check if
 * a line has a breakpoint set. You want to use this when performance really
 * matters such as from the gutter of the source editor.
 *
 * Breakpoints contained in the resulting structure will automatically
 * propagate to the debugger when the debugger has been successfully spawned.
 *
 * Returns: (transfer full): An #IdeDebuggerBreakpoints
 */
IdeDebuggerBreakpoints *
ide_debug_manager_get_breakpoints_for_file (IdeDebugManager *self,
                                            GFile           *file)
{
  IdeDebuggerBreakpoints *breakpoints;

  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  breakpoints = g_hash_table_lookup (self->breakpoints, file);

  if (breakpoints == NULL)
    {
      breakpoints = g_object_new (IDE_TYPE_DEBUGGER_BREAKPOINTS,
                                  "file", file,
                                  NULL);
      g_hash_table_insert (self->breakpoints, g_object_ref (file), breakpoints);
    }

  return g_object_ref (breakpoints);
}

/**
 * _ide_debug_manager_add_breakpoint:
 * @self: An #IdeDebugManager
 * @breakpoint: An #IdeDebuggerBreakpoint
 *
 * This adds a new breakpoint. If the debugger has been started, it
 * is done by notifying the debugger to add the breakpoint. If there is
 * not an active debugger, then it is done by caching the breakpoint
 * until the debugger is next started.
 *
 * Since: 3.26
 */
void
_ide_debug_manager_add_breakpoint (IdeDebugManager       *self,
                                   IdeDebuggerBreakpoint *breakpoint)
{
  g_autoptr(IdeDebuggerBreakpoints) breakpoints = NULL;
  g_autoptr(GFile) file = NULL;
  const gchar *path;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUG_MANAGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  if (self->debugger != NULL)
    {
      ide_debugger_insert_breakpoint_async (self->debugger, breakpoint, NULL, NULL, NULL);
      IDE_EXIT;
    }

  path = ide_debugger_breakpoint_get_file (breakpoint);

  if (path == NULL)
    {
      /* We don't know where this breakpoint is because it's either an
       * address, function, expression, etc. So we just need to queue
       * it until the debugger starts.
       */
      g_queue_push_tail (&self->pending_breakpoints, g_object_ref (breakpoint));
      IDE_EXIT;
    }

  file = g_file_new_for_path (path);
  breakpoints = ide_debug_manager_get_breakpoints_for_file (self, file);
  _ide_debugger_breakpoints_add (breakpoints, breakpoint);

  IDE_EXIT;
}

/**
 * _ide_debug_manager_remove_breakpoint:
 * @self: An #IdeDebugManager
 * @breakpoint: An #IdeDebuggerBreakpoint
 *
 * This removes an exiting breakpoint. If the debugger has been started, it
 * is done by notifying the debugger to remove the breakpoint. If there is
 * not an active debugger, then it is done by removing the cached breakpoint.
 *
 * Since: 3.26
 */
void
_ide_debug_manager_remove_breakpoint (IdeDebugManager       *self,
                                      IdeDebuggerBreakpoint *breakpoint)
{
  g_autoptr(GFile) file = NULL;
  IdeDebuggerBreakpoints *breakpoints;
  const gchar *path;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUG_MANAGER (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  if (self->debugger != NULL)
    {
      /* Just ask the debugger to remove it, we'll update the breakpoints
       * list when we get the #IdeDebugger::breakpoint-removed signal.
       */
      ide_debugger_remove_breakpoint_async (self->debugger, breakpoint, NULL, NULL, NULL);
      IDE_EXIT;
    }

  /* Nothing we can do if this is a memory address-based breakpoint */
  if (NULL == (path = ide_debugger_breakpoint_get_file (breakpoint)))
    IDE_EXIT;

  file = g_file_new_for_path (path);
  breakpoints = g_hash_table_lookup (self->breakpoints, file);
  if (breakpoints != NULL)
    _ide_debugger_breakpoints_remove (breakpoints, breakpoint);

  IDE_EXIT;
}
