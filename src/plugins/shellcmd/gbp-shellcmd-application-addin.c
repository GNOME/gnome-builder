/* gbp-shellcmd-application-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shellcmd-application-addin"

#include "config.h"

#include "gbp-shellcmd-application-addin.h"

struct _GbpShellcmdApplicationAddin
{
  GObject                  parent_instance;
  GbpShellcmdCommandModel *model;
};

GbpShellcmdCommandModel *
gbp_shellcmd_application_addin_get_model (GbpShellcmdApplicationAddin *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_APPLICATION_ADDIN (self), NULL);

  return self->model;
}

static void
gbp_shellcmd_application_addin_load (IdeApplicationAddin *addin,
                                     IdeApplication      *app)
{
  GbpShellcmdApplicationAddin *self = (GbpShellcmdApplicationAddin *)addin;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_SHELLCMD_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (app));

  self->model = gbp_shellcmd_command_model_new ();

  if (!gbp_shellcmd_command_model_load (self->model, NULL, &error))
    g_warning ("Failed to load external-commands: %s", error->message);
}

static void
gbp_shellcmd_application_addin_unload (IdeApplicationAddin *addin,
                                       IdeApplication      *app)
{
  GbpShellcmdApplicationAddin *self = (GbpShellcmdApplicationAddin *)addin;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_SHELLCMD_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (app));

  if (!gbp_shellcmd_command_model_save (self->model, NULL, &error))
    g_warning ("Failed to save external-commands: %s", error->message);

  g_clear_object (&self->model);
}

static void
app_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_shellcmd_application_addin_load;
  iface->unload = gbp_shellcmd_application_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpShellcmdApplicationAddin, gbp_shellcmd_application_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, app_addin_iface_init))

static void
gbp_shellcmd_application_addin_class_init (GbpShellcmdApplicationAddinClass *klass)
{
}

static void
gbp_shellcmd_application_addin_init (GbpShellcmdApplicationAddin *self)
{
}
