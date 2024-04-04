/* gbp-manuals-application-addin.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-manuals-application-addin"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-manuals-application-addin.h"

#include "manuals-importer.h"
#include "manuals-flatpak-importer.h"
#include "manuals-jhbuild-importer.h"
#include "manuals-navigatable.h"
#include "manuals-navigatable-model.h"
#include "manuals-progress.h"
#include "manuals-purge-missing.h"
#include "manuals-system-importer.h"

struct _GbpManualsApplicationAddin
{
  GObject          parent_instance;

  ManualsProgress *import_progress;
  char            *storage_dir;
  DexFuture       *repository;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static DexFuture *
gbp_manuals_application_addin_import (DexFuture *completed,
                                      gpointer   user_data)
{
  GbpManualsApplicationAddin *self = user_data;
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(ManualsImporter) purge = NULL;
  g_autoptr(ManualsImporter) flatpak = NULL;
  g_autoptr(ManualsImporter) jhbuild = NULL;
  g_autoptr(ManualsImporter) system = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (self));

  repository = dex_await_object (dex_ref (completed), NULL);
  purge = manuals_purge_missing_new ();
  //flatpak = manuals_flatpak_importer_new ();
  system = manuals_system_importer_new ();
  jhbuild = manuals_jhbuild_importer_new ();

  return dex_future_all (manuals_importer_import (purge, repository, self->import_progress),
                         manuals_importer_import (system, repository, self->import_progress),
                         //manuals_importer_import (flatpak, repository, self->import_progress),
                         manuals_importer_import (jhbuild, repository, self->import_progress),
                         NULL);

}

static DexFuture *
gbp_manuals_application_addin_import_complete (DexFuture *completed,
                                               gpointer   user_data)
{
  GbpManualsApplicationAddin *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);

  return NULL;
}

static void
gbp_manuals_application_addin_load (IdeApplicationAddin *addin,
                                    IdeApplication      *application)
{
  GbpManualsApplicationAddin *self = (GbpManualsApplicationAddin *)addin;
  g_autofree char *storage_path = NULL;
  DexFuture *future;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (application));

  /* Ensure our storage directory is created */
  storage_path = g_build_filename (self->storage_dir, "manuals.sqlite", NULL);
  g_mkdir_with_parents (self->storage_dir, 0750);

  /* Start loading repository asynchronously */
  self->repository = manuals_repository_open (storage_path);

  future = dex_future_then (dex_ref (self->repository),
                            gbp_manuals_application_addin_import,
                            g_object_ref (self),
                            g_object_unref);
  future = dex_future_finally (future,
                               gbp_manuals_application_addin_import_complete,
                               g_object_ref (self),
                               g_object_unref);
  dex_future_disown (future);
}

static void
gbp_manuals_application_addin_unload (IdeApplicationAddin *addin,
                                      IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

}

static void
app_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_manuals_application_addin_load;
  iface->unload = gbp_manuals_application_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpManualsApplicationAddin, gbp_manuals_application_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, app_addin_iface_init))

static void
gbp_manuals_application_addin_finalize (GObject *object)
{
  GbpManualsApplicationAddin *self = (GbpManualsApplicationAddin *)object;

  dex_clear (&self->repository);
  g_clear_object (&self->import_progress);
  g_clear_pointer (&self->storage_dir, g_free);

  G_OBJECT_CLASS (gbp_manuals_application_addin_parent_class)->finalize (object);
}

static void
gbp_manuals_application_addin_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  GbpManualsApplicationAddin *self = GBP_MANUALS_APPLICATION_ADDIN (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_take_object (value, gbp_manuals_application_addin_dup_model (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_manuals_application_addin_class_init (GbpManualsApplicationAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_manuals_application_addin_finalize;
  object_class->get_property = gbp_manuals_application_addin_get_property;

  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_manuals_application_addin_init (GbpManualsApplicationAddin *self)
{
  g_autofree char *cache_root = ide_dup_default_cache_dir ();

  self->import_progress = manuals_progress_new ();
  self->storage_dir = g_build_filename (cache_root, "manuals", NULL);
}

GListModel *
gbp_manuals_application_addin_dup_model (GbpManualsApplicationAddin *self)
{
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(ManualsNavigatable) navigatable = NULL;

  g_return_val_if_fail (GBP_IS_MANUALS_APPLICATION_ADDIN (self), NULL);

  if (!dex_future_is_resolved (self->repository))
    return NULL;

  if (!(repository = dex_await_object (dex_ref (self->repository), NULL)))
    return NULL;

  navigatable = manuals_navigatable_new_for_resource (G_OBJECT (repository));

  return G_LIST_MODEL (manuals_navigatable_model_new (navigatable));
}
