/* gbp-flatpak-sdk.c
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

#define G_LOG_DOMAIN "gbp-flatpak-sdk"

#include "config.h"

#include "gbp-flatpak-sdk.h"

struct _GbpFlatpakSdk
{
  IdeSdk parent_instance;
  char *id;
  char *name;
  char *arch;
  char *branch;
  char *sdk_name;
  char *sdk_branch;
  char *deploy_dir;
  char *metadata;
  guint is_sdk_extension : 1;
};

G_DEFINE_FINAL_TYPE (GbpFlatpakSdk, gbp_flatpak_sdk, IDE_TYPE_SDK)

static void
gbp_flatpak_sdk_dispose (GObject *object)
{
  GbpFlatpakSdk *self = (GbpFlatpakSdk *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->arch, g_free);
  g_clear_pointer (&self->branch, g_free);
  g_clear_pointer (&self->sdk_name, g_free);
  g_clear_pointer (&self->sdk_branch, g_free);
  g_clear_pointer (&self->deploy_dir, g_free);
  g_clear_pointer (&self->metadata, g_free);

  G_OBJECT_CLASS (gbp_flatpak_sdk_parent_class)->dispose (object);
}

static void
gbp_flatpak_sdk_class_init (GbpFlatpakSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_flatpak_sdk_dispose;
}

static void
gbp_flatpak_sdk_init (GbpFlatpakSdk *self)
{
}

GbpFlatpakSdk *
gbp_flatpak_sdk_new_from_variant (GVariant *variant)
{
  GbpFlatpakSdk *ret;
  g_autofree char *name = NULL;
  g_autofree char *arch = NULL;
  g_autofree char *branch = NULL;
  g_autofree char *sdk_name = NULL;
  g_autofree char *sdk_branch = NULL;
  g_autofree char *deploy_dir = NULL;
  g_autofree char *metadata = NULL;
  g_autofree char *title = NULL;
  gboolean is_sdk_extension = FALSE;

  g_return_val_if_fail (variant != NULL, NULL);
  g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE ("(sssssssb)")), NULL);

  g_variant_get (variant,
                 "(sssssssb)",
                 &name,
                 &arch,
                 &branch,
                 &sdk_name,
                 &sdk_branch,
                 &deploy_dir,
                 &metadata,
                 &is_sdk_extension);

  title = g_strdup_printf ("%s/%s/%s", name, arch, branch);

  ret = g_object_new (GBP_TYPE_FLATPAK_SDK,
                      "title", title,
                      "can-update", TRUE,
                      NULL);

  ret->id = g_strdup_printf ("runtime/%s/%s/%s", name, arch, branch);
  ret->name = g_steal_pointer (&name);
  ret->arch = g_steal_pointer (&arch);
  ret->branch = g_steal_pointer (&branch);
  ret->sdk_name = g_steal_pointer (&sdk_name);
  ret->sdk_branch = g_steal_pointer (&sdk_branch);
  ret->deploy_dir = g_steal_pointer (&deploy_dir);
  ret->metadata = g_steal_pointer (&metadata);
  ret->is_sdk_extension = !!is_sdk_extension;

  return ret;
}

const char *
gbp_flatpak_sdk_get_id (GbpFlatpakSdk *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_SDK (self), NULL);

  return self->id;
}
