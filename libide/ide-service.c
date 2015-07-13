/* ide-service.c
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

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-service.h"

G_DEFINE_INTERFACE (IdeService, ide_service, IDE_TYPE_OBJECT)

enum {
  LOADED,
  START,
  STOP,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

const gchar *
ide_service_get_name (IdeService *service)
{
  g_return_val_if_fail (IDE_IS_SERVICE (service), NULL);

  return IDE_SERVICE_GET_IFACE (service)->get_name (service);
}

void
ide_service_start (IdeService *service)
{
  g_return_if_fail (IDE_IS_SERVICE (service));

  g_signal_emit (service, gSignals [START], 0);
}

void
ide_service_stop (IdeService *service)
{
  g_return_if_fail (IDE_IS_SERVICE (service));

  g_signal_emit (service, gSignals [STOP], 0);
}

void
_ide_service_emit_loaded (IdeService *service)
{
  g_return_if_fail (IDE_IS_SERVICE (service));

  g_signal_emit (service, gSignals [LOADED], 0);
}

static void
ide_service_real_start (IdeService *service)
{
}

static void
ide_service_real_stop (IdeService *service)
{
}

static const gchar *
ide_service_real_get_name (IdeService *service)
{
  return G_OBJECT_TYPE_NAME (service);
}

static void
ide_service_default_init (IdeServiceInterface *iface)
{
  iface->start = ide_service_real_start;
  iface->start = ide_service_real_stop;
  iface->get_name = ide_service_real_get_name;

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            IDE_TYPE_CONTEXT,
                                                            (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));

  gSignals [LOADED] =
    g_signal_new ("loaded",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeServiceInterface, loaded),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  gSignals [START] =
    g_signal_new ("start",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeServiceInterface, start),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  gSignals [STOP] =
    g_signal_new ("stop",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeServiceInterface, stop),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}
