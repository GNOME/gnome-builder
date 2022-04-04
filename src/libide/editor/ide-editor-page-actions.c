/* ide-editor-page-actions.c
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

#define G_LOG_DOMAIN "ide-editor-page-actions"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-editor-page-private.h"

static void
ide_editor_page_actions_save_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeEditorPage) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    ide_page_report_error (IDE_PAGE (self),
                           /* translators: %s is replaced with a technical error message */
                           _("Failed to save file: %s"),
                           error->message);

  ide_page_set_progress (IDE_PAGE (self), NULL);

  IDE_EXIT;
}

static void
ide_editor_page_actions_save (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  g_autoptr(IdeNotification) notif = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_buffer_save_file_async (self->buffer,
                              NULL,
                              NULL,
                              &notif,
                              ide_editor_page_actions_save_cb,
                              g_object_ref (self));

  ide_page_set_progress (IDE_PAGE (self), notif);

  IDE_EXIT;
}

void
_ide_editor_page_class_actions_init (IdeEditorPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_install_action (widget_class, "page.save", NULL, ide_editor_page_actions_save);
}
