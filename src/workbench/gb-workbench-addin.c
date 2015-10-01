/* gb-workbench-addin.c
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

#include "gb-workbench.h"
#include "gb-workbench-addin.h"

G_DEFINE_INTERFACE (GbWorkbenchAddin, gb_workbench_addin, G_TYPE_OBJECT)

static void
gb_workbench_addin_dummy (GbWorkbenchAddin *self,
                          GbWorkbench      *workbench)
{
}

static void
gb_workbench_addin_default_init (GbWorkbenchAddinInterface *iface)
{
  iface->load = gb_workbench_addin_dummy;
  iface->unload = gb_workbench_addin_dummy;
}

void
gb_workbench_addin_load (GbWorkbenchAddin *self,
                         GbWorkbench      *workbench)
{
  g_return_if_fail (GB_IS_WORKBENCH_ADDIN (self));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  GB_WORKBENCH_ADDIN_GET_IFACE (self)->load (self, workbench);
}
void
gb_workbench_addin_unload (GbWorkbenchAddin *self,
                           GbWorkbench      *workbench)
{
  g_return_if_fail (GB_IS_WORKBENCH_ADDIN (self));

  GB_WORKBENCH_ADDIN_GET_IFACE (self)->unload (self, workbench);
}
