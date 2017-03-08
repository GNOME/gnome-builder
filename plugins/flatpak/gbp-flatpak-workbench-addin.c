/* gbp-flatpak-workbench-addin.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-workbench-addin"

#include "gbp-flatpak-workbench-addin.h"

struct _GbpFlatpakWorkbenchAddin
{
  GObject             parent;
  GSimpleActionGroup *actions;
  IdeWorkbench       *workbench;
};

static void
gbp_flatpak_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;
  IdeContext *context;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);
  if (context != NULL)
    gtk_widget_insert_action_group (GTK_WIDGET (workbench), "flatpak", G_ACTION_GROUP (self->actions));
}

static void
gbp_flatpak_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "flatpak", NULL);

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_flatpak_workbench_addin_load;
  iface->unload = gbp_flatpak_workbench_addin_unload;
}

static void
gbp_flatpak_workbench_addin_update_dependencies (GSimpleAction *action,
                                                 GVariant      *param,
                                                 gpointer       user_data)
{
  GbpFlatpakWorkbenchAddin *self = user_data;
  IdeBuildManager *manager;
  IdeBuildPipeline *pipeline;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  manager = ide_context_get_build_manager (ide_workbench_get_context (self->workbench));
  pipeline = ide_build_manager_get_pipeline (manager);
  ide_build_pipeline_invalidate_phase (pipeline, IDE_BUILD_PHASE_DOWNLOADS);
  ide_build_manager_execute_async (manager, IDE_BUILD_PHASE_DOWNLOADS, NULL, NULL, NULL);
}

static void
gbp_flatpak_workbench_addin_export_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBuildManager *manager = (IdeBuildManager *)object;
  g_autoptr(GbpFlatpakWorkbenchAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_build_manager_execute_finish (manager, result, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  /* Open the directory containing the flatpak bundle */
}

static void
gbp_flatpak_workbench_addin_export (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GbpFlatpakWorkbenchAddin *self = user_data;
  IdeBuildManager *manager;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  manager = ide_context_get_build_manager (ide_workbench_get_context (self->workbench));

  ide_build_manager_execute_async (manager,
                                   IDE_BUILD_PHASE_EXPORT,
                                   NULL,
                                   gbp_flatpak_workbench_addin_export_cb,
                                   g_object_ref (self));
}

G_DEFINE_TYPE_EXTENDED (GbpFlatpakWorkbenchAddin, gbp_flatpak_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_flatpak_workbench_addin_finalize (GObject *object)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)object;

  g_clear_object (&self->actions);

  G_OBJECT_CLASS (gbp_flatpak_workbench_addin_parent_class)->finalize (object);
}

static void
gbp_flatpak_workbench_addin_class_init (GbpFlatpakWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_workbench_addin_finalize;
}

static void
gbp_flatpak_workbench_addin_init (GbpFlatpakWorkbenchAddin *self)
{
  static const GActionEntry actions[] = {
    { "update-dependencies", gbp_flatpak_workbench_addin_update_dependencies },
    { "export", gbp_flatpak_workbench_addin_export },
  };

  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}
