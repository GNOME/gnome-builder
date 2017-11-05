/* ide-service.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

/**
 * SECTION:ide-service
 * @title: IdeService
 * @short_description: Provide project services for plugins
 *
 * The #IdeService inteface is meant as a place to provide utility code to
 * your plugin that should have it's lifetime bound to the lifetime of the
 * loaded project.
 *
 * When the project is created, the service will be started. When the project
 * is closed, the service will be stopped and discarded.
 *
 * Since: 3.16
 */

enum {
  CONTEXT_LOADED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

/**
 * ide_service_get_name:
 * @self: an #IdeService
 *
 * Gets the name of the service. By default, the name of the object is
 * used as the service name.
 *
 * Returns: (not nullable): A name for the service.
 *
 * Since: 3.16
 */
const gchar *
ide_service_get_name (IdeService *self)
{
  g_return_val_if_fail (IDE_IS_SERVICE (self), NULL);

  return IDE_SERVICE_GET_IFACE (self)->get_name (self);
}

/**
 * ide_service_start:
 * @self: an #IdeService
 *
 * This calls the #IdeServiceInterface.start virtual function which plugins
 * use to initialize their service.
 *
 * This function is called by the owning #IdeContext and should not be needed
 * by plugins or other internal API in Builder.
 *
 * Since: 3.16
 */
void
ide_service_start (IdeService *self)
{
  g_return_if_fail (IDE_IS_SERVICE (self));

  if (IDE_SERVICE_GET_IFACE (self)->start)
    IDE_SERVICE_GET_IFACE (self)->start (self);
}

/**
 * ide_service_stop:
 * @self: an #IdeService
 *
 * This calls the #IdeServiceInterface.stop virtual function which plugins
 * use to cleanup after their service.
 *
 * This function is called by the owning #IdeContext and should not be needed
 * by plugins or other internal API in Builder.
 *
 * Since: 3.16
 */
void
ide_service_stop (IdeService *self)
{
  g_return_if_fail (IDE_IS_SERVICE (self));

  if (IDE_SERVICE_GET_IFACE (self)->stop)
    IDE_SERVICE_GET_IFACE (self)->stop (self);
}

void
_ide_service_emit_context_loaded (IdeService *self)
{
  g_return_if_fail (IDE_IS_SERVICE (self));

  g_signal_emit (self, signals [CONTEXT_LOADED], 0);
}

static const gchar *
ide_service_real_get_name (IdeService *self)
{
  return G_OBJECT_TYPE_NAME (self);
}

static void
ide_service_default_init (IdeServiceInterface *iface)
{
  iface->get_name = ide_service_real_get_name;

  /**
   * IdeService::context-loaded:
   * @self: an #IdeService
   *
   * The "context-loaded" signal is emitted when the owning #IdeContext
   * has completed loading the project. This may be useful if you want to
   * defer startup procedures until the context is fully loaded.
   *
   * Since: 3.20
   */
  signals [CONTEXT_LOADED] =
    g_signal_new ("context-loaded",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeServiceInterface, context_loaded),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);
}
