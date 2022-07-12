/* ide-application-addin.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-application-addin"

#include "config.h"

#include "ide-application-addin.h"

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
 * ide_application_addin_activate:
 * @self: a #IdeApplicationAddin
 * @application: an #ideApplication
 *
 * This function is activated when the GApplication::activate signal is
 * emitted.
 */
void
ide_application_addin_activate (IdeApplicationAddin *self,
                                IdeApplication      *application)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_APPLICATION (application));

  if (IDE_APPLICATION_ADDIN_GET_IFACE (self)->activate)
    IDE_APPLICATION_ADDIN_GET_IFACE (self)->activate (self, application);
}

/**
 * ide_application_addin_open:
 * @self: a #IdeApplicationAddin
 * @application: an #ideApplication
 * @files: (array length=n_files) (element-type GFile): an array of #GFiles
 * @n_files: the length of @files
 * @hint: a hint provided by the calling instance
 *
 * This function is activated when the #GApplication::open signal is emitted.
 */
void
ide_application_addin_open (IdeApplicationAddin  *self,
                            IdeApplication       *application,
                            GFile               **files,
                            gint                  n_files,
                            const gchar          *hint)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_APPLICATION (application));

  if (IDE_APPLICATION_ADDIN_GET_IFACE (self)->open)
    IDE_APPLICATION_ADDIN_GET_IFACE (self)->open (self, application, files, n_files, hint);
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
 */
void
ide_application_addin_unload (IdeApplicationAddin *self,
                              IdeApplication      *application)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_APPLICATION (application));

  IDE_APPLICATION_ADDIN_GET_IFACE (self)->unload (self, application);
}

/**
 * ide_application_addin_add_option_entries:
 * @self: a #IdeApplicationAddin
 * @application: an #IdeApplication
 *
 * This function is called to allow the application a chance to add various
 * command-line options to the #GOptionContext. See
 * g_application_add_main_option_entries() for more information on how to
 * add arguments.
 *
 * See ide_application_addin_handle_command_line() for how to handle arguments
 * once command line argument processing begins.
 *
 * Make sure you set `X-At-Startup=true` in your `.plugin` file so that the
 * plugin is loaded early during startup or this virtual function will not
 * be called.
 */
void
ide_application_addin_add_option_entries (IdeApplicationAddin *self,
                                          IdeApplication      *application)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_APPLICATION (application));

  if (IDE_APPLICATION_ADDIN_GET_IFACE (self)->add_option_entries)
    IDE_APPLICATION_ADDIN_GET_IFACE (self)->add_option_entries (self, application);
}

/**
 * ide_application_addin_handle_command_line:
 * @self: a #IdeApplicationAddin
 * @application: an #IdeApplication
 * @cmdline: a #GApplicationCommandLine
 *
 * This function is called to allow the addin to procses command line arguments
 * that were parsed based on options added in
 * ide_application_addin_add_option_entries().
 *
 * See g_application_command_line_get_option_dict() for more information.
 */
void
ide_application_addin_handle_command_line (IdeApplicationAddin     *self,
                                           IdeApplication          *application,
                                           GApplicationCommandLine *cmdline)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_APPLICATION (application));
  g_return_if_fail (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (IDE_APPLICATION_ADDIN_GET_IFACE (self)->handle_command_line)
    IDE_APPLICATION_ADDIN_GET_IFACE (self)->handle_command_line (self, application, cmdline);
}

void
ide_application_addin_workbench_added (IdeApplicationAddin *self,
                                       IdeWorkbench        *workbench)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  if (IDE_APPLICATION_ADDIN_GET_IFACE (self)->workbench_added)
    IDE_APPLICATION_ADDIN_GET_IFACE (self)->workbench_added (self, workbench);
}

void
ide_application_addin_workbench_removed (IdeApplicationAddin *self,
                                         IdeWorkbench        *workbench)
{
  g_return_if_fail (IDE_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  if (IDE_APPLICATION_ADDIN_GET_IFACE (self)->workbench_removed)
    IDE_APPLICATION_ADDIN_GET_IFACE (self)->workbench_removed (self, workbench);
}
