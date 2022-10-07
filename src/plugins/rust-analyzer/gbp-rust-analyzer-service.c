/* gbp-rust-analyzer-service.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-rust-analyzer-service"

#include "config.h"

#include <jsonrpc-glib.h>

#include <libide-threading.h>

#include "gbp-rust-analyzer-service.h"

struct _GbpRustAnalyzerService
{
  IdeLspService parent_instance;
  GSettings *settings;
  IdeLspClient *client;
};

G_DEFINE_FINAL_TYPE (GbpRustAnalyzerService, gbp_rust_analyzer_service, IDE_TYPE_LSP_SERVICE)

static void
gbp_rust_analyzer_service_settings_changed_cb (GbpRustAnalyzerService *self,
                                               const char             *key,
                                               GSettings              *settings)
{
  IDE_ENTRY;

  g_assert (GBP_RUST_ANALYZER_SERVICE (self));
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
gbp_rust_analyzer_service_initialized_cb (GbpRustAnalyzerService *self,
                                          IdeLspClient           *client)
{
  g_autoptr(GVariant) params = NULL;

  IDE_ENTRY;

  g_assert (GBP_RUST_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_LSP_CLIENT (client));

  params = JSONRPC_MESSAGE_NEW ("settings", "");

  ide_lsp_client_send_notification_async (client,
                                          "workspace/didChangeConfiguration",
                                          params,
                                          NULL, NULL, NULL);

  IDE_EXIT;
}

static GVariant *
gbp_rust_analyzer_service_load_configuration_cb (GbpRustAnalyzerService *self,
                                                 IdeLspClient           *client)
{
  g_autoptr(GVariant) ret = NULL;
  g_autofree char *command = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (client));
  g_assert (GBP_RUST_ANALYZER_SERVICE (self));

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
gbp_rust_analyzer_service_configure_client (IdeLspService *service,
                                            IdeLspClient  *client)
{
  GbpRustAnalyzerService *self = (GbpRustAnalyzerService *)service;
  g_autoptr(GVariant) params = NULL;
  g_auto(GStrv) features = NULL;
  struct {
    guint build_scripts : 1;
    guint proc_macro : 1;
    guint range_formatting : 1;
  } feat;

  IDE_ENTRY;

  g_assert (GBP_IS_RUST_ANALYZER_SERVICE (self));
  g_assert (IDE_IS_LSP_CLIENT (client));

  g_set_object (&self->client, client);

  ide_lsp_client_add_language (client, "rust");

  features = g_settings_get_strv (self->settings, "features");
  feat.build_scripts = g_strv_contains ((const char * const *)features, "cargo.buildScripts.enable");
  feat.proc_macro = g_strv_contains ((const char * const *)features, "procMacro.enable");
  feat.range_formatting = g_strv_contains ((const char * const *)features, "rustfmt.rangeFormatting.enable");

  /* Opt-in for experimental proc-macro feature to make gtk-rs more
   * useful for GNOME developers.
   *
   * See: https://rust-analyzer.github.io/manual.html#configuration
   */
  params = JSONRPC_MESSAGE_NEW (
    "cargo", "{",
      "buildScripts", "{",
        "enable", JSONRPC_MESSAGE_PUT_BOOLEAN (feat.build_scripts),
      "}",
    "}",
    "procMacro", "{",
      "enable", JSONRPC_MESSAGE_PUT_BOOLEAN (feat.proc_macro),
    "}",
    "rustfmt", "{",
      "rangeFormatting", "{",
        "enable", JSONRPC_MESSAGE_PUT_BOOLEAN (feat.range_formatting),
      "}",
    "}"
  );

  ide_lsp_client_set_initialization_options (client, params);

  g_object_set (client,
                "use-markdown-in-diagnostics", TRUE,
                NULL);

  g_signal_connect_object (client,
                           "load-configuration",
                           G_CALLBACK (gbp_rust_analyzer_service_load_configuration_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (client,
                           "initialized",
                           G_CALLBACK (gbp_rust_analyzer_service_initialized_cb),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_rust_analyzer_service_dispose (GObject *object)
{
  GbpRustAnalyzerService *self = (GbpRustAnalyzerService *)object;

  g_clear_object (&self->settings);
  g_clear_object (&self->client);

  G_OBJECT_CLASS (gbp_rust_analyzer_service_parent_class)->dispose (object);
}

static void
gbp_rust_analyzer_service_class_init (GbpRustAnalyzerServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeLspServiceClass *lsp_service_class = IDE_LSP_SERVICE_CLASS (klass);

  object_class->dispose = gbp_rust_analyzer_service_dispose;

  lsp_service_class->configure_client = gbp_rust_analyzer_service_configure_client;
}

static void
gbp_rust_analyzer_service_init (GbpRustAnalyzerService *self)
{
  ide_lsp_service_set_program (IDE_LSP_SERVICE (self), "rust-analyzer");

  self->settings = g_settings_new ("org.gnome.builder.rust-analyzer");
  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (gbp_rust_analyzer_service_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
