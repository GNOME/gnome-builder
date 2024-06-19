/* gbp-gdb-debugger.c
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

#define G_LOG_DOMAIN "gbp-gdb-debugger"

#include "config.h"

#include <string.h>
#include <unistd.h>

#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <libide-io.h>
#include <libide-threading.h>
#include <libide-terminal.h>

#include "gbp-gdb-debugger.h"

#define READ_BUFFER_LEN 4096

struct _GbpGdbDebugger
{
  IdeDebugger               parent_instance;

  GIOStream                *io_stream;
  gchar                    *read_buffer;
  GCancellable             *read_cancellable;
  GHashTable               *register_names;
  GFile                    *builddir;
  IdeConfig                *current_config;

  struct gdbwire_mi_parser *parser;

  GQueue                    writequeue;
  GQueue                    cmdqueue;
  guint                     cmdseq;

  guint                     has_connected : 1;
};

typedef struct
{
  GMainContext *context;
  struct gdbwire_mi_output *output;
  GError **error;
  gboolean completed;
} SyncHandle;

G_DEFINE_FINAL_TYPE (GbpGdbDebugger, gbp_gdb_debugger, IDE_TYPE_DEBUGGER)

#define DEBUG_LOG(dir,msg)                                 \
  G_STMT_START {                                           \
    IdeLineReader reader;                                  \
    const gchar *line;                                     \
    gsize len;                                             \
    ide_line_reader_init (&reader, msg, -1);               \
    while ((line = ide_line_reader_next (&reader, &len)))  \
      {                                                    \
        g_autofree gchar *copy = g_strndup (line, len);    \
        g_debug ("%s: %s", dir, copy);                     \
      }                                                    \
  } G_STMT_END

static void
gbp_gdb_debugger_parent_set (IdeObject *object,
                             IdeObject *parent)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  const gchar *builddir;
  IdeContext *context;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  context = ide_object_get_context (parent);

  /*
   * We need to save the build directory so that we can translate
   * relative paths coming from gdb into the path within the project
   * source tree.
   */

  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);
  builddir = ide_pipeline_get_builddir (pipeline);

  g_clear_object (&self->builddir);
  self->builddir = g_file_new_for_path (builddir);
}

static inline gboolean
isoctal (int c)
{
  return c >= '0' && c < '8';
}

static char *
decode_maybe_octal (const char *str)
{
  if (strchr (str, '\\') == NULL)
    return g_strdup (str);
  else
    {
      GByteArray *ba = g_byte_array_new ();
      static const guint8 zero[1] = {0};

      /* It looks like gdb is encoding UTF-8 as octal bytes in the string
       * rather than as UTF-8 inline.
       */

      for (const char *c = str; *c; ++c)
        {
          if (c[0] == '\\' && isoctal (c[1]) && isoctal (c[2]) && isoctal (c[3]))
            {
              guint8 b = ((c[1] - '0') * 8 * 8) + ((c[2] - '0') * 8) + (c[3] - '0');
              g_byte_array_append (ba, &b, 1);
              c += 3;
            }
          else
            g_byte_array_append (ba, (const guint8 *)c, 1);
        }

      g_byte_array_append (ba, zero, 1);

      return (char *)g_byte_array_free (ba, FALSE);
    }
}

static gchar *
gbp_gdb_debugger_translate_path (GbpGdbDebugger *self,
                                 const gchar    *path)
{
  g_autofree char *decoded = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));

  if (path == NULL)
    return NULL;

  decoded = decode_maybe_octal (path);

  /* Generate a path, trying to resolve relative paths to
   * make things easier on the runtime path translation.
   */
  if (self->builddir == NULL || g_path_is_absolute (decoded))
    file = g_file_new_for_path (decoded);
  else
    file = g_file_resolve_relative_path (self->builddir, decoded);

  /* If we still have access to the config, translate */
  if (self->current_config != NULL)
    {
      g_autoptr(GFile) translated = ide_config_translate_file (self->current_config, file);

      if (translated != NULL)
        g_set_object (&file, translated);
    }

  return g_file_get_path (file);
}

static void
gbp_gdb_debugger_panic (GbpGdbDebugger *self)
{
  GList *list;

  g_assert (GBP_IS_GDB_DEBUGGER (self));

  list = self->cmdqueue.head;

  self->cmdqueue.head = NULL;
  self->cmdqueue.tail = NULL;
  self->cmdqueue.length = 0;

  for (const GList *iter = list; iter != NULL; iter = iter->next)
    {
      g_autoptr(IdeTask) task = iter->data;

      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "There was a communication failure");
    }

  g_list_free (list);
}

static gboolean
gbp_gdb_debugger_unwrap (const struct gdbwire_mi_output  *output,
                         GError                         **error)
{
  if (output == NULL)
    return FALSE;

  if (output->variant.result_record->result_class == GDBWIRE_MI_ERROR)
    {
      const gchar *msg = output->line;

      if (output->variant.result_record->result != NULL &&
          output->variant.result_record->result->kind == GDBWIRE_MI_CSTRING)
        msg = output->variant.result_record->result->variant.cstring;

      g_debug ("gdb-error: %s", msg);

      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           msg);
      return TRUE;
    }

  return FALSE;
}

static IdeTask *
gbp_gdb_debugger_find_task (GbpGdbDebugger           *self,
                            struct gdbwire_mi_output *output)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output != NULL);
  g_assert (output->line != NULL);

  if (g_ascii_isdigit (output->line[0]))
    {
      g_autofree gchar *id = NULL;
      guint i;

      for (i = 1; g_ascii_isdigit (output->line[i]); i++)
        continue;

      id = g_strndup (output->line, i);

      for (GList *iter = self->cmdqueue.head; iter; iter = iter->next)
        {
          IdeTask *task = iter->data;
          const gchar *task_id = ide_task_get_task_data (task);

          if (strcmp (id, task_id) == 0)
            {
              g_queue_delete_link (&self->cmdqueue, iter);
              return task;
            }
        }
    }

  return NULL;
}

static void
gbp_gdb_debugger_cache_register_names (GbpGdbDebugger           *self,
                                       struct gdbwire_mi_output *output)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output != NULL);

  if (output->kind == GDBWIRE_MI_OUTPUT_RESULT &&
      output->variant.result_record->result->kind == GDBWIRE_MI_LIST &&
      g_strcmp0 (output->variant.result_record->result->variable, "register-names") == 0)
    {
      const struct gdbwire_mi_result *res = output->variant.result_record->result;
      const struct gdbwire_mi_result *iter;
      g_autoptr(GHashTable) hash = NULL;
      guint i = 0;

      hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

      for (iter = res->variant.result; iter; iter = iter->next, i++)
        {
          if (iter->kind == GDBWIRE_MI_CSTRING)
            g_hash_table_insert (hash,
                                 g_strdup_printf ("%u", i),
                                 g_strdup (iter->variant.cstring));
        }

      g_clear_pointer (&self->register_names, g_hash_table_unref);
      self->register_names = g_steal_pointer (&hash);
    }
}

static void
gbp_gdb_debugger_handle_result (GbpGdbDebugger           *self,
                                struct gdbwire_mi_output *output)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output->kind == GDBWIRE_MI_OUTPUT_RESULT);
  g_assert (output->line != NULL);

  task = gbp_gdb_debugger_find_task (self, output);

  if (task != NULL)
    {
      ide_task_return_pointer (task, output, gdbwire_mi_output_free);
      return;
    }

  ide_object_warning (self, "gdb: No reply found for: %s", output->line);

  gdbwire_mi_output_free (output);
}

static void
gbp_gdb_debugger_handle_thread_group (GbpGdbDebugger                 *self,
                                      struct gdbwire_mi_output       *output,
                                      struct gdbwire_mi_async_record *rec)
{
  g_autoptr(IdeDebuggerThreadGroup) thread_group = NULL;
  const struct gdbwire_mi_result *iter;
  const gchar *id = NULL;
  const gchar *pid = NULL;
  const gchar *exit_code = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output != NULL);
  g_assert (rec != NULL);

  for (iter = rec->result; iter != NULL; iter = iter->next)
    {
      if (iter->kind == GDBWIRE_MI_CSTRING)
        {
          if (g_strcmp0 (iter->variable, "id") == 0)
            id = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "pid") == 0)
            pid = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "exit-code") == 0)
            exit_code = iter->variant.cstring;
        }
    }

  thread_group = ide_debugger_thread_group_new (id);
  ide_debugger_thread_group_set_pid (thread_group, pid);
  ide_debugger_thread_group_set_exit_code (thread_group, exit_code);

  switch ((gint)rec->async_class)
    {
    case GDBWIRE_MI_ASYNC_THREAD_GROUP_ADDED:
      ide_debugger_emit_thread_group_added (IDE_DEBUGGER (self), thread_group);
      break;

    case GDBWIRE_MI_ASYNC_THREAD_GROUP_EXITED:
      ide_debugger_emit_thread_group_exited (IDE_DEBUGGER (self), thread_group);
      break;

    case GDBWIRE_MI_ASYNC_THREAD_GROUP_REMOVED:
      ide_debugger_emit_thread_group_removed (IDE_DEBUGGER (self), thread_group);
      break;

    case GDBWIRE_MI_ASYNC_THREAD_GROUP_STARTED:
      ide_debugger_emit_thread_group_started (IDE_DEBUGGER (self), thread_group);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gbp_gdb_debugger_handle_thread (GbpGdbDebugger                 *self,
                                struct gdbwire_mi_output       *output,
                                struct gdbwire_mi_async_record *rec)
{
  g_autoptr(IdeDebuggerThread) thread = NULL;
  const struct gdbwire_mi_result *iter;
  const gchar *id = NULL;
  const gchar *group_id = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output != NULL);
  g_assert (rec != NULL);

  for (iter = rec->result; iter != NULL; iter = iter->next)
    {
      if (iter->kind == GDBWIRE_MI_CSTRING)
        {
          if (g_strcmp0 (iter->variable, "id") == 0)
            id = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "group-id") == 0)
            group_id = iter->variant.cstring;
        }
    }

  thread = ide_debugger_thread_new (id);
  ide_debugger_thread_set_group (thread, group_id);

  switch ((gint)rec->async_class)
    {
    case GDBWIRE_MI_ASYNC_THREAD_CREATED:
      ide_debugger_emit_thread_added (IDE_DEBUGGER (self), thread);
      break;

    case GDBWIRE_MI_ASYNC_THREAD_EXITED:
      ide_debugger_emit_thread_removed (IDE_DEBUGGER (self), thread);
      break;

    case GDBWIRE_MI_ASYNC_THREAD_SELECTED:
      ide_debugger_emit_thread_selected (IDE_DEBUGGER (self), thread);
      break;

    default:
      g_assert_not_reached ();
    }
}

static IdeDebuggerBreakMode
parse_mode_from_string (const gchar *str)
{
  if (str == NULL || g_str_equal (str, "breakpoint"))
    return IDE_DEBUGGER_BREAK_BREAKPOINT;
  else if (g_str_equal (str, "countpoint"))
    return IDE_DEBUGGER_BREAK_COUNTPOINT;
  else if (g_str_equal (str, "watchpoint"))
    return IDE_DEBUGGER_BREAK_WATCHPOINT;
  else
    return IDE_DEBUGGER_BREAK_BREAKPOINT;
}

static IdeDebuggerDisposition
parse_disposition_from_string (const gchar *str)
{
  if (str != NULL)
    {
      if (g_str_equal (str, "dis"))
        return IDE_DEBUGGER_DISPOSITION_DISABLE;
      else if (g_str_equal (str, "del"))
        return IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_HIT;
      else if (g_str_equal (str, "keep"))
        return IDE_DEBUGGER_DISPOSITION_KEEP;
      else if (g_str_equal (str, "dstp"))
        return IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_STOP;
    }

  return IDE_DEBUGGER_DISPOSITION_KEEP;
}

static void
gbp_gdb_debugger_handle_breakpoint (GbpGdbDebugger              *self,
                                    struct gdbwire_mi_output    *output,
                                    struct gdbwire_mi_result    *res,
                                    enum gdbwire_mi_async_class  klass)
{
  g_autoptr(IdeDebuggerBreakpoint) breakpoint = NULL;
  g_autofree gchar *file = NULL;
  g_autofree gchar *fullname = NULL;
  const struct gdbwire_mi_result *iter;
  G_GNUC_UNUSED const gchar *original_location = NULL;
  gboolean enabled = FALSE;
  const gchar *id = NULL;
  const gchar *type = NULL;
  const gchar *disp = NULL;
  const gchar *addr = NULL;
  const gchar *times = NULL;
  const gchar *func = NULL;
  const gchar *line = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output != NULL);

  for (iter = res; iter != NULL; iter = iter->next)
    {
    try_again:

      if (iter->kind == GDBWIRE_MI_CSTRING)
        {
          if (g_strcmp0 (iter->variable, "id") == 0 ||
              g_strcmp0 (iter->variable, "number") == 0)
            id = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "type") == 0)
            type = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "disp") == 0)
            disp = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "enabled") == 0)
            enabled = (iter->variant.cstring[0] == 'y');
          else if (g_strcmp0 (iter->variable, "addr") == 0)
            addr = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "times") == 0)
            times = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "func") == 0)
            func = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "file") == 0)
            {
              g_free (file);
              file = gbp_gdb_debugger_translate_path (self, iter->variant.cstring);
            }
          else if (g_strcmp0 (iter->variable, "fullname") == 0)
            {
              g_free (fullname);
              fullname = gbp_gdb_debugger_translate_path (self, iter->variant.cstring);
            }
          else if (g_strcmp0 (iter->variable, "line") == 0)
            line = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "original-location") == 0)
            original_location = iter->variant.cstring;
        }
      else if (iter->kind == GDBWIRE_MI_TUPLE)
        {
          /* Dive into the bkpt={} if we got one */
          if (g_strcmp0 (iter->variable, "bkpt") == 0)
            {
              iter = iter->variant.result;
              goto try_again;
            }
        }
    }

  breakpoint = ide_debugger_breakpoint_new (id);

  ide_debugger_breakpoint_set_mode (breakpoint, parse_mode_from_string (type));
  ide_debugger_breakpoint_set_disposition (breakpoint, parse_disposition_from_string (disp));
  ide_debugger_breakpoint_set_address (breakpoint, ide_debugger_address_parse (addr));
  ide_debugger_breakpoint_set_function (breakpoint, func);
  ide_debugger_breakpoint_set_enabled (breakpoint, enabled);

  if (fullname != NULL && g_file_test (fullname, G_FILE_TEST_EXISTS))
    ide_debugger_breakpoint_set_file (breakpoint, fullname);
  else
    ide_debugger_breakpoint_set_file (breakpoint, file);

  if (line != NULL)
    ide_debugger_breakpoint_set_line (breakpoint, g_ascii_strtoll (line, NULL, 10));

  if (times != NULL)
    ide_debugger_breakpoint_set_count (breakpoint, g_ascii_strtoll (times, NULL, 10));

  switch ((gint)klass)
    {
    case GDBWIRE_MI_ASYNC_BREAKPOINT_CREATED:
      ide_debugger_emit_breakpoint_added (IDE_DEBUGGER (self), breakpoint);
      break;

    case GDBWIRE_MI_ASYNC_BREAKPOINT_DELETED:
      ide_debugger_emit_breakpoint_removed (IDE_DEBUGGER (self), breakpoint);
      break;

    case GDBWIRE_MI_ASYNC_BREAKPOINT_MODIFIED:
      ide_debugger_emit_breakpoint_modified (IDE_DEBUGGER (self), breakpoint);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gbp_gdb_debugger_handle_stopped (GbpGdbDebugger                 *self,
                                 struct gdbwire_mi_output       *output,
                                 struct gdbwire_mi_async_record *rec)
{
  IdeDebuggerStopReason stop_reason;
  g_autoptr(IdeDebuggerBreakpoint) breakpoint = NULL;
  g_autoptr(IdeDebuggerThread) thread = NULL;
  g_autofree gchar *file = NULL;
  g_autofree gchar *fullname = NULL;
  const gchar *thread_id = NULL;
  const gchar *group_id = NULL;
  const struct gdbwire_mi_result *iter;
  const gchar *id = NULL;
  const gchar *reason = NULL;
  const gchar *disp = NULL;
  const gchar *func = NULL;
  const gchar *address = NULL;
  guint line = 0;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output != NULL);
  g_assert (rec != NULL);

  /*
   * Example:
   *
   * *stopped,reason="breakpoint-hit",disp="keep",bkptno="1",
   *      frame={addr="0x0000555555572c70",func="main",args=[]},
   *      thread-id="1",stopped-threads="all",core="2"
   */

  for (iter = rec->result; iter != NULL; iter = iter->next)
    {
      if (iter->kind == GDBWIRE_MI_CSTRING)
        {
          if (g_strcmp0 (iter->variable, "thread-id") == 0)
            thread_id = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "reason") == 0)
            reason = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "disp") == 0)
            disp = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "bkptno") == 0)
            id = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "group-id") == 0)
            group_id = iter->variant.cstring;
        }
      else if (iter->kind == GDBWIRE_MI_TUPLE)
        {
          if (g_strcmp0 (iter->variable, "frame") == 0)
            {
              const struct gdbwire_mi_result *subiter;

              /*
               * So the question here ... is should we inflate an entire
               * stack frame instance for this, or simply stash it in the
               * breakpoint function property ...
               *
               * We will have another way to get stack frames, so we don't
               * exactly need it here...
               */

              for (subiter = iter->variant.result; subiter != NULL; subiter = subiter->next)
                {
                  if (subiter->kind == GDBWIRE_MI_CSTRING)
                    {
                      if (g_strcmp0 (subiter->variable, "func") == 0)
                        func = subiter->variant.cstring;
                      else if (g_strcmp0 (subiter->variable, "address") == 0)
                        address = subiter->variant.cstring;
                      else if (g_strcmp0 (subiter->variable, "file") == 0)
                        {
                          g_free (file);
                          file = gbp_gdb_debugger_translate_path (self, subiter->variant.cstring);
                        }
                      else if (g_strcmp0 (subiter->variable, "fullname") == 0)
                        {
                          g_free (fullname);
                          fullname = gbp_gdb_debugger_translate_path (self, subiter->variant.cstring);
                        }
                      else if (g_strcmp0 (subiter->variable, "line") == 0)
                        line = g_ascii_strtoll (subiter->variant.cstring, NULL, 10);
                    }
                }
            }
        }
    }

  if (FALSE) {}
  else if (g_strcmp0 (reason, "exited-normally") == 0)
    stop_reason = IDE_DEBUGGER_STOP_EXITED_NORMALLY;
  else if (g_strcmp0 (reason, "breakpoint-hit") == 0)
    stop_reason = IDE_DEBUGGER_STOP_BREAKPOINT_HIT;
  else if (g_strcmp0 (reason, "function-finished") == 0)
    stop_reason = IDE_DEBUGGER_STOP_FUNCTION_FINISHED;
  else if (g_strcmp0 (reason, "location-reached") == 0)
    stop_reason = IDE_DEBUGGER_STOP_LOCATION_REACHED;
  else if (g_strcmp0 (reason, "exited-signaled") == 0)
    stop_reason = IDE_DEBUGGER_STOP_EXITED_SIGNALED;
  else if (g_strcmp0 (reason, "exited") == 0)
    stop_reason = IDE_DEBUGGER_STOP_EXITED;
  else if (g_strcmp0 (reason, "exited-normally") == 0)
    stop_reason = IDE_DEBUGGER_STOP_EXITED_NORMALLY;
  else if (g_strcmp0 (reason, "signal-received") == 0)
    stop_reason = IDE_DEBUGGER_STOP_SIGNAL_RECEIVED;
  else if (g_strcmp0 (reason, "solib-event") == 0 ||
           g_strcmp0 (reason, "fork") == 0 ||
           g_strcmp0 (reason, "vfork") == 0 ||
           g_strcmp0 (reason, "syscall-entry") == 0 ||
           g_strcmp0 (reason, "syscall-return") == 0 ||
           g_strcmp0 (reason, "exec") == 0)
    stop_reason = IDE_DEBUGGER_STOP_CATCH;
  else
    stop_reason = IDE_DEBUGGER_STOP_UNKNOWN;

  breakpoint = ide_debugger_breakpoint_new (id);
  ide_debugger_breakpoint_set_thread (breakpoint, thread_id);
  ide_debugger_breakpoint_set_address (breakpoint, ide_debugger_address_parse (address));
  ide_debugger_breakpoint_set_function (breakpoint, func);
  ide_debugger_breakpoint_set_line (breakpoint, line);
  ide_debugger_breakpoint_set_disposition (breakpoint, parse_disposition_from_string (disp));

  if (fullname != NULL && g_file_test (fullname, G_FILE_TEST_EXISTS))
    ide_debugger_breakpoint_set_file (breakpoint, fullname);
  else
    ide_debugger_breakpoint_set_file (breakpoint, file);

  gbp_gdb_debugger_reload_breakpoints (self);

  thread = ide_debugger_thread_new (thread_id);
  ide_debugger_thread_set_group (thread, group_id);

  ide_debugger_emit_thread_selected (IDE_DEBUGGER (self), thread);
  ide_debugger_emit_stopped (IDE_DEBUGGER (self), stop_reason, breakpoint);

  /* Currently, we expect to have gdb exit with the program. We might change that
   * at some point, but it's currently the expectation.
   */
  if (stop_reason == IDE_DEBUGGER_STOP_EXITED_SIGNALED ||
      stop_reason == IDE_DEBUGGER_STOP_EXITED_NORMALLY ||
      stop_reason == IDE_DEBUGGER_STOP_EXITED)
    gbp_gdb_debugger_exec_async (self, "-gdb-exit", NULL, NULL, NULL);
}

static void
gbp_gdb_debugger_handle_library (GbpGdbDebugger                 *self,
                                 struct gdbwire_mi_output       *output,
                                 struct gdbwire_mi_async_record *rec)
{
  g_autoptr(IdeDebuggerLibrary) library = NULL;
  const struct gdbwire_mi_result *iter;
  G_GNUC_UNUSED const gchar *thread_group = NULL;
  G_GNUC_UNUSED const gchar *symbols_loaded = NULL;
  const gchar *target_name = NULL;
  const gchar *host_name = NULL;
  g_autoptr(GArray) ranges = NULL;
  const gchar *id = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output != NULL);
  g_assert (rec != NULL);

  /*
   * Example:
   *
   * Async record of a library loading looks something like this:
   *
   * =library-loaded,id="/lib64/libfreebl3.so",target-name="/lib64/libfreebl3.so",
   *     host-name="/lib64/libfreebl3.so",symbols-loaded="0",
   *     thread-group="i1",ranges=[{from="0x00007fffdfdfdab0",to="0x00007fffdfdfe1f0"}]
   */

  ranges = g_array_new (FALSE, FALSE, sizeof (IdeDebuggerAddressRange));

  for (iter = rec->result; iter != NULL; iter = iter->next)
    {
      if (iter->kind == GDBWIRE_MI_CSTRING)
        {
          if (g_strcmp0 (iter->variable, "id") == 0)
            id = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "target-name") == 0)
            target_name = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "host-name") == 0)
            host_name = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "symbols-loaded") == 0)
            symbols_loaded = iter->variant.cstring;
          else if (g_strcmp0 (iter->variable, "thread-group") == 0)
            thread_group = iter->variant.cstring;
        }
      else if (iter->kind == GDBWIRE_MI_LIST)
        {
          const struct gdbwire_mi_result *liter;

          if (g_strcmp0 (iter->variable, "ranges") == 0)
            {
              for (liter = iter->variant.result; liter; liter = liter->next)
                {
                  if (liter->kind == GDBWIRE_MI_TUPLE)
                    {
                      const struct gdbwire_mi_result *titer;
                      IdeDebuggerAddressRange range = { 0 };

                      for (titer = liter->variant.result; titer; titer = titer->next)
                        {
                          if (titer->kind == GDBWIRE_MI_CSTRING)
                            {
                              if (g_strcmp0 (titer->variable, "from") == 0)
                                range.from = ide_debugger_address_parse (titer->variant.cstring);
                              else if (g_strcmp0 (titer->variable, "to") == 0)
                                range.to = ide_debugger_address_parse (titer->variant.cstring);
                            }
                        }

                      if (range.from != IDE_DEBUGGER_ADDRESS_INVALID &&
                          range.to != IDE_DEBUGGER_ADDRESS_INVALID)
                        g_array_append_val (ranges, range);
                    }
                }
            }
        }
    }

  library = ide_debugger_library_new (id);

  ide_debugger_library_set_host_name (library, host_name);
  ide_debugger_library_set_target_name (library, target_name);

  for (guint i = 0; i < ranges->len; i++)
    {
      const IdeDebuggerAddressRange *range = &g_array_index (ranges, IdeDebuggerAddressRange, i);

      ide_debugger_library_add_range (library, range);
    }

  if (rec->async_class == GDBWIRE_MI_ASYNC_LIBRARY_LOADED)
    ide_debugger_emit_library_loaded (IDE_DEBUGGER (self), library);
  else if (rec->async_class == GDBWIRE_MI_ASYNC_LIBRARY_UNLOADED)
    ide_debugger_emit_library_unloaded (IDE_DEBUGGER (self), library);
  else
    g_assert_not_reached ();
}

static void
gbp_gdb_debugger_handle_oob_async_notify (GbpGdbDebugger           *self,
                                          struct gdbwire_mi_output *output)
{
  struct gdbwire_mi_async_record *rec;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output->kind == GDBWIRE_MI_OUTPUT_OOB);
  g_assert (output->variant.oob_record->kind == GDBWIRE_MI_ASYNC);
  g_assert (output->variant.oob_record->variant.async_record->kind == GDBWIRE_MI_NOTIFY ||
            output->variant.oob_record->variant.async_record->kind == GDBWIRE_MI_EXEC);

  rec = output->variant.oob_record->variant.async_record;

  /* Handle information about things like new breakpoints, etc */

  switch (rec->async_class)
    {
    case GDBWIRE_MI_ASYNC_THREAD_GROUP_ADDED:
    case GDBWIRE_MI_ASYNC_THREAD_GROUP_EXITED:
    case GDBWIRE_MI_ASYNC_THREAD_GROUP_REMOVED:
    case GDBWIRE_MI_ASYNC_THREAD_GROUP_STARTED:
      gbp_gdb_debugger_handle_thread_group (self, output, rec);
      break;

    case GDBWIRE_MI_ASYNC_THREAD_CREATED:
    case GDBWIRE_MI_ASYNC_THREAD_EXITED:
    case GDBWIRE_MI_ASYNC_THREAD_SELECTED:
      gbp_gdb_debugger_handle_thread (self, output, rec);
      break;

    case GDBWIRE_MI_ASYNC_BREAKPOINT_CREATED:
    case GDBWIRE_MI_ASYNC_BREAKPOINT_DELETED:
    case GDBWIRE_MI_ASYNC_BREAKPOINT_MODIFIED:
      gbp_gdb_debugger_handle_breakpoint (self, output, rec->result, rec->async_class);
      break;

    case GDBWIRE_MI_ASYNC_RUNNING:
      ide_debugger_emit_running (IDE_DEBUGGER (self));

      if (!ide_debugger_get_selected_thread (IDE_DEBUGGER (self)))
        {
          g_autoptr(IdeDebuggerThread) thread = ide_debugger_thread_new ("1");

          /* GDB doesn't notify us of our first selected thread */
          ide_debugger_emit_thread_selected (IDE_DEBUGGER (self), thread);
        }

      break;

    case GDBWIRE_MI_ASYNC_STOPPED:
      gbp_gdb_debugger_handle_stopped (self, output, rec);
      break;

    case GDBWIRE_MI_ASYNC_LIBRARY_LOADED:
    case GDBWIRE_MI_ASYNC_LIBRARY_UNLOADED:
      gbp_gdb_debugger_handle_library (self, output, rec);
      break;

    case GDBWIRE_MI_ASYNC_CMD_PARAM_CHANGED:
    case GDBWIRE_MI_ASYNC_DOWNLOAD:
    case GDBWIRE_MI_ASYNC_MEMORY_CHANGED:
    case GDBWIRE_MI_ASYNC_RECORD_STARTED:
    case GDBWIRE_MI_ASYNC_RECORD_STOPPED:
    case GDBWIRE_MI_ASYNC_TRACEFRAME_CHANGED:
    case GDBWIRE_MI_ASYNC_TSV_CREATED:
    case GDBWIRE_MI_ASYNC_TSV_DELETED:
    case GDBWIRE_MI_ASYNC_TSV_MODIFIED:
      break;

    case GDBWIRE_MI_ASYNC_UNSUPPORTED:
    default:
      g_return_if_reached ();
    }
}

static void
gbp_gdb_debugger_handle_oob_async_record (GbpGdbDebugger           *self,
                                          struct gdbwire_mi_output *output)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output->kind == GDBWIRE_MI_OUTPUT_OOB);
  g_assert (output->variant.oob_record->kind == GDBWIRE_MI_ASYNC);

  switch (output->variant.oob_record->variant.async_record->kind)
    {
    case GDBWIRE_MI_STATUS:
      /* discard */
      break;

    case GDBWIRE_MI_EXEC:
    case GDBWIRE_MI_NOTIFY:
      gbp_gdb_debugger_handle_oob_async_notify (self, output);
      break;

    default:
      g_assert_not_reached ();
    }
}


static void
gbp_gdb_debugger_handle_oob (GbpGdbDebugger           *self,
                             struct gdbwire_mi_output *output)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output->kind == GDBWIRE_MI_OUTPUT_OOB);

  switch (output->variant.oob_record->kind)
    {
    case GDBWIRE_MI_ASYNC:
      gbp_gdb_debugger_handle_oob_async_record (self, output);
      break;

    case GDBWIRE_MI_STREAM:
      {
        IdeDebuggerStream stream;
        g_autoptr(GBytes) content = NULL;
        const gchar *data;
        gsize len;

        switch (output->variant.oob_record->variant.stream_record->kind)
          {
          case GDBWIRE_MI_CONSOLE:
            stream = IDE_DEBUGGER_CONSOLE;
            break;

          case GDBWIRE_MI_TARGET:
            stream = IDE_DEBUGGER_TARGET;
            break;

          case GDBWIRE_MI_LOG:
          default:
            stream = IDE_DEBUGGER_EVENT_LOG;
            break;
          }

        data = output->variant.oob_record->variant.stream_record->cstring;
        len = strlen (data);
        content = g_bytes_new (data, len);

        ide_debugger_emit_log (IDE_DEBUGGER (self), stream, content);
      }
      break;

    default:
      g_return_if_reached ();
    }
}

static void
gbp_gdb_debugger_output_callback (void                     *context,
                                  struct gdbwire_mi_output *output)
{
  GbpGdbDebugger *self = context;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (output != NULL);

  switch (output->kind)
    {
    case GDBWIRE_MI_OUTPUT_PARSE_ERROR:
      ide_object_warning (self, "Failed to parse gdb communication: %s", output->line);
      gdbwire_mi_output_free (output);
      gbp_gdb_debugger_panic (self);
      break;

    case GDBWIRE_MI_OUTPUT_OOB:
      DEBUG_LOG ("from-gdb (OOB)", output->line);
      gbp_gdb_debugger_handle_oob (self, output);
      gdbwire_mi_output_free (output);
      break;

    case GDBWIRE_MI_OUTPUT_RESULT:
      DEBUG_LOG ("from-gdb (RES)", output->line);
      /* handle result steals output pointer */
      gbp_gdb_debugger_handle_result (self, output);
      break;

    case GDBWIRE_MI_OUTPUT_PROMPT:
      /* Ignore prompt for now */
      gdbwire_mi_output_free (output);
      break;

    default:
      g_warning ("Unhandled output type: %d", output->kind);
      gdbwire_mi_output_free (output);
    }
}

static void
gbp_gdb_debugger_list_breakpoints_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GError) error = NULL;
  struct gdbwire_mi_command *command = NULL;
  struct gdbwire_mi_output *output;
  enum gdbwire_result res = GDBWIRE_LOGIC;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (output->kind == GDBWIRE_MI_OUTPUT_RESULT)
    res = gdbwire_get_mi_command (GDBWIRE_MI_BREAK_INFO,
                                  output->variant.result_record,
                                  &command);

  if (res != GDBWIRE_OK)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Invalid reply from gdb");
      return;
    }

  g_assert (command != NULL);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  if (command->kind == GDBWIRE_MI_BREAK_INFO)
    {
      const struct gdbwire_mi_breakpoint *iter;

      for (iter = command->variant.break_info.breakpoints; iter; iter = iter->next)
        {
          g_autoptr(IdeDebuggerBreakpoint) breakpoint = NULL;
          IdeDebuggerDisposition disp;
          g_autofree gchar *fullname = NULL;
          g_autofree gchar *file = NULL;

          breakpoint = ide_debugger_breakpoint_new (iter->number);

          ide_debugger_breakpoint_set_address (breakpoint,
                                               ide_debugger_address_parse (iter->address));
          ide_debugger_breakpoint_set_function (breakpoint, iter->func_name);
          ide_debugger_breakpoint_set_line (breakpoint, iter->line);
          ide_debugger_breakpoint_set_count (breakpoint, iter->times);

          fullname = gbp_gdb_debugger_translate_path (self, iter->fullname);
          file = gbp_gdb_debugger_translate_path (self, iter->file);

          if (fullname != NULL && g_file_test (fullname, G_FILE_TEST_EXISTS))
            ide_debugger_breakpoint_set_file (breakpoint, fullname);
          else
            ide_debugger_breakpoint_set_file (breakpoint, file);

          switch (iter->disposition)
            {
            case GDBWIRE_MI_BP_DISP_DELETE:
              disp = IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_HIT;
              break;

            case GDBWIRE_MI_BP_DISP_DELETE_NEXT_STOP:
              disp = IDE_DEBUGGER_DISPOSITION_DELETE_NEXT_STOP;
              break;

            case GDBWIRE_MI_BP_DISP_DISABLE:
              disp = IDE_DEBUGGER_DISPOSITION_DISABLE;
              break;

            case GDBWIRE_MI_BP_DISP_UNKNOWN:
            case GDBWIRE_MI_BP_DISP_KEEP:
            default:
              disp = IDE_DEBUGGER_DISPOSITION_KEEP;
              break;
            }

          ide_debugger_breakpoint_set_disposition (breakpoint, disp);

          g_ptr_array_add (ar, g_steal_pointer (&breakpoint));
        }
    }

  ide_task_return_pointer (task, g_steal_pointer (&ar), g_ptr_array_unref);

  gdbwire_mi_command_free (command);
  gdbwire_mi_output_free (output);
}

static void
gbp_gdb_debugger_reload_breakpoints_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(GError) error = NULL;
  struct gdbwire_mi_output *output;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    {
      ide_object_warning (self, "%s", error->message);
      goto cleanup;
    }

  if (output->kind == GDBWIRE_MI_OUTPUT_RESULT &&
      output->variant.result_record != NULL &&
      output->variant.result_record->result_class == GDBWIRE_MI_DONE &&
      output->variant.result_record->result != NULL &&
      output->variant.result_record->result->kind == GDBWIRE_MI_TUPLE)
    {
      struct gdbwire_mi_result *tuple = output->variant.result_record->result;
      struct gdbwire_mi_result *iter;

      for (iter = tuple->variant.result; iter != NULL; iter = iter->next)
        {
          if (g_strcmp0 (iter->variable, "body") == 0 && iter->kind == GDBWIRE_MI_LIST)
            {
              struct gdbwire_mi_result *liter;

              for (liter = iter->variant.result; liter != NULL; liter = liter->next)
                {
                  if (g_strcmp0 (liter->variable, "bkpt") == 0)
                    gbp_gdb_debugger_handle_breakpoint (self,
                                                        output,
                                                        liter,
                                                        GDBWIRE_MI_ASYNC_BREAKPOINT_MODIFIED);
                }
            }
        }
    }

cleanup:
  g_clear_pointer (&output, gdbwire_mi_output_free);
}

/*
 * gbp_gdb_debugger_reload_breakpoints:
 * @self: a #GbpGdbDebugger
 *
 * Forces a reload of the breakpoints, emitting "breakpoint-modifed" for
 * modified breakpoints and "breakpoint-inserted" for new ones.
 *
 * This is processed asynchronously.
 */
void
gbp_gdb_debugger_reload_breakpoints (GbpGdbDebugger *self)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));

  gbp_gdb_debugger_exec_async (self,
                               "-break-list",
                               NULL,
                               gbp_gdb_debugger_reload_breakpoints_cb,
                               NULL);
}

static void
gbp_gdb_debugger_list_breakpoints_async (IdeDebugger         *debugger,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_list_breakpoints_async);

  gbp_gdb_debugger_exec_async (self,
                               "-break-info",
                               cancellable,
                               gbp_gdb_debugger_list_breakpoints_cb,
                               g_steal_pointer (&task));
}

static GPtrArray *
gbp_gdb_debugger_list_breakpoints_finish (IdeDebugger   *debugger,
                                          GAsyncResult  *result,
                                          GError       **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
gbp_gdb_debugger_insert_breakpoint_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  struct gdbwire_mi_output *output;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      g_clear_pointer (&output, gdbwire_mi_output_free);
      return;
    }

  if (output->kind == GDBWIRE_MI_OUTPUT_RESULT &&
      output->variant.result_record != NULL &&
      output->variant.result_record->result != NULL)
    {
      gbp_gdb_debugger_handle_breakpoint (self,
                                          output,
                                          output->variant.result_record->result,
                                          GDBWIRE_MI_ASYNC_BREAKPOINT_CREATED);
      ide_task_return_boolean (task, TRUE);
    }
  else
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to decode breakpoint reply");
    }

  gdbwire_mi_output_free (output);
}

static void
gbp_gdb_debugger_insert_breakpoint_async (IdeDebugger           *debugger,
                                          IdeDebuggerBreakpoint *breakpoint,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autofree gchar *translated_file = NULL;
  g_autoptr(GString) command = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) gfile = NULL;
  IdeDebuggerAddress addr;
  const gchar *func;
  const gchar *file;
  const gchar *spec;
  const gchar *thread;
  guint line;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_insert_breakpoint_async);
  ide_task_set_return_on_cancel (task, TRUE);

  command = g_string_new ("-break-insert");

  if (!ide_debugger_breakpoint_get_enabled (breakpoint))
    g_string_append (command, " -d");

  /*
   * We don't have a strict "countpoint", so we just set a really high
   * number for the ignore count in gdb.
   */
  if (ide_debugger_breakpoint_get_mode (breakpoint) == IDE_DEBUGGER_BREAK_COUNTPOINT)
    g_string_append_printf (command, " -i %d", G_MAXINT);

  file = ide_debugger_breakpoint_get_file (breakpoint);
  func = ide_debugger_breakpoint_get_function (breakpoint);
  line = ide_debugger_breakpoint_get_line (breakpoint);
  addr = ide_debugger_breakpoint_get_address (breakpoint);

  /* Possibly translate the file to be relative to builddir, as that is
   * what gdb seems to want from us. That is possibly going to be specific
   * to the build system, and we may need some more massaging here in the
   * future to get the right thing.
   */
  gfile = g_file_new_for_path (file);
  translated_file = ide_g_file_get_uncanonical_relative_path (self->builddir, gfile);
  if (translated_file != NULL)
    file = translated_file;

  if (file != NULL && line > 0)
    {
      g_string_append_printf (command, " --source %s", file);
      g_string_append_printf (command, " --line %u", line);
    }
  else if (file != NULL && func != NULL)
    {
      g_string_append_printf (command, " --source %s", file);
      g_string_append_printf (command, " --function %s", func);
    }
  else if (addr != 0)
    {
      g_string_append_printf (command, " *0x%"G_GINT64_MODIFIER"x", addr);
    }
  else
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to create breakpoint based on request");
      return;
    }

  if (0 != (thread = ide_debugger_breakpoint_get_thread (breakpoint)))
    g_string_append_printf (command, " -p %s", thread);

  if (0 != (spec = ide_debugger_breakpoint_get_spec (breakpoint)))
    g_string_append_printf (command, " -c %s", spec);

  gbp_gdb_debugger_exec_async (self,
                               command->str,
                               cancellable,
                               gbp_gdb_debugger_insert_breakpoint_cb,
                               g_steal_pointer (&task));
}

static gboolean
gbp_gdb_debugger_insert_breakpoint_finish (IdeDebugger   *debugger,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_gdb_debugger_remove_breakpoint_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  struct gdbwire_mi_output *output;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    {
      IdeDebuggerBreakpoint *breakpoint = ide_task_get_task_data (task);

      g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
      g_assert (ide_debugger_breakpoint_get_id (breakpoint) != NULL);

      ide_debugger_emit_breakpoint_removed (IDE_DEBUGGER (self), breakpoint);

      ide_task_return_boolean (task, TRUE);
    }

  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_remove_breakpoint_async (IdeDebugger           *debugger,
                                          IdeDebuggerBreakpoint *breakpoint,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *command = NULL;
  const gchar *id;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  id = ide_debugger_breakpoint_get_id (breakpoint);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_remove_breakpoint_async);
  ide_task_set_task_data (task, g_object_ref (breakpoint), g_object_unref);
  ide_task_set_return_on_cancel (task, TRUE);

  if (id == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Invalid breakpoint identifier");
      return;
    }

  command = g_strdup_printf ("-break-delete %s", id);

  gbp_gdb_debugger_exec_async (self,
                               command,
                               cancellable,
                               gbp_gdb_debugger_remove_breakpoint_cb,
                               g_steal_pointer (&task));
}

static gboolean
gbp_gdb_debugger_remove_breakpoint_finish (IdeDebugger   *debugger,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_gdb_debugger_list_register_names_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  struct gdbwire_mi_output *output;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    {
      ide_object_warning (self, "%s", error->message);
      goto cleanup;
    }

  gbp_gdb_debugger_cache_register_names (self, output);

cleanup:
  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_move_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  struct gdbwire_mi_output *output;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_move_async (IdeDebugger         *debugger,
                             IdeDebuggerMovement  movement,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  const char *command = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_MOVEMENT (movement));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_move_async);


  switch (movement)
    {
    case IDE_DEBUGGER_MOVEMENT_START: {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      g_autoptr(IdeSettings) settings = ide_context_ref_settings (context, "org.gnome.builder.debug");

      if (ide_settings_get_boolean (settings, "insert-breakpoint-at-main"))
        command = "-exec-run --all --start";
      else
        command = "-exec-run --all";

      break;
    }

    case IDE_DEBUGGER_MOVEMENT_CONTINUE:
      command = "-exec-continue";
      break;

    case IDE_DEBUGGER_MOVEMENT_STEP_IN:
      command = "-exec-step";
      break;

    case IDE_DEBUGGER_MOVEMENT_STEP_OVER:
      command = "-exec-next";
      break;

    case IDE_DEBUGGER_MOVEMENT_FINISH:
      command = "-exec-finish";
      break;

    default:
      g_return_if_reached ();
    }

  gbp_gdb_debugger_exec_async (self,
                               command,
                               cancellable,
                               gbp_gdb_debugger_move_cb,
                               g_steal_pointer (&task));

  /*
   * After we've started, we can cache accurate register names.
   * Doing this before we've started gives incorrect information.
   */
  if (self->register_names == NULL)
    gbp_gdb_debugger_exec_async (self,
                                 "-data-list-register-names",
                                 NULL,
                                 gbp_gdb_debugger_list_register_names_cb,
                                 NULL);
}

static gboolean
gbp_gdb_debugger_move_finish (IdeDebugger   *debugger,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_gdb_debugger_list_frames_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  struct gdbwire_mi_output *output;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      goto cleanup;
    }

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  if (output->kind == GDBWIRE_MI_OUTPUT_RESULT &&
      output->variant.result_record != NULL &&
      output->variant.result_record->result != NULL)
    {
      const struct gdbwire_mi_result *res = output->variant.result_record->result;

      if (res->kind == GDBWIRE_MI_LIST)
        {
          const struct gdbwire_mi_result *liter;

          for (liter = res->variant.result; liter != NULL; liter = liter->next)
            {
              if (liter->kind == GDBWIRE_MI_TUPLE)
                {
                  g_autoptr(IdeDebuggerFrame) frame = NULL;
                  g_autofree gchar *file = NULL;
                  g_autofree gchar *fullname = NULL;
                  G_GNUC_UNUSED const gchar *from = NULL;
                  const struct gdbwire_mi_result *iter;
                  const gchar *func = NULL;
                  IdeDebuggerAddress addr = 0;
                  guint level = 0;
                  guint line = 0;

                  for (iter = liter->variant.result; iter; iter = iter->next)
                    {
                      if (iter->kind == GDBWIRE_MI_CSTRING)
                        {
                          if (g_strcmp0 (iter->variable, "level") == 0)
                            level = g_ascii_strtoll (iter->variant.cstring, NULL, 10);
                          else if (g_strcmp0 (iter->variable, "addr") == 0)
                            addr = ide_debugger_address_parse (iter->variant.cstring);
                          else if (g_strcmp0 (iter->variable, "func") == 0)
                            func = iter->variant.cstring;
                          else if (g_strcmp0 (iter->variable, "file") == 0)
                            {
                              g_free (file);
                              file = gbp_gdb_debugger_translate_path (self, iter->variant.cstring);
                            }
                          else if (g_strcmp0 (iter->variable, "fullname") == 0)
                            {
                              g_free (fullname);
                              fullname = gbp_gdb_debugger_translate_path (self, iter->variant.cstring);
                            }
                          else if (g_strcmp0 (iter->variable, "line") == 0)
                            line = g_ascii_strtoll (iter->variant.cstring, NULL, 10);
                          else if (g_strcmp0 (iter->variable, "from") == 0)
                            from = iter->variant.cstring;
                        }
                    }

                  frame = ide_debugger_frame_new ();

                  ide_debugger_frame_set_address (frame, addr);
                  ide_debugger_frame_set_function (frame, func);
                  ide_debugger_frame_set_line (frame, line);
                  ide_debugger_frame_set_depth (frame, level);

                  if (fullname != NULL && g_file_test (fullname, G_FILE_TEST_EXISTS))
                    ide_debugger_frame_set_file (frame, fullname);
                  else
                    ide_debugger_frame_set_file (frame, file);

                  g_ptr_array_add (ar, g_steal_pointer (&frame));
                }
            }
        }
    }

  ide_task_return_pointer (task, g_steal_pointer (&ar), g_ptr_array_unref);

cleanup:
  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_list_frames_async (IdeDebugger         *debugger,
                                    IdeDebuggerThread   *thread,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *command = NULL;
  const gchar *tid = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_DEBUGGER_THREAD (thread));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_list_frames_async);

  /* TODO: We are expected to be stopped here, but we should also make sure
   *       the appropriate thread is selected first.
   */

  tid = ide_debugger_thread_get_id (thread);
  command = g_strdup_printf ("-stack-list-frames --thread %s", tid);

  gbp_gdb_debugger_exec_async (self,
                               command,
                               cancellable,
                               gbp_gdb_debugger_list_frames_cb,
                               g_steal_pointer (&task));
}

static GPtrArray *
gbp_gdb_debugger_list_frames_finish (IdeDebugger   *debugger,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
gbp_gdb_debugger_interrupt_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  struct gdbwire_mi_output *output;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_interrupt_async (IdeDebugger            *debugger,
                                  IdeDebuggerThreadGroup *thread_group,
                                  GCancellable           *cancellable,
                                  GAsyncReadyCallback     callback,
                                  gpointer                user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (!thread_group || IDE_IS_DEBUGGER_THREAD_GROUP (thread_group));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_interrupt_async);

  gbp_gdb_debugger_exec_async (self,
                               "-exec-interrupt --all",
                               cancellable,
                               gbp_gdb_debugger_interrupt_cb,
                               g_steal_pointer (&task));
}

static gboolean
gbp_gdb_debugger_interrupt_finish (IdeDebugger   *debugger,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_gdb_debugger_send_signal_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  struct gdbwire_mi_output *output;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  /* We expect a NULL output because this command cannot have a reply
   * that we can reliably wait for (not command id can be prefaced).
   */

  if (error != NULL || gbp_gdb_debugger_unwrap (output, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  g_clear_pointer (&output, gdbwire_mi_output_free);
}


static void
gbp_gdb_debugger_send_signal_async (IdeDebugger         *debugger,
                                    gint                 signum,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *command = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_send_signal_async);

  command = g_strdup_printf ("signal %d", signum);

  gbp_gdb_debugger_exec_async (self,
                               command,
                               cancellable,
                               gbp_gdb_debugger_send_signal_cb,
                               g_steal_pointer (&task));
}

static gboolean
gbp_gdb_debugger_send_signal_finish (IdeDebugger   *debugger,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_gdb_debugger_modify_breakpoint_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  struct gdbwire_mi_output *output;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  gbp_gdb_debugger_reload_breakpoints (self);

  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_modify_breakpoint_async (IdeDebugger                 *debugger,
                                          IdeDebuggerBreakpointChange  change,
                                          IdeDebuggerBreakpoint       *breakpoint,
                                          GCancellable                *cancellable,
                                          GAsyncReadyCallback          callback,
                                          gpointer                     user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *command = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT_CHANGE (change));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_gdb_debugger_modify_breakpoint_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  switch (change)
    {
    case IDE_DEBUGGER_BREAKPOINT_CHANGE_ENABLED:
      if (ide_debugger_breakpoint_get_enabled (breakpoint))
        command = g_strdup_printf ("-break-enable %s",
                                   ide_debugger_breakpoint_get_id (breakpoint));
      else
        command = g_strdup_printf ("-break-disable %s",
                                   ide_debugger_breakpoint_get_id (breakpoint));
      break;

    default:
      g_assert_not_reached ();
    }

  if (command == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Unsupported change requested");
      return;
    }

  gbp_gdb_debugger_exec_async (self,
                               command,
                               cancellable,
                               gbp_gdb_debugger_modify_breakpoint_cb,
                               g_steal_pointer (&task));
}

static gboolean
gbp_gdb_debugger_modify_breakpoint_finish (IdeDebugger   *debugger,
                                           GAsyncResult  *result,
                                           GError       **error)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_gdb_debugger_handle_list_variables (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data,
                                        gboolean      arguments)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(IdeTask) task = user_data;
  struct gdbwire_mi_output *output;
  struct gdbwire_mi_result *res;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      goto cleanup;
    }

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  /* Decode variables */

  res = output->variant.result_record->result;

  if (res->kind == GDBWIRE_MI_LIST &&
      g_strcmp0 (res->variable, "variables") == 0)
    {
      struct gdbwire_mi_result *iter;

      for (iter = res->variant.result; iter; iter = iter->next)
        {
          if (iter->kind == GDBWIRE_MI_TUPLE)
            {
              struct gdbwire_mi_result *titer;
              g_autoptr(IdeDebuggerVariable) var = NULL;
              G_GNUC_UNUSED const gchar *value = NULL;
              const gchar *type = NULL;
              const gchar *name = NULL;
              gboolean is_arg = FALSE;

              for (titer = iter->variant.result; titer; titer = titer->next)
                {
                  if (titer->kind == GDBWIRE_MI_CSTRING)
                    {
                      if (g_strcmp0 (titer->variable, "name") == 0)
                        name = titer->variant.cstring;
                      else if (g_strcmp0 (titer->variable, "type") == 0)
                        type = titer->variant.cstring;
                      else if (g_strcmp0 (titer->variable, "value") == 0)
                        value = titer->variant.cstring;
                      else if (g_strcmp0 (titer->variable, "arg") == 0)
                        is_arg |= g_strcmp0 (titer->variant.cstring, "1") == 0;
                    }
                }

              if ((name == NULL) || (arguments != is_arg))
                continue;

              var = ide_debugger_variable_new (name);
              ide_debugger_variable_set_type_name (var, type);
              ide_debugger_variable_set_value (var, value);

              /* TODO: We really need to create frozen variables for this
               *       so that we can get the updated value.
               */

              g_ptr_array_add (ar, g_steal_pointer (&var));
            }
        }
    }

  ide_task_return_pointer (task, g_steal_pointer (&ar), g_ptr_array_unref);

cleanup:
  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_list_locals_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  gbp_gdb_debugger_handle_list_variables (object, result, user_data, FALSE);
}

static void
gbp_gdb_debugger_list_locals_async (IdeDebugger         *debugger,
                                    IdeDebuggerThread   *thread,
                                    IdeDebuggerFrame    *frame,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *command = NULL;
  const gchar *tid = NULL;
  guint depth;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));
  g_assert (IDE_IS_DEBUGGER_FRAME (frame));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_list_locals_async);

  tid = ide_debugger_thread_get_id (thread);
  depth = ide_debugger_frame_get_depth (frame);
  command = g_strdup_printf ("-stack-list-variables --thread %s --frame %u --simple-values",
                             tid, depth);

  gbp_gdb_debugger_exec_async (self,
                               command,
                               cancellable,
                               gbp_gdb_debugger_list_locals_cb,
                               g_steal_pointer (&task));
}

static GPtrArray *
gbp_gdb_debugger_list_locals_finish (IdeDebugger   *debugger,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
gbp_gdb_debugger_list_params_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  gbp_gdb_debugger_handle_list_variables (object, result, user_data, TRUE);
}

static void
gbp_gdb_debugger_list_params_async (IdeDebugger         *debugger,
                                    IdeDebuggerThread   *thread,
                                    IdeDebuggerFrame    *frame,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *command = NULL;
  guint depth;
  const gchar *tid = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));
  g_assert (IDE_IS_DEBUGGER_FRAME (frame));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_list_params_async);

  tid = ide_debugger_thread_get_id (thread);
  depth = ide_debugger_frame_get_depth (frame);
  command = g_strdup_printf ("-stack-list-variables --thread %s --frame %u --simple-values",
                             tid, depth);

  gbp_gdb_debugger_exec_async (self,
                               command,
                               cancellable,
                               gbp_gdb_debugger_list_params_cb,
                               g_steal_pointer (&task));
}

static GPtrArray *
gbp_gdb_debugger_list_params_finish (IdeDebugger   *debugger,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
gbp_gdb_debugger_list_registers_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  struct gdbwire_mi_output *output;
  struct gdbwire_mi_result *res;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      goto cleanup;
    }

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  res = output->variant.result_record->result;

  if (res->kind == GDBWIRE_MI_LIST &&
      g_strcmp0 (res->variable, "register-values") == 0)
    {
      const struct gdbwire_mi_result *liter;

      for (liter = res->variant.result; liter; liter = liter->next)
        {
          if (liter->kind == GDBWIRE_MI_TUPLE)
            {
              g_autoptr(IdeDebuggerRegister) reg = NULL;
              const struct gdbwire_mi_result *titer;
              const gchar *name = NULL;
              const gchar *number = NULL;
              const gchar *value = NULL;

              for (titer = liter->variant.result; titer; titer = titer->next)
                {
                  if (titer->kind == GDBWIRE_MI_CSTRING)
                    {
                      if (g_strcmp0 (titer->variable, "number") == 0)
                        number = titer->variant.cstring;
                      else if (g_strcmp0 (titer->variable, "value") == 0)
                        value = titer->variant.cstring;
                    }
                }

              if (number != NULL && self->register_names != NULL)
                name = g_hash_table_lookup (self->register_names, number);

              reg = ide_debugger_register_new (number);
              ide_debugger_register_set_name (reg, name);
              ide_debugger_register_set_value (reg, value);

              g_ptr_array_add (ar, g_steal_pointer (&reg));
            }
        }
    }

  ide_task_return_pointer (task, g_steal_pointer (&ar), g_ptr_array_unref);

cleanup:
  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_list_registers_async (IdeDebugger         *debugger,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_list_registers_async);

  gbp_gdb_debugger_exec_async (self,
                               "-data-list-register-values x",
                               cancellable,
                               gbp_gdb_debugger_list_registers_cb,
                               g_steal_pointer (&task));
}

static GPtrArray *
gbp_gdb_debugger_list_registers_finish (IdeDebugger   *debugger,
                                        GAsyncResult  *result,
                                        GError       **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
gbp_gdb_debugger_disassemble_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  struct gdbwire_mi_output *output;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (result));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      goto cleanup;
    }

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  if (output->kind == GDBWIRE_MI_OUTPUT_RESULT &&
      output->variant.result_record != NULL &&
      output->variant.result_record->result_class == GDBWIRE_MI_DONE &&
      output->variant.result_record->result != NULL &&
      output->variant.result_record->result->kind == GDBWIRE_MI_LIST &&
      g_strcmp0 (output->variant.result_record->result->variable, "asm_insns") == 0)
    {
      const struct gdbwire_mi_result *res = output->variant.result_record->result;
      const struct gdbwire_mi_result *liter;

      for (liter = res->variant.result; liter; liter = liter->next)
        {
          if (liter->kind == GDBWIRE_MI_TUPLE)
            {
              g_autoptr(IdeDebuggerInstruction) inst = NULL;
              const struct gdbwire_mi_result *titer;
              IdeDebuggerAddress addr = 0;
              const gchar *func = NULL;
              const gchar *display = NULL;

              for (titer = liter->variant.result; titer; titer = titer->next)
                {
                  if (titer->kind == GDBWIRE_MI_CSTRING)
                    {
                      if (g_strcmp0 (titer->variable, "address") == 0)
                        addr = ide_debugger_address_parse (titer->variant.cstring);
                      else if (g_strcmp0 (titer->variable, "func-name") == 0)
                        func = titer->variant.cstring;
                      else if (g_strcmp0 (titer->variable, "inst") == 0)
                        display = titer->variant.cstring;
                    }
                }

              inst = ide_debugger_instruction_new (addr);
              ide_debugger_instruction_set_function (inst, func);
              ide_debugger_instruction_set_display (inst, display);

              g_ptr_array_add (ar, g_steal_pointer (&inst));
            }
        }
    }

  ide_task_return_pointer (task, g_steal_pointer (&ar), g_ptr_array_unref);

cleanup:
  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_disassemble_async (IdeDebugger                   *debugger,
                                    const IdeDebuggerAddressRange *range,
                                    GCancellable                  *cancellable,
                                    GAsyncReadyCallback            callback,
                                    gpointer                       user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *command = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (range != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_disassemble_async);

  command = g_strdup_printf ("-data-disassemble "
                             "-s 0x%"G_GINT64_MODIFIER"x "
                             "-e 0x%"G_GINT64_MODIFIER"x 0",
                             range->from, range->to);

  gbp_gdb_debugger_exec_async (self,
                               command,
                               cancellable,
                               gbp_gdb_debugger_disassemble_cb,
                               g_steal_pointer (&task));
}

static GPtrArray *
gbp_gdb_debugger_disassemble_finish (IdeDebugger   *debugger,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  GPtrArray *ret;

  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static gboolean
gbp_gdb_debugger_supports_run_command (IdeDebugger   *debugger,
                                       IdePipeline   *pipeline,
                                       IdeRunCommand *run_command,
                                       int           *priority)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_COMMAND (run_command));
  g_assert (priority != NULL);

  *priority = G_MAXINT;

  return TRUE;
}

static gboolean
gbp_gdb_debugger_run_context_handler_cb (IdeRunContext       *run_context,
                                         const char * const  *argv,
                                         const char * const  *env,
                                         const char          *cwd,
                                         IdeUnixFDMap        *unix_fd_map,
                                         gpointer             user_data,
                                         GError             **error)
{
  static const char * const allowed_shells[] = { "/bin/sh", "sh", "/bin/bash", "bash", NULL };
  GbpGdbDebugger *self = user_data;
  g_autoptr(GIOStream) io_stream = NULL;
  int pty_source_fd;
  int pty_dest_fd;

  IDE_ENTRY;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));

  /* Override $SHELL unless it's sh or bash as that tends to break things
   * like '$SHELL -c "exec $APP"' in gdb.
   */
  if (!g_strv_contains (allowed_shells, ide_get_user_shell ()))
    ide_run_context_setenv (run_context, "SHELL", "sh");

  /* Specify GDB with mi2 wire protocol */
  ide_run_context_append_argv (run_context, "gdb");
  ide_run_context_append_argv (run_context, "--interpreter=mi2");

  /* Set the CWD for the inferior but leave gdb untouched */
  if (cwd != NULL)
    {
      ide_run_context_append_argv (run_context, "--cd");
      ide_run_context_append_argv (run_context, cwd);
    }

  /* Steal the PTY for the inferior so we can assign it as another
   * file-descriptor and map it to the inferior from GDB.
   */
  pty_source_fd = ide_unix_fd_map_steal_stdout (unix_fd_map);
  g_warn_if_fail (pty_source_fd != -1);
  g_warn_if_fail (isatty (pty_source_fd));

  /* Save the PTY fd around to attach after spawning */
  pty_dest_fd = ide_unix_fd_map_get_max_dest_fd (unix_fd_map) + 1;
  ide_unix_fd_map_take (unix_fd_map, ide_steal_fd (&pty_source_fd), pty_dest_fd);

  /* Setup a stream to communicate with GDB over which is just a
   * regular pipe[2] for stdin/stdout and /dev/null for stderr.
   *
   * Otherwise, stderr may get merged and we wont be able to
   * parse the results from gdb.
   */
  if (!(io_stream = ide_unix_fd_map_create_stream (unix_fd_map, STDIN_FILENO, STDOUT_FILENO, error)) ||
      !ide_unix_fd_map_silence_fd (unix_fd_map, STDERR_FILENO, error))
    IDE_RETURN (FALSE);

  /* Make sure we don't have a PTY for our in/out stream */
  g_warn_if_fail (!isatty (g_unix_input_stream_get_fd (G_UNIX_INPUT_STREAM (g_io_stream_get_input_stream (io_stream)))));
  g_warn_if_fail (!isatty (g_unix_output_stream_get_fd (G_UNIX_OUTPUT_STREAM (g_io_stream_get_output_stream (io_stream)))));

  /* Now merge the FD map down a layer */
  if (!ide_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    IDE_RETURN (FALSE);

  /* Now that we have a PTY, we need to make sure our first command is
   * to set the PTY of the inferior to the FD of the PTY we attached.
   */
  ide_run_context_append_argv (run_context, "-ex");
  ide_run_context_append_formatted (run_context,
                                    "set inferior-tty /proc/self/fd/%d",
                                    pty_dest_fd);

  /* We don't want GDB to get the environment from this layer, so we specify
   * a wrapper script to set the environment for the inferior only. That
   * means that "show environment FOO" will not show anything, but the
   * inferior will see "FOO".
   */
  if (env[0] != NULL)
    {
      g_autoptr(GString) str = g_string_new ("set exec-wrapper env");

      for (guint i = 0; env[i]; i++)
        {
          g_autofree char *quoted = g_shell_quote (env[i]);

          g_string_append_c (str, ' ');
          g_string_append (str, quoted);
        }

      ide_run_context_append_argv (run_context, "-ex");
      ide_run_context_append_argv (run_context, str->str);
    }

  /* Now we can setup our command from the upper layer. Everything after
   * this must be part of the inferior's arguments.
   */
  ide_run_context_append_argv (run_context, "--args");
  ide_run_context_append_args (run_context, argv);

  /* Start communicating with gdb */
  gbp_gdb_debugger_connect (self, io_stream, NULL);
  ide_debugger_move_async (IDE_DEBUGGER (self),
                           IDE_DEBUGGER_MOVEMENT_START,
                           NULL, NULL, NULL);

  IDE_RETURN (TRUE);
}

static void
gbp_gdb_debugger_prepare_for_run (IdeDebugger   *debugger,
                                  IdePipeline   *pipeline,
                                  IdeRunContext *run_context)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;

  IDE_ENTRY;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  g_set_object (&self->current_config,
                ide_pipeline_get_config (pipeline));
  ide_run_context_push (run_context,
                        gbp_gdb_debugger_run_context_handler_cb,
                        g_object_ref (self),
                        g_object_unref);

  IDE_EXIT;
}

static void
gbp_gdb_debugger_interpret_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  struct gdbwire_mi_output *output;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  output = gbp_gdb_debugger_exec_finish (self, result, &error);

  if (output == NULL || gbp_gdb_debugger_unwrap (output, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  g_clear_pointer (&output, gdbwire_mi_output_free);
}

static void
gbp_gdb_debugger_interpret_async (IdeDebugger         *debugger,
                                  const gchar         *command,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *escaped = NULL;
  g_autofree gchar *command_str = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (command != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_gdb_debugger_interrupt_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  escaped = g_strescape (command, NULL);
  command_str = g_strdup_printf ("-interpreter-exec console \"%s\"", escaped);

  gbp_gdb_debugger_exec_async (self,
                               command_str,
                               cancellable,
                               gbp_gdb_debugger_interpret_cb,
                               g_steal_pointer (&task));

}

static gboolean
gbp_gdb_debugger_interpret_finish (IdeDebugger   *debugger,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_gdb_debugger_destroy (IdeObject *object)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;
  g_autoptr(GList) list = NULL;

  g_assert (GBP_IS_GDB_DEBUGGER (self));

  g_clear_object (&self->current_config);

  list = self->cmdqueue.head;

  self->cmdqueue.head = NULL;
  self->cmdqueue.tail = NULL;
  self->cmdqueue.length = 0;

  for (const GList *iter = list; iter != NULL; iter = iter->next)
    {
      g_autoptr(IdeTask) task = iter->data;

      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "The task was cancelled");
    }

  if (!g_cancellable_is_cancelled (self->read_cancellable))
    g_cancellable_cancel (self->read_cancellable);

  if (self->io_stream != NULL)
    {
      if (!g_io_stream_is_closed (self->io_stream))
        g_io_stream_close (self->io_stream, NULL, NULL);
    }

  g_queue_foreach (&self->writequeue, (GFunc)g_bytes_unref, NULL);
  g_queue_clear (&self->writequeue);

  IDE_OBJECT_CLASS (gbp_gdb_debugger_parent_class)->destroy (object);
}

static void
gbp_gdb_debugger_finalize (GObject *object)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;

  /* Ensure no tasks were queued after dispose call */
  g_assert (self->cmdqueue.length == 0);

  g_clear_object (&self->io_stream);
  g_clear_object (&self->read_cancellable);
  g_clear_pointer (&self->parser, gdbwire_mi_parser_destroy);
  g_clear_pointer (&self->read_buffer, g_free);
  g_clear_pointer (&self->register_names, g_hash_table_unref);
  g_queue_clear (&self->cmdqueue);

  G_OBJECT_CLASS (gbp_gdb_debugger_parent_class)->finalize (object);
}

static void
gbp_gdb_debugger_class_init (GbpGdbDebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *ide_object_class = IDE_OBJECT_CLASS (klass);
  IdeDebuggerClass *debugger_class = IDE_DEBUGGER_CLASS (klass);

  object_class->finalize = gbp_gdb_debugger_finalize;

  ide_object_class->destroy = gbp_gdb_debugger_destroy;
  ide_object_class->parent_set = gbp_gdb_debugger_parent_set;

  debugger_class->supports_run_command = gbp_gdb_debugger_supports_run_command;
  debugger_class->prepare_for_run = gbp_gdb_debugger_prepare_for_run;
  debugger_class->disassemble_async = gbp_gdb_debugger_disassemble_async;
  debugger_class->disassemble_finish = gbp_gdb_debugger_disassemble_finish;
  debugger_class->insert_breakpoint_async = gbp_gdb_debugger_insert_breakpoint_async;
  debugger_class->insert_breakpoint_finish = gbp_gdb_debugger_insert_breakpoint_finish;
  debugger_class->interrupt_async = gbp_gdb_debugger_interrupt_async;
  debugger_class->interrupt_finish = gbp_gdb_debugger_interrupt_finish;
  debugger_class->list_breakpoints_async = gbp_gdb_debugger_list_breakpoints_async;
  debugger_class->list_breakpoints_finish = gbp_gdb_debugger_list_breakpoints_finish;
  debugger_class->list_frames_async = gbp_gdb_debugger_list_frames_async;
  debugger_class->list_frames_finish = gbp_gdb_debugger_list_frames_finish;
  debugger_class->list_locals_async = gbp_gdb_debugger_list_locals_async;
  debugger_class->list_locals_finish = gbp_gdb_debugger_list_locals_finish;
  debugger_class->list_params_async = gbp_gdb_debugger_list_params_async;
  debugger_class->list_params_finish = gbp_gdb_debugger_list_params_finish;
  debugger_class->list_registers_async = gbp_gdb_debugger_list_registers_async;
  debugger_class->list_registers_finish = gbp_gdb_debugger_list_registers_finish;
  debugger_class->modify_breakpoint_async = gbp_gdb_debugger_modify_breakpoint_async;
  debugger_class->modify_breakpoint_finish = gbp_gdb_debugger_modify_breakpoint_finish;
  debugger_class->move_async = gbp_gdb_debugger_move_async;
  debugger_class->move_finish = gbp_gdb_debugger_move_finish;
  debugger_class->remove_breakpoint_async = gbp_gdb_debugger_remove_breakpoint_async;
  debugger_class->remove_breakpoint_finish = gbp_gdb_debugger_remove_breakpoint_finish;
  debugger_class->send_signal_async = gbp_gdb_debugger_send_signal_async;
  debugger_class->send_signal_finish = gbp_gdb_debugger_send_signal_finish;
  debugger_class->interpret_async = gbp_gdb_debugger_interpret_async;
  debugger_class->interpret_finish = gbp_gdb_debugger_interpret_finish;
}

static void
gbp_gdb_debugger_init (GbpGdbDebugger *self)
{
  struct gdbwire_mi_parser_callbacks callbacks = {
    self, gbp_gdb_debugger_output_callback
  };

  self->parser = gdbwire_mi_parser_create (callbacks);
  self->read_cancellable = g_cancellable_new ();
  self->read_buffer = g_malloc (READ_BUFFER_LEN);

  g_queue_init (&self->cmdqueue);
}

GbpGdbDebugger *
gbp_gdb_debugger_new (void)
{
  return g_object_new (GBP_TYPE_GDB_DEBUGGER, NULL);
}

static void
gbp_gdb_debugger_read_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr(GbpGdbDebugger) self = user_data;
  GInputStream *stream = (GInputStream *)object;
  g_autoptr(GError) error = NULL;
  enum gdbwire_result res;
  GCancellable *read_cancellable;
  gchar *read_buffer;
  gssize n_read;

  g_assert (G_IS_INPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_GDB_DEBUGGER (self));

  n_read = g_input_stream_read_finish (stream, result, &error);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CLOSED))
        ide_object_warning (self, "gdb client read failed: %s", error->message);
      return;
    }

  if (n_read <= 0)
    {
      g_message ("empty read from peer, possibly closed?");
      return;
    }

#if 0
  self->read_buffer[n_read] = 0;
  g_printerr (")))%s", self->read_buffer);
#endif

  res = gdbwire_mi_parser_push_data (self->parser, self->read_buffer, n_read);

  if (res != GDBWIRE_OK)
    {
      ide_object_warning (self, "Failed to push data into gdbwire parser: %d", res);
      return;
    }

  /* We can't access these inline because we need to steal *
   * the self pointer for proper ownership management.     */
  read_buffer = self->read_buffer;
  read_cancellable = self->read_cancellable;

  g_input_stream_read_async (stream,
                             read_buffer,
                             READ_BUFFER_LEN,
                             G_PRIORITY_LOW,
                             read_cancellable,
                             gbp_gdb_debugger_read_cb,
                             g_steal_pointer (&self));
}

void
gbp_gdb_debugger_connect (GbpGdbDebugger *self,
                          GIOStream      *io_stream,
                          GCancellable   *cancellable)
{
  GInputStream *stream;

  g_return_if_fail (GBP_IS_GDB_DEBUGGER (self));
  g_return_if_fail (self->has_connected == FALSE);
  g_return_if_fail (G_IS_IO_STREAM (io_stream));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (self->io_stream == NULL);

  self->has_connected = TRUE;

  g_set_object (&self->io_stream, io_stream);

  stream = g_io_stream_get_input_stream (self->io_stream);

  g_return_if_fail (stream != NULL);
  g_return_if_fail (G_IS_INPUT_STREAM (stream));

  g_input_stream_read_async (stream,
                             self->read_buffer,
                             READ_BUFFER_LEN,
                             G_PRIORITY_LOW,
                             self->read_cancellable,
                             gbp_gdb_debugger_read_cb,
                             g_object_ref (self));

  gbp_gdb_debugger_exec_async (self, "-gdb-set mi-async on", NULL, NULL, NULL);
  gbp_gdb_debugger_reload_breakpoints (self);
}

static gboolean
gbp_gdb_debugger_check_ready (GbpGdbDebugger  *self,
                              GError         **error)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));

  if (self->io_stream == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "The connection to gdb has not been set");
      return FALSE;
    }

  if (g_io_stream_is_closed (self->io_stream))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CLOSED,
                   "The connection is closed");
      return FALSE;
    }

  return TRUE;
}

static void
gbp_gdb_debugger_write_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GOutputStream *stream = (GOutputStream *)object;
  g_autoptr(GbpGdbDebugger) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;

  g_assert (G_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));

  g_output_stream_write_bytes_finish (stream, result, &error);

  if (error != NULL)
    {
      ide_object_warning (self, "%s", error->message);
      gbp_gdb_debugger_panic (self);
      return;
    }

  bytes = g_queue_pop_head (&self->writequeue);

  if (bytes != NULL)
    {
      GCancellable *cancellable = self->read_cancellable;

      g_output_stream_write_bytes_async (stream,
                                         bytes,
                                         G_PRIORITY_LOW,
                                         cancellable,
                                         gbp_gdb_debugger_write_cb,
                                         g_steal_pointer (&self));
    }
}

/**
 * gbp_gdb_debugger_exec_async:
 * @self: An #GbpGdbDebugger
 * @command: the command to be executed
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @user_data: user data for @cancellable
 *
 * Submits a command to the gdb process to be executed by the debugger.
 *
 * For this function to succeed, you must have already called
 * gbp_gdb_debugger_connect().
 *
 * This asynchronous function will complete when we have received a response
 * from the debugger with the result, or the connection has closed. Whichever
 * is first.
 */
void
gbp_gdb_debugger_exec_async (GbpGdbDebugger      *self,
                             const gchar         *command,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;
  GOutputStream *stream;
  GString *str;
  guint id;

  g_return_if_fail (GBP_IS_GDB_DEBUGGER (self));
  g_return_if_fail (command != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* Wrap at 10,000 */
  id = ++self->cmdseq;
  if (id == 10000)
    id = self->cmdseq = 1;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_gdb_debugger_exec_async);
  ide_task_set_task_data (task, g_strdup_printf ("%03u", id), g_free);

  if (!gbp_gdb_debugger_check_ready (self, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_assert (self->io_stream != NULL);
  g_assert (!g_io_stream_is_closed (self->io_stream));

  stream = g_io_stream_get_output_stream (self->io_stream);

  str = g_string_new (NULL);

  if (command[0] == '-' || strstr (command, "@@@@") != NULL)
    {
      const gchar *at = strstr (command, "@@@@");

      if (at != NULL)
        {
          g_string_append_len (str, command, at - command);
          g_string_append_printf (str, "%03u", id);
          g_string_append_printf (str, "%s", at + 4);
          if (str->str[str->len - 1] != '\n')
            g_string_append_c (str, '\n');
        }
      else
        g_string_append_printf (str, "%03u%s\n", id, command);

      /*
       * Stash the task to be completed when we have received the result
       * on the GInputStream and decoded via gdbwire.
       */
      g_queue_push_tail (&self->cmdqueue, g_object_ref (task));
    }
  else
    {
      /* This command is not one that we can get a reply for because it does
       * not start with "-". So we will just send the result immediately and
       * synthesize a NULL response.
       */
      g_string_append_printf (str, "%s\n", command);
      (ide_task_return_pointer) (task, NULL, NULL);
    }

  DEBUG_LOG ("to-gdb", str->str);

  bytes = g_string_free_to_bytes (str);
  g_object_set_data_full (G_OBJECT (task), "REQUEST_BYTES",
                          g_bytes_ref (bytes), (GDestroyNotify)g_bytes_unref);

  if (g_output_stream_has_pending (stream) || self->writequeue.length > 0)
    g_queue_push_tail (&self->writequeue, g_steal_pointer (&bytes));
  else
    g_output_stream_write_bytes_async (stream,
                                       bytes,
                                       G_PRIORITY_LOW,
                                       self->read_cancellable,
                                       gbp_gdb_debugger_write_cb,
                                       g_object_ref (self));
}

/**
 * gbp_gdb_debugger_exec_finish:
 * @self: a #GbpGdbDebugger
 * @result: A result provided to the async callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous operation to gbp_gdb_debugger_exec_async().
 *
 * Returns: a gdbwire_mi_output which should be freed with
 *   gdbwire_mi_output_free() when no longer in use.
 */
struct gdbwire_mi_output *
gbp_gdb_debugger_exec_finish (GbpGdbDebugger  *self,
                              GAsyncResult    *result,
                              GError         **error)
{
  struct gdbwire_mi_output *ret;

  g_return_val_if_fail (GBP_IS_GDB_DEBUGGER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return g_steal_pointer (&ret);
}
