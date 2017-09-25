/* ide-subprocess-supervisor.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include "subprocess/ide-subprocess.h"
#include "subprocess/ide-subprocess-launcher.h"

G_BEGIN_DECLS

#define IDE_TYPE_SUBPROCESS_SUPERVISOR (ide_subprocess_supervisor_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeSubprocessSupervisor, ide_subprocess_supervisor, IDE, SUBPROCESS_SUPERVISOR, GObject)

struct _IdeSubprocessSupervisorClass
{
  GObjectClass parent_class;

  void (*spawned) (IdeSubprocessSupervisor *self,
                   IdeSubprocess           *subprocess);

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IdeSubprocessSupervisor *ide_subprocess_supervisor_new            (void);
IdeSubprocessLauncher   *ide_subprocess_supervisor_get_launcher   (IdeSubprocessSupervisor *self);
void                     ide_subprocess_supervisor_set_launcher   (IdeSubprocessSupervisor *self,
                                                                   IdeSubprocessLauncher   *launcher);
void                     ide_subprocess_supervisor_start          (IdeSubprocessSupervisor *self);
void                     ide_subprocess_supervisor_stop           (IdeSubprocessSupervisor *self);
IdeSubprocess           *ide_subprocess_supervisor_get_subprocess (IdeSubprocessSupervisor *self);
void                     ide_subprocess_supervisor_set_subprocess (IdeSubprocessSupervisor *self,
                                                                   IdeSubprocess           *subprocess);

G_END_DECLS
