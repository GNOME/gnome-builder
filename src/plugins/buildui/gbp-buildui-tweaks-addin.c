/* gbp-buildui-tweaks-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-buildui-tweaks-addin"

#include "config.h"

#include <libide-foundry.h>

#include "gbp-buildui-tweaks-addin.h"

struct _GbpBuilduiTweaksAddin
{
  IdeTweaksAddin parent_instance;
  IdeContext *context;
};

G_DEFINE_FINAL_TYPE (GbpBuilduiTweaksAddin, gbp_buildui_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static void
gbp_buildui_tweaks_addin_load (IdeTweaksAddin *addin,
                               IdeTweaks      *tweaks)
{
  GbpBuilduiTweaksAddin *self = (GbpBuilduiTweaksAddin *)addin;

  g_assert (GBP_IS_BUILDUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  if ((self->context = ide_tweaks_get_context (tweaks)))
    {
      IdeRuntimeManager *runtime_manager = ide_runtime_manager_from_context (self->context);
      ide_tweaks_expose_object (tweaks, "Runtimes", G_OBJECT (runtime_manager));
    }

  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/buildui/tweaks.ui"));

  IDE_TWEAKS_ADDIN_CLASS (gbp_buildui_tweaks_addin_parent_class)->load (addin, tweaks);
}

static void
gbp_buildui_tweaks_addin_unload (IdeTweaksAddin *addin,
                                 IdeTweaks      *tweaks)
{
  GbpBuilduiTweaksAddin *self = (GbpBuilduiTweaksAddin *)addin;

  g_assert (GBP_IS_BUILDUI_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS (tweaks));

  self->context = NULL;
}

static void
gbp_buildui_tweaks_addin_class_init (GbpBuilduiTweaksAddinClass *klass)
{
  IdeTweaksAddinClass *tweaks_addin_class = IDE_TWEAKS_ADDIN_CLASS (klass);

  tweaks_addin_class->load = gbp_buildui_tweaks_addin_load;
  tweaks_addin_class->unload = gbp_buildui_tweaks_addin_unload;
}

static void
gbp_buildui_tweaks_addin_init (GbpBuilduiTweaksAddin *self)
{
}
