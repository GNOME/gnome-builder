/* ide-editor-save-delegate.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-save-delegate"

#include "config.h"

#include <libide-threading.h>

#include "ide-editor-save-delegate.h"

struct _IdeEditorSaveDelegate
{
  PanelSaveDelegate  parent_instance;
  IdeBuffer         *buffer;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeEditorSaveDelegate, ide_editor_save_delegate, PANEL_TYPE_SAVE_DELEGATE)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_save_delegate_save_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_editor_save_delegate_save_async (PanelSaveDelegate   *delegate,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  IdeEditorSaveDelegate *self = (IdeEditorSaveDelegate *)delegate;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_SAVE_DELEGATE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_save_delegate_save_async);

  ide_buffer_save_file_async (self->buffer,
                              NULL,
                              NULL,
                              &notif,
                              ide_editor_save_delegate_save_cb,
                              g_steal_pointer (&task));

  g_object_bind_property (notif, "progress", self, "progress", G_BINDING_SYNC_CREATE);

  IDE_EXIT;
}

static gboolean
ide_editor_save_delegate_save_finish (PanelSaveDelegate  *delegate,
                                      GAsyncResult       *result,
                                      GError            **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_SAVE_DELEGATE (delegate));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_editor_save_delegate_dispose (GObject *object)
{
  IdeEditorSaveDelegate *self = (IdeEditorSaveDelegate *)object;

  g_clear_object (&self->buffer);

  G_OBJECT_CLASS (ide_editor_save_delegate_parent_class)->dispose (object);
}

static void
ide_editor_save_delegate_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeEditorSaveDelegate *self = IDE_EDITOR_SAVE_DELEGATE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, self->buffer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_save_delegate_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeEditorSaveDelegate *self = IDE_EDITOR_SAVE_DELEGATE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      self->buffer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_save_delegate_class_init (IdeEditorSaveDelegateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  PanelSaveDelegateClass *save_delegate_class = PANEL_SAVE_DELEGATE_CLASS (klass);

  object_class->dispose = ide_editor_save_delegate_dispose;
  object_class->get_property = ide_editor_save_delegate_get_property;
  object_class->set_property = ide_editor_save_delegate_set_property;

  save_delegate_class->save_async = ide_editor_save_delegate_save_async;
  save_delegate_class->save_finish = ide_editor_save_delegate_save_finish;

  properties[PROP_BUFFER] =
    g_param_spec_object ("buffer", NULL, NULL,
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_editor_save_delegate_init (IdeEditorSaveDelegate *self)
{
}

PanelSaveDelegate *
ide_editor_save_delegate_new (IdeEditorPage *page)
{
  IdeEditorSaveDelegate *ret;
  IdeBuffer *buffer;

  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (page), NULL);

  buffer = ide_editor_page_get_buffer (page);

  ret = g_object_new (IDE_TYPE_EDITOR_SAVE_DELEGATE,
                      "buffer", buffer,
                      NULL);

  g_object_bind_property (page, "title", ret, "title", G_BINDING_SYNC_CREATE);
  g_object_bind_property (page, "icon", ret, "icon", G_BINDING_SYNC_CREATE);

  return PANEL_SAVE_DELEGATE (ret);
}
