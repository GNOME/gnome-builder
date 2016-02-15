/* gbp-xdg-runtime-provider.c
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
#include <xdg-app.h>

#include "util/ide-posix.h"

#include "gbp-xdg-runtime.h"
#include "gbp-xdg-runtime-provider.h"

struct _GbpXdgRuntimeProvider
{
  GObject             parent_instance;
  IdeRuntimeManager  *manager;
  XdgAppInstallation *installation;
  GCancellable       *cancellable;
  GPtrArray          *runtimes;
};

static void runtime_provider_iface_init (IdeRuntimeProviderInterface *);

G_DEFINE_TYPE_EXTENDED (GbpXdgRuntimeProvider, gbp_xdg_runtime_provider, G_TYPE_OBJECT, 0,
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
gbp_xdg_runtime_provider_load_worker (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  GbpXdgRuntimeProvider *self = source_object;
  g_autofree gchar *host_type = NULL;
  IdeContext *context;
  GPtrArray *ret;
  GPtrArray *ar;
  GError *error = NULL;
  guint i;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_XDG_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (self->manager));

  context = ide_object_get_context (IDE_OBJECT (self->manager));
  host_type = ide_get_system_arch ();

  self->installation = xdg_app_installation_new_user (cancellable, &error);

  if (self->installation == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  ar = xdg_app_installation_list_installed_refs_by_kind (self->installation,
                                                         XDG_APP_REF_KIND_RUNTIME,
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
      XdgAppInstalledRef *ref = g_ptr_array_index (ar, i);
      g_autofree gchar *str = NULL;
      g_autofree gchar *id = NULL;
      g_autofree gchar *name = NULL;
      const gchar *arch;
      const gchar *branch;
      g_autofree gchar *metadata = NULL;
      g_autofree gchar *sdk = NULL;
      g_autoptr(GKeyFile) key_file = NULL;

      g_assert (XDG_APP_IS_INSTALLED_REF (ref));

      name = g_strdup (xdg_app_ref_get_name (XDG_APP_REF (ref)));

      sanitize_name (name);

      arch = xdg_app_ref_get_arch (XDG_APP_REF (ref));
      branch = xdg_app_ref_get_branch (XDG_APP_REF (ref));

      id = g_strdup_printf ("xdg-app:%s/%s/%s", name, branch, arch);

      if (g_strcmp0 (host_type, arch) == 0)
        str = g_strdup_printf ("%s <b>%s</b>", name, branch);
      else
        str = g_strdup_printf ("%s <b>%s</b> <sup>%s</sup>", name, branch, arch);

      metadata = xdg_app_installed_ref_load_metadata (XDG_APP_INSTALLED_REF (ref),
                                                      cancellable, &error);

      if (metadata == NULL)
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
          continue;
        }

      key_file = g_key_file_new ();

      if (!g_key_file_load_from_data (key_file, metadata, -1, G_KEY_FILE_NONE, &error))
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
                       g_object_new (GBP_TYPE_XDG_RUNTIME,
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
gbp_xdg_runtime_provider_load_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbpXdgRuntimeProvider *self = (GbpXdgRuntimeProvider *)object;
  GPtrArray *ret;
  GError *error = NULL;
  guint i;

  g_assert (GBP_IS_XDG_RUNTIME_PROVIDER (self));
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
gbp_xdg_runtime_provider_load (IdeRuntimeProvider *provider,
                               IdeRuntimeManager  *manager)
{
  GbpXdgRuntimeProvider *self = (GbpXdgRuntimeProvider *)provider;
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_XDG_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  ide_set_weak_pointer (&self->manager, manager);

  self->cancellable = g_cancellable_new ();

  task = g_task_new (self, self->cancellable, gbp_xdg_runtime_provider_load_cb, NULL);
  g_task_run_in_thread (task, gbp_xdg_runtime_provider_load_worker);
}

static void
gbp_xdg_runtime_provider_unload (IdeRuntimeProvider *provider,
                                 IdeRuntimeManager  *manager)
{
  GbpXdgRuntimeProvider *self = (GbpXdgRuntimeProvider *)provider;

  g_assert (GBP_IS_XDG_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);

  ide_clear_weak_pointer (&self->manager);
  g_clear_object (&self->cancellable);
}

static void
gbp_xdg_runtime_provider_class_init (GbpXdgRuntimeProviderClass *klass)
{
}

static void
gbp_xdg_runtime_provider_init (GbpXdgRuntimeProvider *self)
{
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_xdg_runtime_provider_load;
  iface->unload = gbp_xdg_runtime_provider_unload;
}
