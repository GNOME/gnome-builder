/* ide-application-addin.c
 *
 * Copyright 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-application-addin"

#include "config.h"

#include "application/ide-application-addin.h"

/**
 * SECTION:ide-application-addin
 * @title: IdeApplicationAddin
 * @short_description: extend functionality of #IdeApplication
 *
 * The #IdeApplicationAddin interface is used by plugins that want to extend
 * the set of features provided by #IdeApplication. This is useful if you need
 * utility code that is bound to the lifetime of the #IdeApplication.
 *
 * The #IdeApplicationAddin is created after the application has initialized
 * and unloaded when Builder is shut down.
 *
 * Use this interface when you can share code between multiple projects that
 * are open at the same time.
 *
 * Since: 3.24
 */

G_DEFINE_INTERFACE (IdeApplicationAddin, ide_application_addin, G_TYPE_OBJECT)

static void
ide_application_addin_real_load (IdeApplicationAddin *self,
                                 IdeApplication      *application)
{
}

static void
ide_application_addin_real_unload (IdeApplicationAddin *self,
                                   IdeApplication      *application)
{
}

static void
ide_application_addin_default_init (IdeApplicationAddinInterface *iface)
{
  iface->load = ide_application_addin_real_load;
  iface->unload = ide_application_addin_real_unload;
}

/**
 * ide_application_addin_load:
 * @self: An #IdeApplicationAddin.
 * @application: An #IdeApplication.
 *
 * This interface method is called when the application is started or the
 * plugin has just been activated.
 *
 * Use this to setup code in your plugin that needs to be loaded once per
 * application process.
 *
 * Since: 3.24
 */
void
ide_application_addin_load (IdeApplicationAddin *self,
                            IdeApplication      *application)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_APPLICATION (application));

  IDE_APPLICATION_ADDIN_GET_IFACE (self)->load (self, application);
}

/**
 * ide_application_addin_unload:
 * @self: An #IdeApplicationAddin.
 * @application: An #IdeApplication.
 *
 * This inteface method is called when the application is shutting down or the
 * plugin has been unloaded.
 *
 * Use this function to cleanup after anything setup in
 * ide_application_addin_load().
 *
 * Since: 3.24
 */
void
ide_application_addin_unload (IdeApplicationAddin *self,
                              IdeApplication      *application)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_APPLICATION (application));

  IDE_APPLICATION_ADDIN_GET_IFACE (self)->unload (self, application);
}
