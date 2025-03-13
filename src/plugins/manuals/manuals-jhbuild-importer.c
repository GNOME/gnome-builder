/*
 * manuals-jhbuild-importer.c
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
#include "manuals-jhbuild-importer.h"

struct _ManualsJhbuildImporter
{
  ManualsImporter parent_instance;
};

G_DEFINE_FINAL_TYPE (ManualsJhbuildImporter, manuals_jhbuild_importer, MANUALS_TYPE_IMPORTER)

typedef struct
{
  ManualsJhbuildImporter *self;
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

static inline gpointer
get_jhbuild_install_dir_thread (gpointer data)
{
  g_autoptr(DexPromise) promise = data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  g_autoptr(GSubprocess) subprocess = NULL;

  if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    subprocess = g_subprocess_launcher_spawn (launcher, &error, "flatpak-spawn", "--host", "--watch-bus", "jhbuild", "run", "sh", "-c", "echo $JHBUILD_PREFIX", NULL);
  else
    subprocess = g_subprocess_launcher_spawn (launcher, &error, "jhbuild", "run", "sh", "-c", "echo $JHBUILD_PREFIX", NULL);

  if (subprocess != NULL)
    {
      g_autofree char *stdout_buf = NULL;

      if (g_subprocess_communicate_utf8 (subprocess, NULL, NULL, &stdout_buf, NULL, &error))
        {
          dex_promise_resolve_string (promise, g_strstrip (g_steal_pointer (&stdout_buf)));
          return NULL;
        }
    }

  dex_promise_reject (promise, g_steal_pointer (&error));

  return NULL;
}

static DexFuture *
get_jhbuild_install_dir (void)
{
  DexPromise *promise = dex_promise_new ();
  g_autoptr(GThread) thread = g_thread_new ("jhbuild-discovery",
                                            get_jhbuild_install_dir_thread,
                                            dex_ref (promise));
  return DEX_FUTURE (promise);
}

static DexFuture *
manuals_jhbuild_importer_import_fiber (gpointer user_data)
{
  g_autofree char *jhbuild_dir = NULL;
  g_autofree char *docdir = NULL;
  g_autofree char *gtkdocdir = NULL;
  g_autoptr(ManualsDevhelpImporter) devhelp = NULL;
  g_autoptr(ManualsJobMonitor) job = NULL;
  g_autoptr(ManualsSdk) sdk = NULL;
  g_autoptr(GomFilter) filter = NULL;
  g_autoptr(GError) error = NULL;
  Import *state = user_data;
  g_auto(GValue) gvalue = G_VALUE_INIT;
  gint64 sdk_id;

  g_assert (state != NULL);
  g_assert (MANUALS_IS_JHBUILD_IMPORTER (state->self));
  g_assert (MANUALS_IS_REPOSITORY (state->repository));
  g_assert (MANUALS_IS_PROGRESS (state->progress));

  /* If we can't discover jhbuild directory, just bail */
  if (!(jhbuild_dir = dex_await_string (get_jhbuild_install_dir (), NULL)))
    return dex_future_new_for_boolean (TRUE);

  job = manuals_progress_begin_job (state->progress);
  manuals_job_set_title (job, _("Importing JHBuild Documentation"));
  manuals_job_set_subtitle (job, _("Scanning jhbuild for new documentation"));

  /* Locate the previous SDK object for the host system or else
   * create and persist that object.
   */
  g_value_init (&gvalue, G_TYPE_STRING);
  g_value_set_static_string (&gvalue, "jhbuild");
  filter = gom_filter_new_eq (MANUALS_TYPE_SDK, "kind", &gvalue);
  if (!(sdk = dex_await_object (manuals_repository_find_one (state->repository,
                                                             MANUALS_TYPE_SDK,
                                                             filter),
                                NULL)))
    {
      sdk = g_object_new (MANUALS_TYPE_SDK,
                          "repository", state->repository,
                          "kind", "jhbuild",
                          "name", "JHBuild",
                          "uri", "jhbuild://",
                          NULL);

      if (!dex_await (gom_resource_save (GOM_RESOURCE (sdk)), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  sdk_id = manuals_sdk_get_id (sdk);
  devhelp = manuals_devhelp_importer_new ();

  docdir = g_build_filename (jhbuild_dir, "share", "doc", NULL);
  gtkdocdir = g_build_filename (jhbuild_dir, "share", "gtk-doc", "html", NULL);

  manuals_devhelp_importer_add_directory (devhelp, docdir, sdk_id);
  manuals_devhelp_importer_add_directory (devhelp, gtkdocdir, sdk_id);

  if (!dex_await (manuals_importer_import (MANUALS_IMPORTER (devhelp),
                                           state->repository,
                                           state->progress),
                  &error))
    g_debug ("Failed to import jhbuild docs: %s", error->message);

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
manuals_jhbuild_importer_import (ManualsImporter   *importer,
                                 ManualsRepository *repository,
                                 ManualsProgress   *progress)
{
  ManualsJhbuildImporter *self = (ManualsJhbuildImporter *)importer;
  Import *state;

  g_assert (MANUALS_IS_JHBUILD_IMPORTER (self));
  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (MANUALS_IS_PROGRESS (progress));

  state = g_new0 (Import, 1);
  g_set_object (&state->self, self);
  g_set_object (&state->repository, repository);
  g_set_object (&state->progress, progress);

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (),
                              0,
                              manuals_jhbuild_importer_import_fiber,
                              state,
                              (GDestroyNotify)import_free);
}

static void
manuals_jhbuild_importer_class_init (ManualsJhbuildImporterClass *klass)
{
  ManualsImporterClass *importer_class = MANUALS_IMPORTER_CLASS (klass);

  importer_class->import = manuals_jhbuild_importer_import;
}

static void
manuals_jhbuild_importer_init (ManualsJhbuildImporter *self)
{
}

ManualsImporter *
manuals_jhbuild_importer_new (void)
{
  return g_object_new (MANUALS_TYPE_JHBUILD_IMPORTER, NULL);
}
