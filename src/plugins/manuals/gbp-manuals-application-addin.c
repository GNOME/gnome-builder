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
#include "manuals-jhbuild-importer.h"
#include "manuals-progress.h"
#include "manuals-purge-missing.h"
#include "manuals-system-importer.h"

#ifdef HAVE_FLATPAK
# include "manuals-flatpak-importer.h"
#endif

struct _GbpManualsApplicationAddin
{
  GObject          parent_instance;

  ManualsProgress *import_progress;
  char            *storage_dir;
  DexFuture       *repository;
};

static DexFuture *
gbp_manuals_application_addin_import (DexFuture *completed,
                                      gpointer   user_data)
{
  GbpManualsApplicationAddin *self = user_data;
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(ManualsImporter) purge = NULL;
  g_autoptr(ManualsImporter) jhbuild = NULL;
  g_autoptr(ManualsImporter) system = NULL;
#ifdef HAVE_FLATPAK
  g_autoptr(ManualsImporter) flatpak = NULL;
#endif

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MANUALS_APPLICATION_ADDIN (self));

  repository = dex_await_object (dex_ref (completed), NULL);
  purge = manuals_purge_missing_new ();
  system = manuals_system_importer_new ();
  jhbuild = manuals_jhbuild_importer_new ();
#ifdef HAVE_FLATPAK
  flatpak = manuals_flatpak_importer_new ();
#endif

  return dex_future_all (manuals_importer_import (purge, repository, self->import_progress),
                         manuals_importer_import (system, repository, self->import_progress),
#ifdef HAVE_FLATPAK
                         manuals_importer_import (flatpak, repository, self->import_progress),
#endif
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

  /* TODO: Notify UI at all? */

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
gbp_manuals_application_addin_class_init (GbpManualsApplicationAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_manuals_application_addin_finalize;
}

static void
gbp_manuals_application_addin_init (GbpManualsApplicationAddin *self)
{
  g_autofree char *cache_root = ide_dup_default_cache_dir ();

  self->import_progress = manuals_progress_new ();
  self->storage_dir = g_build_filename (cache_root, "manuals", NULL);
}

DexFuture *
gbp_manuals_application_addin_load_repository (GbpManualsApplicationAddin *self)
{
  g_return_val_if_fail (GBP_IS_MANUALS_APPLICATION_ADDIN (self), NULL);

  return dex_ref (self->repository);
}
