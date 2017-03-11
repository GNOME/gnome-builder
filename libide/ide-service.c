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
  CONTEXT_LOADED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

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

  if (IDE_SERVICE_GET_IFACE (service)->start)
    IDE_SERVICE_GET_IFACE (service)->start (service);
}

void
ide_service_stop (IdeService *service)
{
  g_return_if_fail (IDE_IS_SERVICE (service));

  if (IDE_SERVICE_GET_IFACE (service)->stop)
    IDE_SERVICE_GET_IFACE (service)->stop (service);
}

void
_ide_service_emit_context_loaded (IdeService *service)
{
  g_return_if_fail (IDE_IS_SERVICE (service));

  g_signal_emit (service, signals [CONTEXT_LOADED], 0);
}

static const gchar *
ide_service_real_get_name (IdeService *service)
{
  return G_OBJECT_TYPE_NAME (service);
}

static void
ide_service_default_init (IdeServiceInterface *iface)
{
  iface->get_name = ide_service_real_get_name;

  signals [CONTEXT_LOADED] =
    g_signal_new ("context-loaded",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeServiceInterface, context_loaded),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}
