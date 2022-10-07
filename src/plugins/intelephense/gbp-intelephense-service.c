/* gbp-intelephense-service.c
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

#define G_LOG_DOMAIN "gbp-intelephense-service"

#include "config.h"

#include <glib/gi18n.h>

#include <jsonrpc-glib.h>

#include "gbp-intelephense-service.h"

struct _GbpIntelephenseService
{
  IdeLspService    parent_instance;
  IdeNotification *notif;
};

G_DEFINE_FINAL_TYPE (GbpIntelephenseService, gbp_intelephense_service, IDE_TYPE_LSP_SERVICE)

static GVariant *
gbp_intelephense_service_client_load_configuration_cb (GbpIntelephenseService *self,
                                                       IdeLspClient           *client)
{
  GVariant *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_INTELEPHENSE_SERVICE (self));
  g_assert (IDE_IS_LSP_CLIENT (client));

  ret = JSONRPC_MESSAGE_NEW (
    "intelephense", "{",
      "files", "{",
        "associations", "[", "*.php", "*.phtml", "]",
        "exclude", "[", "]",
      "}",
      "completion", "{",
        "insertUseDeclaration", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
        "fullyQualifyGlobalConstantsAndFunctions", JSONRPC_MESSAGE_PUT_BOOLEAN (FALSE),
        "triggerParameterHints", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
        "maxItems", JSONRPC_MESSAGE_PUT_INT32 (100),
      "}",
      "format", "{",
        "enable", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
      "}",
    "}"
  );

  IDE_RETURN (ret);
}

static void
clear_notification (GbpIntelephenseService *self)
{
  g_assert (GBP_IS_INTELEPHENSE_SERVICE (self));

  if (self->notif == NULL)
    return;

  ide_notification_withdraw (self->notif);
  g_clear_object (&self->notif);
}

static void
gbp_intelephense_service_client_notification_cb (GbpIntelephenseService *self,
                                                 const char             *method,
                                                 GVariant               *params,
                                                 IdeLspClient           *client)
{
  IDE_ENTRY;

  g_assert (GBP_IS_INTELEPHENSE_SERVICE (self));
  g_assert (method != NULL);

  if (ide_str_equal0 (method, "indexingStarted"))
    {
      clear_notification (self);
      self->notif = g_object_new (IDE_TYPE_NOTIFICATION,
                                  "id", "org.gnome.builder.intelephense.indexing",
                                  "title", "Intelephense",
                                  "body", _("Indexing PHP code"),
                                  "has-progress", TRUE,
                                  "progress-is-imprecise", TRUE,
                                  NULL);
      ide_notification_attach (self->notif, IDE_OBJECT (self));
    }
  else if (ide_str_equal0 (method, "indexingEnded"))
    {
      clear_notification (self);
    }

  IDE_EXIT;
}

static void
gbp_intelephense_service_configure_client (IdeLspService *service,
                                           IdeLspClient  *client)
{
  GbpIntelephenseService *self = (GbpIntelephenseService *)service;

  IDE_ENTRY;

  g_assert (GBP_IS_INTELEPHENSE_SERVICE (self));
  g_assert (IDE_IS_LSP_CLIENT (client));

  ide_lsp_client_add_language (client, "php");

  g_signal_connect_object (client,
                           "load-configuration",
                           G_CALLBACK (gbp_intelephense_service_client_load_configuration_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (client,
                           "notification",
                           G_CALLBACK (gbp_intelephense_service_client_notification_cb),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_intelephense_service_prepare_run_context (IdeLspService *service,
                                              IdePipeline   *pipeline,
                                              IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_assert (IDE_IS_LSP_SERVICE (service));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  ide_run_context_append_argv (run_context, "--stdio");

  IDE_EXIT;
}

static void
gbp_intelephense_service_dispose (GObject *object)
{
  GbpIntelephenseService *self = (GbpIntelephenseService *)object;

  clear_notification (self);

  G_OBJECT_CLASS (gbp_intelephense_service_parent_class)->dispose (object);
}

static void
gbp_intelephense_service_class_init (GbpIntelephenseServiceClass *klass)
{
  IdeLspServiceClass *lsp_service_class = IDE_LSP_SERVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_intelephense_service_dispose;

  lsp_service_class->configure_client = gbp_intelephense_service_configure_client;
  lsp_service_class->prepare_run_context = gbp_intelephense_service_prepare_run_context;
}

static void
gbp_intelephense_service_init (GbpIntelephenseService *self)
{
  ide_lsp_service_set_program (IDE_LSP_SERVICE (self), "intelephense");
}
