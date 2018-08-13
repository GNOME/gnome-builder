/* ide-editor-session-addin.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "ide-editor-session-addin.h"

#include "editor/ide-editor-session-addin.h"
#include "threading/ide-task.h"

struct _IdeEditorSessionAddin
{
  IdeObject parent_instance;
};

static void
ide_editor_session_addin_save_async (IdeSessionAddin     *addin,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GVariantDict dict;

  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_session_addin_save_async);

  g_print ("Save session editor\n");

  g_variant_dict_init (&dict, NULL);

  ide_task_return_pointer (task,
                           g_variant_take_ref (g_variant_dict_end (&dict)),
                           (GDestroyNotify)g_variant_unref);
}

static GVariant *
ide_editor_session_addin_save_finish (IdeSessionAddin  *self,
                                      GAsyncResult     *result,
                                      GError          **error)
{
  g_assert (IDE_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_editor_session_addin_restore_async (IdeSessionAddin     *addin,
                                        GVariant            *state,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_SESSION_ADDIN (addin));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_session_addin_restore_async);

  g_print ("Restore session editor: %s\n", g_variant_print (state, TRUE));

  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_editor_session_addin_restore_finish (IdeSessionAddin  *self,
                                         GAsyncResult     *result,
                                         GError          **error)
{
  g_assert (IDE_IS_EDITOR_SESSION_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
session_addin_iface_init (IdeSessionAddinInterface *iface)
{
  iface->save_async = ide_editor_session_addin_save_async;
  iface->save_finish = ide_editor_session_addin_save_finish;
  iface->restore_async = ide_editor_session_addin_restore_async;
  iface->restore_finish = ide_editor_session_addin_restore_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeEditorSessionAddin, ide_editor_session_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SESSION_ADDIN, session_addin_iface_init))

static void
ide_editor_session_addin_class_init (IdeEditorSessionAddinClass *klass)
{
}

static void
ide_editor_session_addin_init (IdeEditorSessionAddin *self)
{
}
