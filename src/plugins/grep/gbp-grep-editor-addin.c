/* gbp-grep-editor-addin.c
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-grep-editor-addin"

#include "gbp-grep-dialog.h"
#include "gbp-grep-editor-addin.h"
#include "gbp-grep-model.h"
#include "gbp-grep-panel.h"

struct _GbpGrepEditorAddin
{
  GObject               parent_instance;
  IdeEditorPerspective *editor;
};

static void
model_scan_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
  GbpGrepModel *model = (GbpGrepModel *)object;
  g_autoptr(GbpGrepPanel) panel = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GREP_MODEL (model));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_GREP_PANEL (panel));

  if (!gbp_grep_model_scan_finish (model, result, &error))
    return;

  gbp_grep_panel_set_model (panel, model);
}

static void
gbp_grep_editor_addin_find_in_files (GSimpleAction *action,
                                     GVariant      *param,
                                     gpointer       user_data)
{
  GbpGrepEditorAddin *self = user_data;
  g_autoptr(GbpGrepModel) model = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  IdeWorkbench *workbench;
  IdeContext *context;
  GtkWidget *panel;
  GtkWidget *utils;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GREP_EDITOR_ADDIN (self));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self->editor));
  context = ide_workbench_get_context (workbench);
  model = gbp_grep_model_new (context);
  cancellable = g_cancellable_new ();

  /* TODO: Show a popover, etc ... */
  gbp_grep_model_set_recursive (model, TRUE);
  gbp_grep_model_set_query (model, "tunes");

  panel = gbp_grep_panel_new ();
  utils = ide_editor_perspective_get_utilities (self->editor);
  gtk_container_add (GTK_CONTAINER (utils), panel);
  gtk_widget_show (panel);

  ide_workbench_focus (workbench, panel);

  gbp_grep_model_scan_async (model,
                             cancellable,
                             model_scan_cb,
                             g_object_ref (panel));
}

static void
gbp_grep_editor_addin_load (IdeEditorAddin       *addin,
                            IdeEditorPerspective *editor)
{
  GbpGrepEditorAddin *self = (GbpGrepEditorAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  static const GActionEntry actions[] = {
    { "find-in-files", gbp_grep_editor_addin_find_in_files },
  };

  g_assert (GBP_IS_GREP_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  self->editor = editor;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (editor), "grep", G_ACTION_GROUP (group));
}

static void
gbp_grep_editor_addin_unload (IdeEditorAddin       *addin,
                              IdeEditorPerspective *editor)
{
  GbpGrepEditorAddin *self = (GbpGrepEditorAddin *)addin;

  g_assert (GBP_IS_GREP_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  gtk_widget_insert_action_group (GTK_WIDGET (editor), "grep", NULL);

  self->editor = NULL;
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = gbp_grep_editor_addin_load;
  iface->unload = gbp_grep_editor_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpGrepEditorAddin, gbp_grep_editor_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN, editor_addin_iface_init))

static void
gbp_grep_editor_addin_class_init (GbpGrepEditorAddinClass *klass)
{
}

static void
gbp_grep_editor_addin_init (GbpGrepEditorAddin *self)
{
}
