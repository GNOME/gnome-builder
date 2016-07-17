/* ide-workbench-actions.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-workbench"

#include <glib/gi18n.h>

#include "ide-debug.h"

#include "application/ide-application.h"
#include "buffers/ide-buffer-manager.h"
#include "workbench/ide-workbench.h"
#include "workbench/ide-workbench-header-bar.h"
#include "workbench/ide-workbench-private.h"

static void
ide_workbench_actions_open_with_dialog_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeWorkbench *self = (IdeWorkbench *)object;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));

  if (!ide_workbench_open_files_finish (self, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  IDE_EXIT;
}

static void
ide_workbench_actions_open_with_dialog (GSimpleAction *action,
                                        GVariant      *param,
                                        gpointer       user_data)
{
  IdeWorkbench *self = user_data;
  GtkWidget *button;
  GtkWidget *dialog;
  gint ret;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));

  dialog = gtk_file_chooser_dialog_new (_("Open File"),
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Cancel"), GTK_RESPONSE_CANCEL,
                                        _("Open"), GTK_RESPONSE_OK,
                                        NULL);

  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);

  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (button),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  /*
   * TODO: Allow workbench addins to specify file filters?
   *       Do we want to move this to a custom interface and use that
   *       for file loading as well?
   */

  ret = gtk_dialog_run (GTK_DIALOG (dialog));

  if (ret == GTK_RESPONSE_OK)
    {
      g_autoptr(GFile) file = NULL;

      IDE_PROBE;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      ide_workbench_open_files_async (self,
                                      &file,
                                      1,
                                      NULL,
                                      IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                      NULL,
                                      ide_workbench_actions_open_with_dialog_cb,
                                      NULL);
    }

  gtk_widget_destroy (dialog);

  IDE_EXIT;
}

static void
ide_workbench_actions_save_all (GSimpleAction *action,
                                GVariant      *variant,
                                gpointer       user_data)
{
  IdeWorkbench *workbench = user_data;
  IdeContext *context;
  IdeBufferManager *bufmgr;

  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);
  if (context == NULL)
    return;

  bufmgr = ide_context_get_buffer_manager (context);
  ide_buffer_manager_save_all_async (bufmgr, NULL, NULL, NULL);
}

static void
save_all_quit_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeWorkbench) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (IDE_IS_WORKBENCH (self));

  if (!ide_buffer_manager_save_all_finish (bufmgr, result, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  g_application_quit (G_APPLICATION (IDE_APPLICATION_DEFAULT));
}

static void
ide_workbench_actions_save_all_quit (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeWorkbench *workbench = user_data;
  IdeContext *context;
  IdeBufferManager *bufmgr;

  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);
  if (context == NULL)
    return;

  bufmgr = ide_context_get_buffer_manager (context);
  ide_buffer_manager_save_all_async (bufmgr,
                                     NULL,
                                     save_all_quit_cb,
                                     g_object_ref (workbench));
}

static void
ide_workbench_actions_opacity (GSimpleAction *action,
                               GVariant      *variant,
                               gpointer       user_data)
{
  IdeWorkbench *workbench = user_data;
  gdouble opacity;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT32));

  opacity = CLAMP (g_variant_get_int32 (variant), 10, 100) / 100.0;
  gtk_widget_set_opacity (GTK_WIDGET (workbench), opacity);
}

static void
ide_workbench_actions_global_search (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeWorkbench *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_WORKBENCH (self));

  ide_workbench_header_bar_focus_search (self->header_bar);
}

void
ide_workbench_actions_init (IdeWorkbench *self)
{
  GPropertyAction *action;
  const GActionEntry actions[] = {
    { "global-search", ide_workbench_actions_global_search },
    { "opacity", NULL, "i", "100", ide_workbench_actions_opacity },
    { "open-with-dialog", ide_workbench_actions_open_with_dialog },
    { "save-all", ide_workbench_actions_save_all },
    { "save-all-quit", ide_workbench_actions_save_all_quit },
  };

  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);

  action = g_property_action_new ("perspective", self, "visible-perspective-name");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);
}
