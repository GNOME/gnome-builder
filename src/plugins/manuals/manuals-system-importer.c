/*
 * manuals-system-importer.c
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

#include "config.h"

#include <glib/gi18n.h>

#include <libdex.h>

#include "manuals-devhelp-importer.h"
#include "manuals-gom.h"
#include "manuals-job.h"
#include "manuals-sdk.h"
#include "manuals-system-importer.h"

#define VAR_RUN_HOST "/var/run/host"

struct _ManualsSystemImporter
{
  ManualsImporter parent_instance;
};

G_DEFINE_FINAL_TYPE (ManualsSystemImporter, manuals_system_importer, MANUALS_TYPE_IMPORTER)

typedef struct
{
  ManualsSystemImporter *self;
  ManualsRepository     *repository;
  ManualsProgress       *progress;
} Import;

static void
import_free (Import *state)
{
  g_clear_object (&state->self);
  g_clear_object (&state->repository);
  g_clear_object (&state->progress);
  g_free (state);
}

static DexFuture *
manuals_system_importer_import_fiber (gpointer user_data)
{
  g_autoptr(ManualsDevhelpImporter) devhelp = NULL;
  g_autoptr(ManualsJobMonitor) job = NULL;
  g_autoptr(ManualsSdk) sdk = NULL;
  g_autoptr(GomFilter) filter = NULL;
  g_autoptr(GError) error = NULL;
  Import *state = user_data;
  g_auto(GValue) gvalue = G_VALUE_INIT;
  gint64 sdk_id;

  g_assert (state != NULL);
  g_assert (MANUALS_IS_SYSTEM_IMPORTER (state->self));
  g_assert (MANUALS_IS_REPOSITORY (state->repository));
  g_assert (MANUALS_IS_PROGRESS (state->progress));

  job = manuals_progress_begin_job (state->progress);
  manuals_job_set_title (job, _("Importing System Documentation"));
  manuals_job_set_subtitle (job, _("Scanning system for new documentation"));

  /* Locate the previous SDK object for the host system or else
   * create and persist that object.
   */
  g_value_init (&gvalue, G_TYPE_STRING);
  g_value_set_static_string (&gvalue, "host");
  filter = gom_filter_new_eq (MANUALS_TYPE_SDK, "kind", &gvalue);
  if (!(sdk = dex_await_object (manuals_repository_find_one (state->repository,
                                                             MANUALS_TYPE_SDK,
                                                             filter),
                                NULL)))
    {
      sdk = g_object_new (MANUALS_TYPE_SDK,
                          "repository", state->repository,
                          "kind", "host",
                          "name", NULL,
                          "uri", "file://",
                          NULL);

      if (!dex_await (gom_resource_save (GOM_RESOURCE (sdk)), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  sdk_id = manuals_sdk_get_id (sdk);
  devhelp = manuals_devhelp_importer_new ();

  if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    {
      manuals_devhelp_importer_add_directory (devhelp, VAR_RUN_HOST "/usr/share/doc", sdk_id);
      manuals_devhelp_importer_add_directory (devhelp, VAR_RUN_HOST "/usr/share/gtk-doc/html", sdk_id);
    }
  else
    {
      manuals_devhelp_importer_add_directory (devhelp, "/usr/share/doc", sdk_id);
      manuals_devhelp_importer_add_directory (devhelp, "/usr/share/gtk-doc/html", sdk_id);
    }

  dex_await (manuals_importer_import (MANUALS_IMPORTER (devhelp),
                                      state->repository,
                                      state->progress),
             NULL);

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
manuals_system_importer_import (ManualsImporter   *importer,
                                ManualsRepository *repository,
                                ManualsProgress   *progress)
{
  ManualsSystemImporter *self = (ManualsSystemImporter *)importer;
  Import *state;

  g_assert (MANUALS_IS_SYSTEM_IMPORTER (self));
  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (MANUALS_IS_PROGRESS (progress));

  state = g_new0 (Import, 1);
  g_set_object (&state->self, self);
  g_set_object (&state->repository, repository);
  g_set_object (&state->progress, progress);

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (),
                              0,
                              manuals_system_importer_import_fiber,
                              state,
                              (GDestroyNotify)import_free);
}

static void
manuals_system_importer_class_init (ManualsSystemImporterClass *klass)
{
  ManualsImporterClass *importer_class = MANUALS_IMPORTER_CLASS (klass);

  importer_class->import = manuals_system_importer_import;
}

static void
manuals_system_importer_init (ManualsSystemImporter *self)
{
}

ManualsImporter *
manuals_system_importer_new (void)
{
  return g_object_new (MANUALS_TYPE_SYSTEM_IMPORTER, NULL);
}
