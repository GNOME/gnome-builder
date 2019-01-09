/* ide-session.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-session"

#include "config.h"

#include <libpeas/peas.h>
#include <libide-plugins.h>
#include <libide-threading.h>

#include "ide-session-addin.h"
#include "ide-session-private.h"

struct _IdeSession
{
  IdeObject               parent_instance;
  IdeExtensionSetAdapter *addins;
};

typedef struct
{
  GPtrArray    *addins;
  GVariantDict  dict;
  gint          active;
  guint         dict_needs_clear : 1;
} Save;

typedef struct
{
  IdeWorkbench *workbench;
  GPtrArray    *addins;
  GVariant     *state;
  gint          active;
} Restore;

G_DEFINE_TYPE (IdeSession, ide_session, IDE_TYPE_OBJECT)

static void
restore_free (Restore *r)
{
  g_assert (r != NULL);

  g_clear_pointer (&r->addins, g_ptr_array_unref);
  g_clear_pointer (&r->state, g_variant_unref);
  g_clear_object (&r->workbench);
  g_slice_free (Restore, r);
}

static void
save_free (Save *s)
{
  g_assert (s != NULL);
  g_assert (s->active == 0);

  if (s->dict_needs_clear)
    g_variant_dict_clear (&s->dict);

  g_clear_pointer (&s->addins, g_ptr_array_unref);
  g_slice_free (Save, s);
}

static void
collect_addins_cb (IdeExtensionSetAdapter *set,
                   PeasPluginInfo         *plugin_info,
                   PeasExtension          *exten,
                   gpointer                user_data)
{
  GPtrArray *ar = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_SESSION_ADDIN (exten));
  g_assert (ar != NULL);

  g_ptr_array_add (ar, g_object_ref (exten));
}

static void
ide_session_destroy (IdeObject *object)
{
  IdeSession *self = (IdeSession *)object;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SESSION (self));

  ide_clear_and_destroy_object (&self->addins);

  IDE_OBJECT_CLASS (ide_session_parent_class)->destroy (object);

  IDE_EXIT;
}

static void
ide_session_parent_set (IdeObject *object,
                        IdeObject *parent)
{
  IdeSession *self = (IdeSession *)object;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SESSION (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent == NULL)
    return;

  self->addins = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                peas_engine_get_default (),
                                                IDE_TYPE_SESSION_ADDIN,
                                                NULL, NULL);
}

static void
ide_session_class_init (IdeSessionClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = ide_session_destroy;
  i_object_class->parent_set = ide_session_parent_set;
}

static void
ide_session_init (IdeSession *self)
{
}

static void
ide_session_restore_addin_restore_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeSessionAddin *addin = (IdeSessionAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Restore *r;

  IDE_ENTRY;

  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  r = ide_task_get_task_data (task);

  g_assert (r != NULL);
  g_assert (r->addins != NULL);
  g_assert (r->active > 0);
  g_assert (r->state != NULL);

  if (!ide_session_addin_restore_finish (addin, result, &error))
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (addin), error->message);

  r->active--;

  if (r->active == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_session_restore_load_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) bytes = NULL;
  GCancellable *cancellable;
  Restore *r;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  r = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (r != NULL);
  g_assert (r->addins != NULL);
  g_assert (r->active > 0);
  g_assert (r->state == NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(bytes = g_file_load_bytes_finish (file, result, NULL, &error)))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        ide_task_return_boolean (task, TRUE);
      else
        ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (g_bytes_get_size (bytes) == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  r->state = g_variant_new_from_bytes (G_VARIANT_TYPE_VARDICT, bytes, FALSE);

  if (r->state == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Failed to decode session state");
      IDE_EXIT;
    }

  g_assert (r->addins != NULL);
  g_assert (r->addins->len > 0);

  for (guint i = 0; i < r->addins->len; i++)
    {
      IdeSessionAddin *addin = g_ptr_array_index (r->addins, i);
      g_autoptr(GVariant) state = NULL;

      g_assert (IDE_IS_SESSION_ADDIN (addin));

      state = g_variant_lookup_value (r->state,
                                      G_OBJECT_TYPE_NAME (addin),
                                      NULL);

      ide_session_addin_restore_async (addin,
                                       r->workbench,
                                       state,
                                       cancellable,
                                       ide_session_restore_addin_restore_cb,
                                       g_object_ref (task));
    }

  IDE_EXIT;
}

/**
 * ide_session_restore_async:
 * @self: an #IdeSession
 * @workbench: an #IdeWorkbench
 * @cancellable: (nullable): a #GCancellbale or %NULL
 * @callback: the callback to execute upon completion
 * @user_data: user data for callback
 *
 * This function will asynchronously restore the state of the project to
 * the point it was last saved (typically upon shutdown). This includes
 * open documents and editor splits to the degree possible.
 *
 * Since: 3.30
 */
void
ide_session_restore_async (IdeSession          *self,
                           IdeWorkbench        *workbench,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  IdeContext *context;
  Restore *r;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SESSION (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_session_restore_async);

  r = g_slice_new0 (Restore);
  r->workbench = g_object_ref (workbench);
  r->addins = g_ptr_array_new_with_free_func (g_object_unref);
  ide_extension_set_adapter_foreach (self->addins, collect_addins_cb, r->addins);
  r->active = r->addins->len;
  ide_task_set_task_data (task, r, restore_free);

  if (r->active == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  file = ide_context_cache_file (context, "session.gvariant", NULL);

  g_file_load_bytes_async (file,
                           cancellable,
                           ide_session_restore_load_cb,
                           g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_session_restore_finish (IdeSession    *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SESSION (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_session_save_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_session_save_addin_save_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeSessionAddin *addin = (IdeSessionAddin *)object;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeSession *self;
  Save *s;

  IDE_ENTRY;

  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  s = ide_task_get_task_data (task);

  g_assert (IDE_IS_SESSION (self));
  g_assert (s != NULL);
  g_assert (s->addins != NULL);
  g_assert (s->active > 0);

  variant = ide_session_addin_save_finish (addin, result, &error);

  if (error != NULL)
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (addin), error->message);

  if (variant != NULL)
    {
      g_assert (!g_variant_is_floating (variant));

      s->dict_needs_clear = TRUE;
      g_variant_dict_insert_value (&s->dict, G_OBJECT_TYPE_NAME (addin), variant);
    }

  s->active--;

  if (s->active == 0)
    {
      g_autoptr(GVariant) state = NULL;
      g_autoptr(GBytes) bytes = NULL;
      g_autoptr(GFile) file = NULL;
      GCancellable *cancellable;
      IdeContext *context;

      s->dict_needs_clear = FALSE;

      state = g_variant_take_ref (g_variant_dict_end (&s->dict));
      bytes = g_variant_get_data_as_bytes (state);

      cancellable = ide_task_get_cancellable (task);
      context = ide_object_get_context (IDE_OBJECT (self));
      file = ide_context_cache_file (context, "session.gvariant", NULL);

      if (ide_task_return_error_if_cancelled (task))
        IDE_EXIT;

      g_file_replace_contents_bytes_async (file,
                                           bytes,
                                           NULL,
                                           FALSE,
                                           G_FILE_CREATE_NONE,
                                           cancellable,
                                           ide_session_save_cb,
                                           g_steal_pointer (&task));
    }

  IDE_EXIT;
}

/**
 * ide_session_save_async:
 * @self: an #IdeSession
 * @workbench: an #IdeWorkbench
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: user data for @callback
 *
 * This function will request that various components save their active state
 * so that the project may be restored to the current layout when the project
 * is re-opened at a later time.
 *
 * Since: 3.30
 */
void
ide_session_save_async (IdeSession          *self,
                        IdeWorkbench        *workbench,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  Save *s;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SESSION (self));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_session_save_async);

  s = g_slice_new0 (Save);
  s->addins = g_ptr_array_new_with_free_func (g_object_unref);
  ide_extension_set_adapter_foreach (self->addins, collect_addins_cb, s->addins);
  s->active = s->addins->len;
  g_variant_dict_init (&s->dict, NULL);
  s->dict_needs_clear = TRUE;
  ide_task_set_task_data (task, s, save_free);

  if (s->active == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  for (guint i = 0; i < s->addins->len; i++)
    {
      IdeSessionAddin *addin = g_ptr_array_index (s->addins, i);

      ide_session_addin_save_async (addin,
                                    workbench,
                                    cancellable,
                                    ide_session_save_addin_save_cb,
                                    g_object_ref (task));
    }

  g_assert (s != NULL);
  g_assert (s->active > 0);
  g_assert (s->addins->len == s->active);

  IDE_EXIT;
}

gboolean
ide_session_save_finish (IdeSession    *self,
                         GAsyncResult  *result,
                         GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SESSION (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

IdeSession *
ide_session_new (void)
{
  return g_object_new (IDE_TYPE_SESSION, NULL);
}
