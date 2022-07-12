/* ide-application-addin.h
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

#pragma once

#include <libide-core.h>

#include "ide-application.h"

G_BEGIN_DECLS

#define IDE_TYPE_APPLICATION_ADDIN (ide_application_addin_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeApplicationAddin, ide_application_addin, IDE, APPLICATION_ADDIN, GObject)

/**
 * IdeApplicationAddinInterface:
 * @load: Set this virtual method to implement the ide_application_addin_load()
 *   virtual method.
 * @unload: Set this virtual method to implement the
 *   ide_application_addin_unload() virtual method.
 * @add_option_entries: Set this virtual method to add option entries to
 *   the gnome-builder command-line argument parsing. See
 *   g_application_add_main_option_entries().
 * @handle_command_line: Set this virtual method to handle parsing command
 *   line arguments.
 */
struct _IdeApplicationAddinInterface
{
  GTypeInterface parent_interface;

  void (*load)                (IdeApplicationAddin     *self,
                               IdeApplication          *application);
  void (*unload)              (IdeApplicationAddin     *self,
                               IdeApplication          *application);
  void (*activate)            (IdeApplicationAddin     *self,
                               IdeApplication          *application);
  void (*open)                (IdeApplicationAddin     *self,
                               IdeApplication          *application,
                               GFile                  **files,
                               gint                     n_files,
                               const gchar             *hint);
  void (*add_option_entries)  (IdeApplicationAddin     *self,
                               IdeApplication          *application);
  void (*handle_command_line) (IdeApplicationAddin     *self,
                               IdeApplication          *application,
                               GApplicationCommandLine *cmdline);
  void (*workbench_added)     (IdeApplicationAddin     *self,
                               IdeWorkbench            *workbench);
  void (*workbench_removed)   (IdeApplicationAddin     *self,
                               IdeWorkbench            *workbench);
};

IDE_AVAILABLE_IN_ALL
void ide_application_addin_open                (IdeApplicationAddin    *self,
                                                IdeApplication         *application,
                                                GFile                 **files,
                                                gint                    n_files,
                                                const gchar             *hint);
IDE_AVAILABLE_IN_ALL
void ide_application_addin_activate            (IdeApplicationAddin     *self,
                                                IdeApplication          *application);
IDE_AVAILABLE_IN_ALL
void ide_application_addin_load                (IdeApplicationAddin     *self,
                                                IdeApplication          *application);
IDE_AVAILABLE_IN_ALL
void ide_application_addin_unload              (IdeApplicationAddin     *self,
                                                IdeApplication          *application);
IDE_AVAILABLE_IN_ALL
void ide_application_addin_add_option_entries  (IdeApplicationAddin     *self,
                                                IdeApplication          *application);
IDE_AVAILABLE_IN_ALL
void ide_application_addin_handle_command_line (IdeApplicationAddin     *self,
                                                IdeApplication          *application,
                                                GApplicationCommandLine *cmdline);
IDE_AVAILABLE_IN_ALL
void ide_application_addin_workbench_added     (IdeApplicationAddin     *self,
                                                IdeWorkbench            *workbench);
IDE_AVAILABLE_IN_ALL
void ide_application_addin_workbench_removed   (IdeApplicationAddin     *self,
                                                IdeWorkbench            *workbench);

G_END_DECLS
