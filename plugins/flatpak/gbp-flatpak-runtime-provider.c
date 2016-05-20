/* gbp-flatpak-runtime-provider.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include <string.h>
#include <flatpak.h>

#include "util/ide-posix.h"

#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-runtime-provider.h"

struct _GbpFlatpakRuntimeProvider
{
  GObject             parent_instance;
  IdeRuntimeManager  *manager;
  FlatpakInstallation *installation;
  GCancellable       *cancellable;
  GPtrArray          *runtimes;
};

static void runtime_provider_iface_init (IdeRuntimeProviderInterface *);

G_DEFINE_TYPE_EXTENDED (GbpFlatpakRuntimeProvider, gbp_flatpak_runtime_provider, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER,
                                               runtime_provider_iface_init))

static inline void
sanitize_name (gchar *name)
{
  gchar *tmp = strchr (name, '/');

  if (tmp != NULL)
    *tmp = '\0';
}

static void
gbp_flatpak_runtime_provider_load_worker (GTask        *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable)
{
  GbpFlatpakRuntimeProvider *self = source_object;
  g_autofree gchar *host_type = NULL;
  IdeContext *context;
  GPtrArray *ret;
  GPtrArray *ar;
  GError *error = NULL;
  guint i;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (self->manager));

  context = ide_object_get_context (IDE_OBJECT (self->manager));
  host_type = ide_get_system_arch ();

  self->installation = flatpak_installation_new_user (cancellable, &error);

  if (self->installation == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  ar = flatpak_installation_list_installed_refs_by_kind (self->installation,
                                                         FLATPAK_REF_KIND_RUNTIME,
                                                         cancellable,
                                                         &error);

  if (ar == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < ar->len; i++)
    {
      FlatpakInstalledRef *ref = g_ptr_array_index (ar, i);
      g_autofree gchar *str = NULL;
      g_autofree gchar *id = NULL;
      g_autofree gchar *name = NULL;
      const gchar *arch;
      const gchar *branch;
      g_autoptr(GBytes) metadata = NULL;
      g_autofree gchar *sdk = NULL;
      g_autoptr(GKeyFile) key_file = NULL;
      const gchar *metadata_data;
      gsize metadata_len;

      g_assert (FLATPAK_IS_INSTALLED_REF (ref));

      name = g_strdup (flatpak_ref_get_name (FLATPAK_REF (ref)));

      sanitize_name (name);

      arch = flatpak_ref_get_arch (FLATPAK_REF (ref));
      branch = flatpak_ref_get_branch (FLATPAK_REF (ref));

      id = g_strdup_printf ("flatpak-app:%s/%s/%s", name, branch, arch);

      if (g_strcmp0 (host_type, arch) == 0)
        str = g_strdup_printf ("%s <b>%s</b>", name, branch);
      else
        str = g_strdup_printf ("%s <b>%s</b> <sup>%s</sup>", name, branch, arch);

      metadata = flatpak_installed_ref_load_metadata (FLATPAK_INSTALLED_REF (ref),
                                                      cancellable, &error);

      if (metadata == NULL)
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
          continue;
        }

      metadata_data = g_bytes_get_data (metadata, &metadata_len);

      key_file = g_key_file_new ();

      if (!g_key_file_load_from_data (key_file, metadata_data, metadata_len, G_KEY_FILE_NONE, &error))
        {
          /*
           * If this is not really a runtime, but something like a locale, then
           * the metadata file will not exist.
           */
          if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            {
              g_clear_error (&error);
              continue;
            }

          g_warning ("%s", error->message);
          g_clear_error (&error);
          continue;
        }

      if (!(sdk = g_key_file_get_string (key_file, "Runtime", "sdk", NULL)))
        sdk = g_strdup (name);

      sanitize_name (sdk);

      g_ptr_array_add (ret,
                       g_object_new (GBP_TYPE_FLATPAK_RUNTIME,
                                     "branch", branch,
                                     "sdk", sdk,
                                     "platform", name,
                                     "context", context,
                                     "id", id,
                                     "display-name", str,
                                     NULL));
    }

  g_ptr_array_unref (ar);

  g_task_return_pointer (task, ret, (GDestroyNotify)g_ptr_array_unref);
}

static void
gbp_flatpak_runtime_provider_load_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)object;
  GPtrArray *ret;
  GError *error = NULL;
  guint i;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (G_IS_TASK (result));

  if (!(ret = g_task_propagate_pointer (G_TASK (result), &error)))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  for (i = 0; i < ret->len; i++)
    {
      IdeRuntime *runtime = g_ptr_array_index (ret, i);

      ide_runtime_manager_add (self->manager, runtime);
    }

  self->runtimes = ret;
}

static void
gbp_flatpak_runtime_provider_load (IdeRuntimeProvider *provider,
                               IdeRuntimeManager  *manager)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  ide_set_weak_pointer (&self->manager, manager);

  self->cancellable = g_cancellable_new ();

  task = g_task_new (self, self->cancellable, gbp_flatpak_runtime_provider_load_cb, NULL);
  g_task_run_in_thread (task, gbp_flatpak_runtime_provider_load_worker);
}

static void
gbp_flatpak_runtime_provider_unload (IdeRuntimeProvider *provider,
                                 IdeRuntimeManager  *manager)
{
  GbpFlatpakRuntimeProvider *self = (GbpFlatpakRuntimeProvider *)provider;

  g_assert (GBP_IS_FLATPAK_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  if (self->runtimes != NULL)
    {
      for (guint i= 0; i < self->runtimes->len; i++)
        {
          IdeRuntime *runtime = g_ptr_array_index (self->runtimes, i);

          ide_runtime_manager_remove (manager, runtime);
        }
    }

  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->installation);

  ide_clear_weak_pointer (&self->manager);
}

static void
gbp_flatpak_runtime_provider_class_init (GbpFlatpakRuntimeProviderClass *klass)
{
}

static void
gbp_flatpak_runtime_provider_init (GbpFlatpakRuntimeProvider *self)
{
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_flatpak_runtime_provider_load;
  iface->unload = gbp_flatpak_runtime_provider_unload;
}
