/* rust-analyzer-service.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "rust-analyzer-service"

#include "config.h"

#include <jsonrpc-glib.h>

#include "rust-analyzer-pipeline-addin.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerService
{
  GObject                  parent_instance;
  IdeWorkbench            *workbench;
  IdeLspClient            *client;
  IdeSubprocessSupervisor *supervisor;
  IdeSignalGroup          *pipeline_signals;
  GSettings               *settings;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (RustAnalyzerService, rust_analyzer_service, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
rust_analyzer_service_pipeline_loaded_cb (RustAnalyzerService *self,
                                          IdePipeline         *pipeline)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  IdePipelineAddin *addin;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));

  IDE_TRACE_MSG ("Pipeline loaded, attempting to locate rust-analyzer");

  ide_subprocess_supervisor_set_launcher (self->supervisor, NULL);
  ide_subprocess_supervisor_stop (self->supervisor);

  if (!(addin = ide_pipeline_addin_find_by_module_name (pipeline, "rust-analyzer")) ||
      !(launcher = rust_analyzer_pipeline_addin_create_launcher (RUST_ANALYZER_PIPELINE_ADDIN (addin))))
    IDE_EXIT;

  ide_subprocess_supervisor_set_launcher (self->supervisor, launcher);
  ide_subprocess_supervisor_start (self->supervisor);

  IDE_EXIT;
}

static void
rust_analyzer_service_bind_pipeline (RustAnalyzerService *self,
                                     IdePipeline         *pipeline,
                                     IdeSignalGroup      *signal_group)
{
  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_SIGNAL_GROUP (signal_group));

  if (ide_pipeline_is_ready (pipeline))
    rust_analyzer_service_pipeline_loaded_cb (self, pipeline);

  IDE_EXIT;
}

static void
rust_analyzer_service_lsp_initialized_cb (RustAnalyzerService *self,
                                          IdeLspClient        *client)
{
  g_autoptr(GVariant) params = NULL;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_LSP_CLIENT (client));

  params = JSONRPC_MESSAGE_NEW ("settings", "");

  ide_lsp_client_send_notification_async (client,
                                          "workspace/didChangeConfiguration",
                                          params,
                                          NULL, NULL, NULL);

  IDE_EXIT;
}

static GVariant *
rust_analyzer_service_lsp_load_configuration_cb (RustAnalyzerService *self,
                                                 IdeLspClient        *client)
{
  g_autoptr(GVariant) ret = NULL;
  g_autofree gchar *command = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (RUST_IS_ANALYZER_SERVICE (self));

  command = g_settings_get_string (self->settings, "cargo-command");

  ret = JSONRPC_MESSAGE_NEW_ARRAY ("{",
                                     "checkOnSave", "{",
                                       "enable", JSONRPC_MESSAGE_PUT_BOOLEAN (command[0] != 0),
                                       "command", JSONRPC_MESSAGE_PUT_STRING (command),
                                     "}",
                                   "}");

  IDE_RETURN (g_steal_pointer (&ret));
}

static void
rust_analyzer_service_supervisor_spawned_cb (RustAnalyzerService     *self,
                                             IdeSubprocess           *subprocess,
                                             IdeSubprocessSupervisor *supervisor)
{
  g_autoptr(GIOStream) io_stream = NULL;
  IdeSubprocessLauncher *launcher;
  GOutputStream *output;
  GInputStream *input;
  const gchar *workdir;
  IdeContext *context;
  g_autoptr(GVariant) params = NULL;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));

  input = ide_subprocess_get_stdout_pipe (subprocess);
  output = ide_subprocess_get_stdin_pipe (subprocess);
  io_stream = g_simple_io_stream_new (input, output);

  if (self->client != NULL)
    {
      ide_lsp_client_stop (self->client);
      ide_object_destroy (IDE_OBJECT (self->client));
      g_clear_object (&self->client);
    }

  self->client = ide_lsp_client_new (io_stream);

  /* Opt-in for experimental proc-macro feature to make gtk-rs more
   * useful for GNOME developers.
   *
   * See: https://rust-analyzer.github.io/manual.html#configuration
   */
  params = JSONRPC_MESSAGE_NEW (
    "procMacro", "{",
      "enable", JSONRPC_MESSAGE_PUT_BOOLEAN(TRUE),
    "}"
  );

  ide_lsp_client_set_initialization_options (self->client, params);

  g_object_set (self->client,
                "use-markdown-in-diagnostics", TRUE,
                NULL);

  g_signal_connect_object (self->client,
                           "load-configuration",
                           G_CALLBACK (rust_analyzer_service_lsp_load_configuration_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->client,
                           "initialized",
                           G_CALLBACK (rust_analyzer_service_lsp_initialized_cb),
                           self,
                           G_CONNECT_SWAPPED);

  if ((launcher = ide_subprocess_supervisor_get_launcher (supervisor)) &&
      (workdir = ide_subprocess_launcher_get_cwd (launcher)))
    {
      g_autoptr(GFile) file = g_file_new_for_path (workdir);
      g_autofree gchar *root_uri = g_file_get_uri (file);

      ide_lsp_client_set_root_uri (self->client, root_uri);
    }

  context = ide_workbench_get_context (self->workbench);
  ide_lsp_client_add_language (self->client, "rust");
  ide_object_append (IDE_OBJECT (context), IDE_OBJECT (self->client));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);

  ide_lsp_client_start (self->client);

  IDE_EXIT;
}

static void
rust_analyzer_service_settings_changed_cb (RustAnalyzerService *self,
                                           const gchar         *key,
                                           GSettings           *settings)
{
  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (G_IS_SETTINGS (settings));

  if (self->client != NULL)
    {
      g_autoptr(GVariant) params = JSONRPC_MESSAGE_NEW ("settings", "");
      ide_lsp_client_send_notification_async (self->client,
                                              "workspace/didChangeConfiguration",
                                              params,
                                              NULL, NULL, NULL);
    }

  IDE_EXIT;
}

static void
rust_analyzer_service_finalize (GObject *object)
{
  RustAnalyzerService *self = (RustAnalyzerService *)object;

  IDE_ENTRY;

  g_clear_object (&self->supervisor);
  g_clear_object (&self->pipeline_signals);
  g_clear_object (&self->client);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (rust_analyzer_service_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
rust_analyzer_service_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  RustAnalyzerService *self = RUST_ANALYZER_SERVICE (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, rust_analyzer_service_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rust_analyzer_service_class_init (RustAnalyzerServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = rust_analyzer_service_finalize;
  object_class->get_property = rust_analyzer_service_get_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The language server protocol client",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
rust_analyzer_service_init (RustAnalyzerService *self)
{
  self->settings = g_settings_new ("org.gnome.builder.rust-analyzer");
  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (rust_analyzer_service_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->supervisor = ide_subprocess_supervisor_new ();
  g_signal_connect_object (self->supervisor,
                           "spawned",
                           G_CALLBACK (rust_analyzer_service_supervisor_spawned_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->pipeline_signals = ide_signal_group_new (IDE_TYPE_PIPELINE);
  ide_signal_group_connect_object (self->pipeline_signals,
                                   "loaded",
                                   G_CALLBACK (rust_analyzer_service_pipeline_loaded_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_connect_object (self->pipeline_signals,
                           "bind",
                           G_CALLBACK (rust_analyzer_service_bind_pipeline),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
rust_analyzer_service_load (IdeWorkbenchAddin *addin,
                            IdeWorkbench      *workbench)
{
  RustAnalyzerService *self = (RustAnalyzerService *)addin;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  IDE_EXIT;
}

static void
rust_analyzer_service_unload (IdeWorkbenchAddin *addin,
                              IdeWorkbench      *workbench)
{
  RustAnalyzerService *self = (RustAnalyzerService *)addin;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;

  ide_signal_group_set_target (self->pipeline_signals, NULL);

  if (self->client != NULL)
    {
      g_autoptr(IdeLspClient) client = g_steal_pointer (&self->client);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
      ide_lsp_client_stop (client);
      ide_object_destroy (IDE_OBJECT (client));
    }

  if (self->supervisor != NULL)
    {
      ide_subprocess_supervisor_stop (self->supervisor);
      g_clear_object (&self->supervisor);
    }

  IDE_EXIT;
}

static void
rust_analyzer_service_notify_pipeline_cb (RustAnalyzerService *self,
                                          GParamSpec          *pspec,
                                          IdeBuildManager     *build_manager)
{
  IdePipeline *pipeline;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  pipeline = ide_build_manager_get_pipeline (build_manager);
  ide_signal_group_set_target (self->pipeline_signals, pipeline);

  IDE_EXIT;
}

static void
rust_analyzer_service_project_loaded (IdeWorkbenchAddin *addin,
                                      IdeProjectInfo    *project_info)
{
  RustAnalyzerService *self = (RustAnalyzerService *)addin;
  IdeBuildManager *build_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  /* We only start things if we have a project loaded or else there isn't
   * a whole lot we can do safely as too many subsystems will be in play
   * which should only be loaded when a project is active.
   */

  context = ide_workbench_get_context (self->workbench);
  build_manager = ide_build_manager_from_context (context);
  g_signal_connect_object (build_manager,
                           "notify::pipeline",
                           G_CALLBACK (rust_analyzer_service_notify_pipeline_cb),
                           self,
                           G_CONNECT_SWAPPED);
  rust_analyzer_service_notify_pipeline_cb (self, NULL, build_manager);

  IDE_EXIT;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = rust_analyzer_service_load;
  iface->unload = rust_analyzer_service_unload;
  iface->project_loaded = rust_analyzer_service_project_loaded;
}

RustAnalyzerService *
rust_analyzer_service_from_context (IdeContext *context)
{
  IdeWorkbenchAddin *addin;
  IdeWorkbench *workbench;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  workbench = ide_workbench_from_context (context);
  addin = ide_workbench_addin_find_by_module_name (workbench, "rust-analyzer");

  return RUST_ANALYZER_SERVICE (addin);
}

IdeLspClient *
rust_analyzer_service_get_client (RustAnalyzerService *self)
{
  g_return_val_if_fail (RUST_IS_ANALYZER_SERVICE (self), NULL);

  return self->client;
}

void
rust_analyzer_service_ensure_started (RustAnalyzerService *self)
{
  IdeSubprocessLauncher *launcher;
  IdePipeline *pipeline;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (RUST_IS_ANALYZER_SERVICE (self));
  g_return_if_fail (self->workbench != NULL);

  /* Ignore unless a project is loaded. Without a project loaded we
   * dont have access to foundry subsystem.
   */
  context = ide_workbench_get_context (self->workbench);
  if (!ide_context_has_project (context))
    IDE_EXIT;

  /* Do nothing if the supervisor already has a launcher */
  if ((launcher = ide_subprocess_supervisor_get_launcher (self->supervisor)))
    IDE_EXIT;

  /* Try again (maybe new files opened) to see if we can get launcher
   * using a discovered Cargo.toml.
   */
  if (!(pipeline = ide_signal_group_get_target (self->pipeline_signals)) ||
      !ide_pipeline_is_ready (pipeline))
    IDE_EXIT;

  rust_analyzer_service_pipeline_loaded_cb (self, pipeline);

  IDE_EXIT;
}
