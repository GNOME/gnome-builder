/* gb-application-addin.c
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

#include "gb-application-addin.h"

G_DEFINE_INTERFACE (GbApplicationAddin, gb_application_addin, G_TYPE_OBJECT)

static void
gb_application_addin_real_load (GbApplicationAddin *self,
                                GbApplication      *application)
{
}

static void
gb_application_addin_real_unload (GbApplicationAddin *self,
                                  GbApplication      *application)
{
}

static void
gb_application_addin_default_init (GbApplicationAddinInterface *iface)
{
  iface->load = gb_application_addin_real_load;
  iface->unload = gb_application_addin_real_unload;
}

void
gb_application_addin_load (GbApplicationAddin *self,
                           GbApplication      *application)
{
  g_return_if_fail (GB_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (GB_IS_APPLICATION (application));

  GB_APPLICATION_ADDIN_GET_IFACE (self)->load (self, application);
}

void
gb_application_addin_unload (GbApplicationAddin *self,
                             GbApplication      *application)
{
  g_return_if_fail (GB_IS_APPLICATION_ADDIN (self));
  g_return_if_fail (GB_IS_APPLICATION (application));

  GB_APPLICATION_ADDIN_GET_IFACE (self)->unload (self, application);
}
