/* rust-analyzer-service.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#include "rust-analyzer-service.h"
#include "rust-analyzer-transfer.h"
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>
#include <libide-core.h>
#include <jsonrpc-glib.h>
#include <glib/gi18n.h>
#include <libide-search.h>
#include <libide-io.h>
#include <libide-editor.h>
#include <libide-gui.h>
#include <ide-gui-private.h>
#include "rust-analyzer-search-provider.h"

struct _RustAnalyzerService
{
  IdeObject parent_instance;
  IdeLspClient  *client;
  IdeSubprocessSupervisor *supervisor;
  GFileMonitor *cargo_monitor;
  RustAnalyzerSearchProvider *search_provider;
  GSettings *settings;
  gchar *cargo_command;

  ServiceState state;
};

G_DEFINE_TYPE (RustAnalyzerService, rust_analyzer_service, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CLIENT,
  PROP_CARGO_COMMAND,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

RustAnalyzerService *
rust_analyzer_service_new (void)
{
  return g_object_new (RUST_TYPE_ANALYZER_SERVICE, NULL);
}

static void
_cargo_toml_changed_cb (GFileMonitor      *monitor,
                        GFile             *file,
                        GFile             *other_file,
                        GFileMonitorEvent  event_type,
                        gpointer           user_data)
{
  RustAnalyzerService *self = RUST_ANALYZER_SERVICE (user_data);

  g_return_if_fail (RUST_IS_ANALYZER_SERVICE (self));

  if (self->supervisor != NULL)
    {
      IdeSubprocess *subprocess = ide_subprocess_supervisor_get_subprocess (self->supervisor);
      if (subprocess != NULL)
        ide_subprocess_force_exit (subprocess);
    }
}

static IdeSearchEngine *
_get_search_engine (RustAnalyzerService *self)
{
  IdeContext *context = NULL;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  return ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_SEARCH_ENGINE);
}

static GFile *
rust_analyzer_service_get_current_file (RustAnalyzerService *self)
{
  g_autoptr(IdeContext) context = NULL;
  IdeWorkbench *workbench = NULL;
  IdeWorkspace *workspace = NULL;
  IdeSurface *surface = NULL;
  IdePage *page = NULL;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));

  context = ide_object_ref_context (IDE_OBJECT (self));
  workbench = _ide_workbench_from_context (context);
  workspace = ide_workbench_get_current_workspace (workbench);
  surface = ide_workspace_get_surface_by_name (workspace, "editor");
  page = ide_editor_surface_get_active_page (IDE_EDITOR_SURFACE (surface));

  if (!IDE_IS_EDITOR_PAGE (page)) return NULL;

  IDE_RETURN (g_object_ref (ide_editor_page_get_file (IDE_EDITOR_PAGE (page))));
}

static gboolean
rust_analyzer_service_search_cargo_root (RustAnalyzerService *self,
                                         GFile               *dir)
{
  g_autoptr(GFile) cargofile = NULL;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));

  cargofile = g_file_get_child (dir, "Cargo.toml");

  if (g_file_query_exists (cargofile, NULL))
    IDE_RETURN (TRUE);

  IDE_RETURN (FALSE);
}

static GFile *
rust_analyzer_service_determine_workdir (RustAnalyzerService *self)
{
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(IdeContext) context = NULL;

  g_assert (RUST_IS_ANALYZER_SERVICE (self));

  /* Search workbench root first */
  context = ide_object_ref_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);
  if (rust_analyzer_service_search_cargo_root (self, workdir) == FALSE)
    {
      /* Search now from the current opened file upwards */
      g_autoptr(GFile) current_file = NULL;
      g_autoptr(GFile) parent = NULL;

      current_file = rust_analyzer_service_get_current_file (self);
      if (current_file == NULL) goto end;
      parent = g_file_get_parent (current_file);

      while (!g_file_equal (workdir, parent))
        {
          if (rust_analyzer_service_search_cargo_root (self, parent))
            {
              return g_steal_pointer (&parent);
            }
          parent = g_file_get_parent (parent);
        }
    }

end:
  return g_steal_pointer (&workdir);
}

static GVariant *
rust_analyzer_service_load_configuration (IdeLspClient *client,
                                          gpointer      user_data)
{
  RustAnalyzerService *self = (RustAnalyzerService *)user_data;
  GVariant *ret = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (RUST_IS_ANALYZER_SERVICE (self));

  ret = JSONRPC_MESSAGE_NEW_ARRAY ("{",
                                     "checkOnSave", "{",
                                       "command", JSONRPC_MESSAGE_PUT_STRING (self->cargo_command),
                                     "}",
                                   "}");

  IDE_RETURN (g_steal_pointer (&ret));
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
    case PROP_CARGO_COMMAND:
      g_value_set_string (value, rust_analyzer_service_get_cargo_command (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rust_analyzer_service_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  RustAnalyzerService *self = RUST_ANALYZER_SERVICE (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      rust_analyzer_service_set_client (self, g_value_get_object (value));
      break;
    case PROP_CARGO_COMMAND:
      rust_analyzer_service_set_cargo_command (self, g_value_dup_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
rust_analyzer_service_set_parent (IdeObject *object,
                                  IdeObject *parent)
{
  RustAnalyzerService *self = RUST_ANALYZER_SERVICE (object);

  IdeContext *context = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) cargo_toml = NULL;

  g_return_if_fail (RUST_IS_ANALYZER_SERVICE (object));

  if (parent == NULL)
    return;

  context = ide_object_get_context (object);
  workdir = ide_context_ref_workdir (context);
  cargo_toml = g_file_get_child (workdir, "Cargo.toml");

  if (g_file_query_exists (cargo_toml, NULL))
    {
      GError *error = NULL;

      if (self->cargo_monitor != NULL)
        return;

      self->cargo_monitor = g_file_monitor (cargo_toml, G_FILE_MONITOR_NONE, NULL, &error);
      if (error != NULL)
        {
          g_warning ("%s", error->message);
          return;
        }
      g_file_monitor_set_rate_limit (self->cargo_monitor, 5 * 1000); // 5 Seconds
      g_signal_connect (self->cargo_monitor, "changed", G_CALLBACK (_cargo_toml_changed_cb), self);
    }

}

static void
rust_analyzer_service_destroy (IdeObject *object)
{
  RustAnalyzerService *self = RUST_ANALYZER_SERVICE (object);
  IdeSearchEngine *search_engine = NULL;

  if (self->supervisor != NULL)
    {
      g_autoptr(IdeSubprocessSupervisor) supervisor = g_steal_pointer (&self->supervisor);

      ide_subprocess_supervisor_stop (supervisor);
    }

  g_clear_object (&self->client);

  search_engine = _get_search_engine (self);
  if (search_engine != NULL)
    ide_search_engine_remove_provider (search_engine, IDE_SEARCH_PROVIDER (self->search_provider));
  g_clear_object (&self->search_provider);

  IDE_OBJECT_CLASS (rust_analyzer_service_parent_class)->destroy (object);
}

static void
rust_analyzer_service_class_init (RustAnalyzerServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = rust_analyzer_service_get_property;
  object_class->set_property = rust_analyzer_service_set_property;

  i_class->parent_set = rust_analyzer_service_set_parent;
  i_class->destroy = rust_analyzer_service_destroy;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The Language Server client",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CARGO_COMMAND] =
    g_param_spec_string ("cargo-command",
                         "Cargo-command",
                         "The used cargo command for rust-analyzer",
                         "check",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
rust_analyzer_service_init (RustAnalyzerService *self)
{
  self->client = NULL;
  self->state = RUST_ANALYZER_SERVICE_INIT;
  self->settings = g_settings_new ("org.gnome.builder.rust-analyzer");
  g_settings_bind (self->settings, "cargo-command", self, "cargo-command", G_SETTINGS_BIND_DEFAULT);
}

IdeLspClient *
rust_analyzer_service_get_client (RustAnalyzerService *self)
{
  g_return_val_if_fail (RUST_IS_ANALYZER_SERVICE (self), NULL);

  return self->client;
}

void
rust_analyzer_service_set_client (RustAnalyzerService *self,
                                  IdeLspClient        *client)
{
  g_return_if_fail (RUST_IS_ANALYZER_SERVICE (self));
  g_return_if_fail (!client || IDE_IS_LSP_CLIENT (client));

  if (g_set_object (&self->client, client))
    {
      g_signal_connect_object (self->client,
                               "load-configuration",
                               G_CALLBACK (rust_analyzer_service_load_configuration),
                               self,
                               0);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
    }
}

static void
rust_analyzer_service_lsp_initialized (IdeLspClient *client,
                                       gpointer      user_data)
{
  RustAnalyzerService *self = (RustAnalyzerService *) user_data;
  g_autoptr(GVariant) params = NULL;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (RUST_IS_ANALYZER_SERVICE (self));

  params = JSONRPC_MESSAGE_NEW ("settings", "");
  ide_lsp_client_send_notification_async (client, "workspace/didChangeConfiguration", params, NULL, NULL, NULL);
}

void
rust_analyzer_service_lsp_started (IdeSubprocessSupervisor *supervisor,
                                   IdeSubprocess           *subprocess,
                                   gpointer                 user_data)
{
  RustAnalyzerService *self = user_data;
  g_autoptr(GIOStream) io_stream = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autofree gchar *root_uri = NULL;
  GInputStream *input;
  GOutputStream *output;
  IdeLspClient *client = NULL;

  g_return_if_fail (RUST_IS_ANALYZER_SERVICE (self));
  g_return_if_fail (IDE_IS_SUBPROCESS_SUPERVISOR (supervisor));
  g_return_if_fail (IDE_IS_SUBPROCESS (subprocess));

  input = ide_subprocess_get_stdout_pipe (subprocess);
  output = ide_subprocess_get_stdin_pipe (subprocess);
  io_stream = g_simple_io_stream_new (input, output);

  if (self->client != NULL)
    {
      ide_lsp_client_stop (self->client);
      ide_object_destroy (IDE_OBJECT (self->client));
    }

  client = ide_lsp_client_new (io_stream);
  g_signal_connect (client, "initialized", G_CALLBACK (rust_analyzer_service_lsp_initialized), self);
  rust_analyzer_service_set_client (self, client);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (client));
  ide_lsp_client_add_language (client, "rust");
  workdir = rust_analyzer_service_determine_workdir (self);
  root_uri = g_file_get_uri (workdir);
  ide_lsp_client_set_root_uri (client, root_uri);
  ide_lsp_client_start (client);

  // register SearchProvider
  if (self->search_provider == NULL)
    {
      IdeSearchEngine *search_engine = _get_search_engine (self);

      self->search_provider = rust_analyzer_search_provider_new ();
      ide_search_engine_add_provider (search_engine, IDE_SEARCH_PROVIDER (self->search_provider));
    }
  rust_analyzer_search_provider_set_client (self->search_provider, client);
}

static gboolean
rust_analyzer_service_check_rust_analyzer_bin (RustAnalyzerService *self)
{
  /* Check if `rust-analyzer` can be found on PATH or if there is an executable
   * in typical location
   */
  g_autoptr(GFile) rust_analyzer_bin_file = NULL;
  g_autofree gchar *rust_analyzer_bin = NULL;
  g_autoptr(GFileInfo) file_info = NULL;

  g_return_val_if_fail (RUST_IS_ANALYZER_SERVICE (self), FALSE);

  rust_analyzer_bin = g_find_program_in_path ("rust-analyzer");
  if (rust_analyzer_bin == NULL)
    {
      g_autofree gchar *path = NULL;
      const gchar *homedir = g_get_home_dir ();

      path = g_build_path (G_DIR_SEPARATOR_S, homedir, ".cargo", "bin", "rust-analyzer", NULL);
      rust_analyzer_bin_file = g_file_new_for_path (path);
    }
  else
    {
      rust_analyzer_bin_file = g_file_new_for_path (rust_analyzer_bin);
    }

  if (!g_file_query_exists (rust_analyzer_bin_file, NULL))
    {
      return FALSE;
    }

  file_info = g_file_query_info (rust_analyzer_bin_file,
                                 "*",
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL, NULL);

  if (ide_str_equal0 ("application/x-sharedlib", g_file_info_get_content_type (file_info)))
      return TRUE;

  return FALSE;
}

void
rust_analyzer_service_ensure_started (RustAnalyzerService *self)
{
  g_return_if_fail (RUST_IS_ANALYZER_SERVICE (self));

  if (self->state == RUST_ANALYZER_SERVICE_INIT)
    {
      if (!rust_analyzer_service_check_rust_analyzer_bin (self))
        {
          g_autoptr(IdeNotification) notification = NULL;
          IdeContext *context = NULL;

          self->state = RUST_ANALYZER_SERVICE_OFFER_DOWNLOAD;

          notification = ide_notification_new ();
          ide_notification_set_id (notification, "org.gnome-builder.rust-analyzer");
          ide_notification_set_title (notification, _("Your computer is missing the Rust Analyzer Language Server"));
          ide_notification_set_body (notification, _("The Language Server is necessary to provide IDE features like completion or diagnostic"));
          ide_notification_set_icon_name (notification, "dialog-warning-symbolic");
          ide_notification_add_button (notification, _("Install Language Server"), NULL, "win.install-rust-analyzer");
          ide_notification_set_urgent (notification, TRUE);
          context = ide_object_get_context (IDE_OBJECT (self));
          ide_notification_attach (notification, IDE_OBJECT (context));
        }
      else
          self->state = RUST_ANALYZER_SERVICE_READY;
    }
  else if (self->state == RUST_ANALYZER_SERVICE_READY)
    {
      g_autofree gchar *newpath = NULL;
      g_autoptr(GFile) workdir = NULL;
      g_autofree gchar *root_path = NULL;
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;
      const gchar *oldpath = NULL;

      launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDIN_PIPE);
      ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
      ide_subprocess_launcher_set_clear_env (launcher, TRUE);

      workdir = rust_analyzer_service_determine_workdir (self);
      root_path = g_file_get_path (workdir);
      ide_subprocess_launcher_set_cwd (launcher, root_path);
      oldpath = g_getenv ("PATH");
      newpath = g_strdup_printf ("%s/%s:%s", g_get_home_dir (), ".cargo/bin", oldpath);
      ide_subprocess_launcher_setenv (launcher, "PATH", newpath, TRUE);

      ide_subprocess_launcher_push_argv (launcher, "rust-analyzer");

      self->supervisor = ide_subprocess_supervisor_new ();
      g_signal_connect (self->supervisor, "spawned", G_CALLBACK (rust_analyzer_service_lsp_started), self);
      ide_subprocess_supervisor_set_launcher (self->supervisor, launcher);
      ide_subprocess_supervisor_start (self->supervisor);
      self->state = RUST_ANALYZER_SERVICE_LSP_STARTED;
    }
}

void
rust_analyzer_service_set_state (RustAnalyzerService *self,
                                 ServiceState         state)
{
  g_return_if_fail (RUST_IS_ANALYZER_SERVICE (self));

  self->state = state;
}

gchar *
rust_analyzer_service_get_cargo_command (RustAnalyzerService *self)
{
  g_return_val_if_fail (RUST_IS_ANALYZER_SERVICE (self), NULL);

  return self->cargo_command;
}

void
rust_analyzer_service_set_cargo_command (RustAnalyzerService *self,
                                         const gchar         *cargo_command)
{
  g_autoptr(GVariant) params = NULL;

  g_return_if_fail (RUST_IS_ANALYZER_SERVICE (self));
  g_return_if_fail (cargo_command != NULL);

  g_clear_pointer (&self->cargo_command, g_free);
  self->cargo_command = g_strdup (cargo_command);

  params = JSONRPC_MESSAGE_NEW ("settings", "");
  if (self->client != NULL)
    ide_lsp_client_send_notification_async (self->client, "workspace/didChangeConfiguration", params, NULL, NULL, NULL);
}
