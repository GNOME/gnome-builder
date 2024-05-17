/*
 * manuals-flatpak-importer.c
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

#include "manuals-book.h"
#include "manuals-devhelp-importer.h"
#include "manuals-flatpak.h"
#include "manuals-flatpak-importer.h"
#include "manuals-gom.h"
#include "manuals-sdk.h"

#include "../flatpak/gbp-flatpak-util.h"

struct _ManualsFlatpakImporter
{
  ManualsImporter parent_instance;
};

G_DEFINE_FINAL_TYPE (ManualsFlatpakImporter, manuals_flatpak_importer, MANUALS_TYPE_IMPORTER)

static const char *suffixes[] = {
  "files/doc/",
  "files/gtk-doc/html",
  NULL
};

typedef struct _ImportInstallations
{
  ManualsRepository *repository;
  ManualsProgress *progress;
} ImportInstallations;

static void
import_installations_free (gpointer data)
{
  ImportInstallations *import = data;

  g_clear_object (&import->repository);
  g_clear_object (&import->progress);
  g_free (import);
}

static char *
rewrite_uri (ManualsFlatpakRuntime *runtime,
             char                  *uri)
{
  const char *beginptr;

  g_assert (MANUALS_IS_FLATPAK_RUNTIME (runtime));
  g_assert (uri != NULL);

  if (!g_str_has_suffix (uri, "/active") && (beginptr = strrchr (uri, '/')))
    {
      GString *str = g_string_new (uri);

      g_string_truncate (str, beginptr - uri);
      g_string_append (str, "/active");

      g_free (uri);

      return g_string_free (str, FALSE);
    }

  return uri;
}

static DexFuture *
find_or_create_sdk_for_runtime (ManualsRepository     *repository,
                                ManualsFlatpakRuntime *runtime)
{
  g_autoptr(ManualsSdk) sdk = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree char *uri = NULL;
  const char *deploy_dir;

  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (MANUALS_IS_FLATPAK_RUNTIME (runtime));

  deploy_dir = manuals_flatpak_runtime_get_deploy_dir (runtime);
  file = g_file_new_for_path (deploy_dir);
  uri = rewrite_uri (runtime, g_file_get_uri (file));

  sdk = dex_await_object (manuals_repository_find_sdk (repository, uri), NULL);

  if (sdk != NULL)
    return dex_future_new_take_object (g_steal_pointer (&sdk));

  sdk = g_object_new (MANUALS_TYPE_SDK,
                      "repository", repository,
                      "kind", "flatpak",
                      "uri", uri,
                      "name", manuals_flatpak_runtime_get_name (runtime),
                      "version", manuals_flatpak_runtime_get_branch (runtime),
                      NULL);

  if (dex_await (gom_resource_save (GOM_RESOURCE (sdk)), &error))
    return dex_future_new_take_object (g_steal_pointer (&sdk));
  else
    return dex_future_new_for_error (g_steal_pointer (&error));
}

static DexFuture *
delete_sdk_if_unused_fiber (gpointer user_data)
{
  ManualsSdk *sdk = user_data;
  g_autoptr(ManualsRepository) repository = NULL;
  g_autoptr(GomFilter) filter = NULL;
  g_auto(GValue) value = G_VALUE_INIT;
  guint count;

  g_assert (MANUALS_IS_SDK (sdk));

  g_object_get (sdk, "repository", &repository, NULL);

  g_value_init (&value, G_TYPE_UINT);
  g_value_set_uint (&value, manuals_sdk_get_id (sdk));

  filter = gom_filter_new_eq (MANUALS_TYPE_BOOK, "sdk-id", &value);

  if (!(count = dex_await_uint (manuals_repository_count (repository,
                                                          MANUALS_TYPE_BOOK,
                                                          filter),
                                NULL)))
    {
      g_autoptr(GomFilter) id_filter = gom_filter_new_eq (MANUALS_TYPE_SDK, "id", &value);
      dex_await (manuals_repository_delete (repository, MANUALS_TYPE_SDK, id_filter), NULL);
    }

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
delete_sdk_if_unused (DexFuture *completed,
                      gpointer   user_data)
{
  ManualsSdk *sdk = user_data;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (MANUALS_IS_SDK (sdk));

  return dex_scheduler_spawn (NULL, 0,
                              delete_sdk_if_unused_fiber,
                              g_object_ref (sdk),
                              g_object_unref);
}

static DexFuture *
manuals_flatpak_importer_import_fiber (gpointer user_data)
{
  ImportInstallations *import = user_data;
  g_autoptr(GPtrArray) installations = NULL;
  g_autoptr(GPtrArray) futures = NULL;
  g_autoptr(GListModel) runtimes = NULL;
  g_autoptr(GError) error = NULL;
  const char *default_arch;
  guint n_runtimes;

  g_assert (import != NULL);
  g_assert (MANUALS_IS_REPOSITORY (import->repository));
  g_assert (MANUALS_IS_PROGRESS (import->progress));

  default_arch = gbp_flatpak_get_default_arch ();

  if (!(runtimes = dex_await_object (manuals_flatpak_list_runtimes (), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  n_runtimes = g_list_model_get_n_items (runtimes);
  futures = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < n_runtimes; i++)
    {
      g_autoptr(ManualsFlatpakRuntime) runtime = g_list_model_get_item (runtimes, i);
      const char *name = manuals_flatpak_runtime_get_name (runtime);
      const char *arch = manuals_flatpak_runtime_get_arch (runtime);
      const char *deploy_dir = manuals_flatpak_runtime_get_deploy_dir (runtime);
      g_autoptr(ManualsDevhelpImporter) devhelp = NULL;
      g_autoptr(ManualsSdk) sdk = NULL;
      g_autoptr(GFile) file = g_file_new_for_path (deploy_dir);
      g_autofree char *uri = rewrite_uri (runtime, g_file_get_uri (file));
      g_autoptr(GFile) active_file = g_file_new_for_uri (uri);
      g_autofree char *doc_dir = NULL;
      g_autofree char *gtk_doc_dir = NULL;
      gint64 sdk_id;

      if (g_strcmp0 (arch, default_arch) != 0)
        continue;

      /* Only try to import runtimes that end in .Docs such as
       * org.gnome.Sdk.Docs until this sort of convention changes
       * in various runtime/sdks.
       */
      if (!g_str_has_suffix (name, ".Docs"))
        continue;

      devhelp = manuals_devhelp_importer_new ();

      for (guint k = 0; suffixes[k]; k++)
        {
          g_autoptr(GFile) dir = g_file_get_child (active_file, suffixes[k]);

          if (g_file_query_file_type (dir, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY)
            manuals_devhelp_importer_add_directory (devhelp, g_file_peek_path (dir), 0);
        }

      if (manuals_devhelp_importer_get_size (devhelp) == 0)
        continue;

      if (!(sdk = dex_await_object (find_or_create_sdk_for_runtime (import->repository, runtime), NULL)))
        continue;

      sdk_id = manuals_sdk_get_id (sdk);
      manuals_devhelp_importer_set_sdk_id (devhelp, sdk_id);

      g_ptr_array_add (futures,
                       dex_future_finally (manuals_importer_import (MANUALS_IMPORTER (devhelp),
                                                                    import->repository,
                                                                    import->progress),
                                           delete_sdk_if_unused,
                                           g_object_ref (sdk),
                                           g_object_unref));
    }

  if (futures->len > 0)
    dex_await (dex_future_allv ((DexFuture **)futures->pdata, futures->len), NULL);

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
manuals_flatpak_importer_import (ManualsImporter   *importer,
                                 ManualsRepository *repository,
                                 ManualsProgress   *progress)
{
  ImportInstallations *import;

  g_assert (MANUALS_IS_FLATPAK_IMPORTER (importer));
  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (MANUALS_IS_PROGRESS (progress));

  import = g_new0 (ImportInstallations, 1);
  import->repository = g_object_ref (repository);
  import->progress = g_object_ref (progress);

  return dex_scheduler_spawn (NULL, 0,
                              manuals_flatpak_importer_import_fiber,
                              import,
                              import_installations_free);
}

static void
manuals_flatpak_importer_finalize (GObject *object)
{
  G_OBJECT_CLASS (manuals_flatpak_importer_parent_class)->finalize (object);
}

static void
manuals_flatpak_importer_class_init (ManualsFlatpakImporterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ManualsImporterClass *importer_class = MANUALS_IMPORTER_CLASS (klass);

  object_class->finalize = manuals_flatpak_importer_finalize;

  importer_class->import = manuals_flatpak_importer_import;
}

static void
manuals_flatpak_importer_init (ManualsFlatpakImporter *self)
{
}

ManualsImporter *
manuals_flatpak_importer_new (void)
{
  return g_object_new (MANUALS_TYPE_FLATPAK_IMPORTER, NULL);
}
