/* ide-gca-diagnostic-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-gca-diagnostic-provider"

#include <gca-diagnostics.h>
#include <glib/gi18n.h>

#include "ide-internal.h"

#include "ide-gca-diagnostic-provider.h"
#include "ide-gca-service.h"

#include "gca-structs.h"

struct _IdeGcaDiagnosticProvider
{
  IdeObject   parent_instance;
  GHashTable *document_cache;
};

typedef struct
{
  GTask          *task; /* Integrity check backpointer */
  IdeUnsavedFile *unsaved_file;
  IdeFile        *file;
  gchar          *language_id;
} DiagnoseState;

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGcaDiagnosticProvider, ide_gca_diagnostic_provider, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                               diagnostic_provider_iface_init))

static GSettings *gca_settings;

static void
diagnose_state_free (gpointer data)
{
  DiagnoseState *state = data;

  if (state)
    {
      g_clear_object (&state->file);
      g_free (state->language_id);
      g_clear_pointer (&state->unsaved_file, ide_unsaved_file_unref);
      g_slice_free (DiagnoseState, state);
    }
}

static IdeDiagnosticSeverity
get_severity (guint val)
{
  switch (val)
    {
    case GCA_SEVERITY_INFO:
      return IDE_DIAGNOSTIC_NOTE;

    case GCA_SEVERITY_WARNING:
      return IDE_DIAGNOSTIC_WARNING;

    case GCA_SEVERITY_DEPRECATED:
      return IDE_DIAGNOSTIC_DEPRECATED;

    case GCA_SEVERITY_ERROR:
      return IDE_DIAGNOSTIC_ERROR;

    case GCA_SEVERITY_FATAL:
      return IDE_DIAGNOSTIC_FATAL;

    case GCA_SEVERITY_NONE:
    default:
      return IDE_DIAGNOSTIC_IGNORED;
      break;
    }
}

static IdeDiagnostics *
variant_to_diagnostics (DiagnoseState *state,
                        GVariant *variant)
{

  GPtrArray *ar;
  GVariantIter iter;
  GVariantIter *b;
  GVariantIter *c;
  gchar *d = NULL;
  guint a;

  IDE_PROBE;

  g_assert (variant);

  ar = g_ptr_array_new ();
  g_ptr_array_set_free_func (ar, (GDestroyNotify)ide_diagnostic_unref);

  g_variant_iter_init (&iter, variant);

  while (g_variant_iter_loop (&iter, "(ua((x(xx)(xx))s)a(x(xx)(xx))s)",
                              &a, &b, &c, &d))
    {
      IdeDiagnosticSeverity severity;
      IdeDiagnostic *diag;
      gint64 x1, x2, x3, x4, x5;
      gchar *e;

      severity = get_severity (a);

      while (g_variant_iter_next (b, "((x(xx)(xx))s)",
                                  &x1, &x2, &x3, &x4, &x5, &e))
        {
          /*
           * TODO: Add fixits back after we plumb them into IdeDiagnostic.
           */
#if 0
          GcaFixit fixit = {{ 0 }};

          fixit.range.file = x1;
          fixit.range.begin.line = x2 - 1;
          fixit.range.begin.column = x3 - 1;
          fixit.range.end.line = x4 - 1;
          fixit.range.end.column = x5 - 1;
          fixit.value = g_strdup (e);

          g_array_append_val (diag.fixits, fixit);
#endif
        }

      diag = ide_diagnostic_new (severity, d, NULL);

      while (g_variant_iter_next (c, "(x(xx)(xx))", &x1, &x2, &x3, &x4, &x5))
        {
          IdeSourceRange *range;
          IdeSourceLocation *begin;
          IdeSourceLocation *end;
          IdeFile *file = NULL;

          /*
           * FIXME:
           *
           * Not always true, but we can cheat for now and claim it is within
           * the file we just parsed.
           */
          file = state->file;

          begin = ide_source_location_new (file, x2 - 1, x3 - 1, 0);
          end = ide_source_location_new (file, x4 - 1, x5 - 1, 0);

          range = ide_source_range_new (begin, end);
          ide_diagnostic_take_range (diag, range);

          ide_source_location_unref (begin);
          ide_source_location_unref (end);
        }

      g_ptr_array_add (ar, diag);
    }

  return ide_diagnostics_new (ar);
}

static void
diagnostics_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  GcaDiagnostics *proxy = (GcaDiagnostics *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) var = NULL;
  GError *error = NULL;
  IdeDiagnostics *diagnostics;
  DiagnoseState *state;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!gca_diagnostics_call_diagnostics_finish (proxy, &var, result, &error))
    {
      IDE_TRACE_MSG ("%s", error->message);
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  state = g_task_get_task_data (task);
  g_assert (state->task == task);

  diagnostics = variant_to_diagnostics (state, var);

  g_task_return_pointer (task, diagnostics,
                         (GDestroyNotify)ide_diagnostics_unref);

  IDE_EXIT;
}

static void
get_diag_proxy_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  IdeGcaDiagnosticProvider *self;
  GcaDiagnostics *proxy;
  GError *error = NULL;
  const gchar *path;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));

  self = g_task_get_source_object (task);

  proxy = gca_diagnostics_proxy_new_finish (result, &error);

  if (!proxy)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (proxy));
  g_hash_table_replace (self->document_cache, g_strdup (path), proxy);

  gca_diagnostics_call_diagnostics (proxy,
                                    g_task_get_cancellable (task),
                                    diagnostics_cb,
                                    g_object_ref (task));

  IDE_EXIT;
}

static void
parse_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  GcaService *proxy = (GcaService *)object;
  IdeGcaDiagnosticProvider *self;
  DiagnoseState *state;
  g_autoptr(GTask) task = user_data;
  GcaDiagnostics *doc_proxy;
  gboolean ret;
  GError *error = NULL;
  g_autofree gchar *document_path = NULL;

  IDE_ENTRY;

  g_assert (GCA_IS_SERVICE (proxy));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  state = g_task_get_task_data (task);

  ret = gca_service_call_parse_finish (proxy, &document_path, result, &error);

  if (!ret)
    {
      if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
        {
          g_task_return_pointer (task, ide_diagnostics_new (NULL),
                                 (GDestroyNotify)ide_diagnostics_unref);
        }
      else
        {
          IDE_TRACE_MSG ("%s", error->message);
          g_task_return_error (task, error);
        }

      IDE_EXIT;
    }

  doc_proxy = g_hash_table_lookup (self->document_cache, document_path);

  if (!doc_proxy)
    {
      g_autofree gchar *well_known_name = NULL;
      GDBusConnection *conn;

      well_known_name = g_strdup_printf ("org.gnome.CodeAssist.v1.%s",
                                         state->language_id);
      conn = g_dbus_proxy_get_connection (G_DBUS_PROXY (proxy));

      gca_diagnostics_proxy_new (conn,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 well_known_name,
                                 document_path,
                                 g_task_get_cancellable (task),
                                 get_diag_proxy_cb,
                                 g_object_ref (task));
      IDE_EXIT;
    }

  gca_diagnostics_call_diagnostics (doc_proxy,
                                    g_task_get_cancellable (task),
                                    diagnostics_cb,
                                    g_object_ref (task));

  IDE_EXIT;
}

static GVariant *
get_parse_options (void)
{
  if (G_UNLIKELY (gca_settings == NULL))
    gca_settings = g_settings_new ("org.gnome.builder.gnome-code-assistance");

  if (g_settings_get_boolean (gca_settings, "enable-pylint"))
    {
      GVariantBuilder builder;

      g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
      g_variant_builder_add (&builder, "{sv}", "pylint", g_variant_new_boolean (TRUE));
      return g_variant_builder_end (&builder);
    }

  return g_variant_new ("a{sv}", 0);
}

static void
get_proxy_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(GVariant) options = NULL;
  IdeGcaService *service = (IdeGcaService *)object;
  DiagnoseState *state;
  GcaService *proxy;
  const gchar *temp_path;
  GError *error = NULL;
  GFile *gfile;
  g_autofree gchar *path = NULL;
  GVariant *cursor = NULL;

  IDE_ENTRY;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_GCA_SERVICE (service));

  state = g_task_get_task_data (task);
  g_assert (state->task == task);

  proxy = ide_gca_service_get_proxy_finish (service, result, &error);

  if (!proxy)
    {
      g_task_return_error (task, error);
      IDE_GOTO (cleanup);
    }

  gfile = ide_file_get_file (state->file);
  temp_path = path = g_file_get_path (gfile);

  if (!path)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Code assistance requires a local file."));
      IDE_GOTO (cleanup);
    }

  if (state->unsaved_file)
    {
      if (!ide_unsaved_file_persist (state->unsaved_file,
                                     g_task_get_cancellable (task),
                                     &error))
        {
          g_task_return_error (task, error);
          IDE_GOTO (cleanup);
        }

      temp_path = ide_unsaved_file_get_temp_path (state->unsaved_file);
    }

  /* TODO: Plumb support for cursors down to this level? */
  cursor = g_variant_new ("(xx)", (gint64)0, (gint64)0);
  options = g_variant_ref_sink (get_parse_options ());

  gca_service_call_parse (proxy,
                          path,
                          temp_path,
                          cursor,
                          options,
                          g_task_get_cancellable (task),
                          parse_cb,
                          g_object_ref (task));

cleanup:
  g_clear_object (&proxy);

  IDE_EXIT;
}

static void
ide_gca_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                            IdeFile               *file,
                                            IdeBuffer             *buffer,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data)
{
  IdeGcaDiagnosticProvider *self = (IdeGcaDiagnosticProvider *)provider;
  g_autoptr(GTask) task = NULL;
  IdeGcaService *service;
  DiagnoseState *state;
  GtkSourceLanguage *language;
  IdeContext *context;
  IdeUnsavedFiles *files;
  const gchar *language_id = NULL;
  GFile *gfile;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GCA_DIAGNOSTIC_PROVIDER (self));

  task = g_task_new (self, cancellable, callback, user_data);

  language = ide_file_get_language (file);

  if (language != NULL)
    language_id = gtk_source_language_get_id (language);

  if (language_id == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "No language specified, code assistance not supported.");
      IDE_EXIT;
    }

  context = ide_object_get_context (IDE_OBJECT (provider));
  service = ide_context_get_service_typed (context, IDE_TYPE_GCA_SERVICE);
  files = ide_context_get_unsaved_files (context);
  gfile = ide_file_get_file (file);

  state = g_slice_new0 (DiagnoseState);
  state->task = task;
  state->language_id = g_strdup (language_id);
  state->file = g_object_ref (file);
  state->unsaved_file = ide_unsaved_files_get_unsaved_file (files, gfile);

  g_task_set_task_data (task, state, diagnose_state_free);

  ide_gca_service_get_proxy_async (service, language_id, cancellable,
                                   get_proxy_cb, g_object_ref (task));

  IDE_EXIT;
}

static IdeDiagnostics *
ide_gca_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *self,
                                             GAsyncResult           *result,
                                             GError                **error)
{
  GTask *task = (GTask *)result;
  IdeDiagnostics *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_GCA_DIAGNOSTIC_PROVIDER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  ret = g_task_propagate_pointer (task, error);

  IDE_RETURN (ret);
}

static void
ide_gca_diagnostic_provider_finalize (GObject *object)
{
  IdeGcaDiagnosticProvider *self = (IdeGcaDiagnosticProvider *)object;

  g_clear_pointer (&self->document_cache, g_hash_table_unref);

  G_OBJECT_CLASS (ide_gca_diagnostic_provider_parent_class)->finalize (object);
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_gca_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_gca_diagnostic_provider_diagnose_finish;
}

static void
ide_gca_diagnostic_provider_class_init (IdeGcaDiagnosticProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gca_diagnostic_provider_finalize;
}

static void
ide_gca_diagnostic_provider_init (IdeGcaDiagnosticProvider *self)
{
  self->document_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                g_free, g_object_unref);
}
