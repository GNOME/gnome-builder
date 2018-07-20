/* ide-gi-service.c
 *
 * Copyright (C) 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-gi-service"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-gi-service.h"

struct _IdeGiService
{
  IdeObject         parent_instance;

  IdeGiRepository  *repository;
  IdePausable      *pausable;
  GCancellable     *cancellable;

  guint             stopped : 1;
};

static void service_iface_init (IdeServiceInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGiService, ide_gi_service, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_SERVICE, service_iface_init))

/* TODO: finish the pausable system */
G_GNUC_UNUSED static void
register_pausable (IdeGiService *self)
{
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_GI_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (context == NULL || IDE_IS_CONTEXT (context));

  if (context != NULL && self->pausable != NULL)
    ide_context_add_pausable (context, self->pausable);
}

static void
unregister_pausable (IdeGiService *self)
{
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_GI_SERVICE (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (context == NULL || IDE_IS_CONTEXT (context));

  if (context != NULL && self->pausable != NULL)
    ide_context_remove_pausable (context, self->pausable);
}

static void
ide_gi_service_paused (IdeGiService *self,
                       IdePausable  *pausable)
{
  g_assert (IDE_IS_GI_SERVICE (self));
  g_assert (IDE_IS_PAUSABLE (pausable));

  if (self->stopped)
    return;

  g_cancellable_cancel (self->cancellable);
}

static void
ide_gi_service_unpaused (IdeGiService *self,
                         IdePausable  *pausable)
{
  g_assert (IDE_IS_GI_SERVICE (self));
  g_assert (IDE_IS_PAUSABLE (pausable));

  if (self->stopped)
    return;

  /* TODO: trigger another update */
}

static void
ide_gi_service_cache_dir_reaped_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  DzlDirectoryReaper *reaper = (DzlDirectoryReaper *)object;
  g_autoptr(IdeGiService) self = (IdeGiService *)user_data;
  IdeContext *context;
  g_autoptr(GError) error = NULL;

  g_assert (DZL_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_ASYNC_RESULT (result));

  context = ide_object_get_context (IDE_OBJECT (self));
  if (!dzl_directory_reaper_execute_finish (reaper, result, &error))
    g_warning ("Failed to reap old GI data: %s", error->message);

  self->repository = g_object_new (IDE_TYPE_GI_REPOSITORY,
                                   "context", context,
                                   "update-on-build", TRUE,
                                   NULL);
  self->stopped = FALSE;
}

static void
ide_gi_service_context_loaded (IdeService *service)
{
  IdeContext *context;
  IdeGiService *self = (IdeGiService *)service;
  g_autoptr(DzlDirectoryReaper) reaper = NULL;
  g_autofree gchar *cache_path = NULL;
  g_autoptr(GFile) dir = NULL;

  g_assert (IDE_IS_GI_SERVICE (self));

  IDE_ENTRY;

  context = ide_object_get_context (IDE_OBJECT (self));
  cache_path = ide_context_cache_filename (context, "gi", NULL);
  dir = g_file_new_for_path (cache_path);

  reaper = dzl_directory_reaper_new ();
  dzl_directory_reaper_add_directory (reaper, dir, 0);
  dzl_directory_reaper_execute_async (reaper,
                                      NULL,
                                      ide_gi_service_cache_dir_reaped_cb,
                                      g_object_ref (self));

  IDE_EXIT;
}

static void
ide_gi_service_start (IdeService *service)
{
  ;
}

static void
ide_gi_service_stop (IdeService *service)
{
  IdeGiService *self = (IdeGiService *)service;

  g_assert (IDE_IS_GI_SERVICE (self));

  IDE_ENTRY;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->stopped = TRUE;

  g_clear_object (&self->repository);

  unregister_pausable (self);

  IDE_EXIT;
}

static void
ide_gi_service_finalize (GObject *object)
{
  IdeGiService *self = (IdeGiService *)object;

  g_assert (self->stopped == TRUE);
  g_assert (self->repository == NULL);

  g_clear_object (&self->pausable);

  G_OBJECT_CLASS (ide_gi_service_parent_class)->finalize (object);
}

static void
ide_gi_service_class_init (IdeGiServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_service_finalize;
}

static void
service_iface_init (IdeServiceInterface *iface)
{
  iface->context_loaded = ide_gi_service_context_loaded;
  iface->start = ide_gi_service_start;
  iface->stop = ide_gi_service_stop;
}

static void
ide_gi_service_init (IdeGiService *self)
{
  self->pausable = g_object_new (IDE_TYPE_PAUSABLE,
                                 "paused", FALSE,
                                 "title", _("GIR Indexer"),
                                 "subtitle", _("Hovering, diagnostics and autocompletion may be limited until complete."),
                                 NULL);

  g_signal_connect_object (self->pausable,
                           "paused",
                           G_CALLBACK (ide_gi_service_paused),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->pausable,
                           "unpaused",
                           G_CALLBACK (ide_gi_service_unpaused),
                           self,
                           G_CONNECT_SWAPPED);
}

IdeGiRepository *
ide_gi_service_get_repository (IdeGiService *self)
{
  g_return_val_if_fail (IDE_IS_GI_SERVICE (self), NULL);

  if (self->repository == NULL)
    g_warning ("Context not loaded yet\n");

  return self->repository;
}
