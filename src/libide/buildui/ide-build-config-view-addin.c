/* ide-build-config-view-addin.c
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#define G_LOG_DOMAIN "ide-build-config-view-addin"

#include <glib/gi18n.h>

#include "ide-context.h"

#include "buildsystem/ide-build-system.h"
#include "config/ide-config-view-addin.h"
#include "buildui/ide-build-config-view-addin.h"
#include "runtimes/ide-runtime-manager.h"
#include "threading/ide-task.h"

struct _IdeBuildConfigViewAddin
{
  IdeObject parent_instance;
};

static void
add_description_row (DzlPreferences *preferences,
                     const gchar    *page,
                     const gchar    *group,
                     const gchar    *title,
                     const gchar    *value)
{
  GtkWidget *widget;

  g_assert (DZL_IS_PREFERENCES (preferences));
  g_assert (title);
  g_assert (value);

  widget = g_object_new (GTK_TYPE_LABEL,
                         "xalign", 0.0f,
                         "label", title,
                         "visible", TRUE,
                         "margin-right", 12,
                         NULL);
  dzl_gtk_widget_add_style_class (widget, "dim-label");
  dzl_preferences_add_table_row (preferences, page, group, widget,
                                 g_object_new (GTK_TYPE_LABEL,
                                               "hexpand", TRUE,
                                               "label", value,
                                               "xalign", 0.0f,
                                               "visible", TRUE,
                                               NULL),
                                 NULL);
}

static void
ide_build_config_view_addin_load_async (IdeConfigViewAddin  *addin,
                                        DzlPreferences      *preferences,
                                        IdeConfiguration    *config,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeBuildConfigViewAddin *self = (IdeBuildConfigViewAddin *)addin;
  g_autoptr(DzlListModelFilter) filter = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdeRuntimeManager *runtime_manager;
  IdeBuildSystem *build_system;
  IdeContext *context;

  g_assert (IDE_IS_BUILD_CONFIG_VIEW_ADDIN (addin));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_build_config_view_addin_load_async);

  /* Get our manager objects */
  context = ide_object_get_context (IDE_OBJECT (self));
  runtime_manager = ide_context_get_runtime_manager (context);
  build_system = ide_context_get_build_system (context);

  /* Add our pages */
  dzl_preferences_add_page (preferences, "general", _("General"), 0);

  /* Add groups to pages */
  dzl_preferences_add_list_group (preferences, "general", "general", _("General"), GTK_SELECTION_NONE, 0);

  /* Add description info */
  add_description_row (preferences, "general", "general", _("Build System"), ide_build_system_get_display_name (build_system));
  add_description_row (preferences, "general", "general", _("Prefix"), "/app");

  /* Setup runtime selection */
  dzl_preferences_add_list_group (preferences, "general", "runtime", _("Application Runtime"), GTK_SELECTION_NONE, 0);

  filter = dzl_list_model_filter_new (G_LIST_MODEL (runtime_manager));

#if 0
  dzl_preferences_add_list_group (preferences, "general", "general", _("General"), GTK_SELECTION_NONE, 0);
  dzl_preferences_add_custom (preferences, "general", "general", g_object_new (GTK_TYPE_LABEL, "visible", TRUE, NULL), NULL, 0);
  dzl_preferences_add_custom (preferences, "general", "general", g_object_new (GTK_TYPE_LABEL, "visible", TRUE, NULL), NULL, 0);
#endif

  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_build_config_view_addin_load_finish (IdeConfigViewAddin  *addin,
                                         GAsyncResult        *result,
                                         GError             **error)
{
  g_assert (IDE_IS_BUILD_CONFIG_VIEW_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
config_view_addin_iface_init (IdeConfigViewAddinInterface *iface)
{
  iface->load_async = ide_build_config_view_addin_load_async;
  iface->load_finish = ide_build_config_view_addin_load_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeBuildConfigViewAddin, ide_build_config_view_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CONFIG_VIEW_ADDIN,
                                                config_view_addin_iface_init))

static void
ide_build_config_view_addin_class_init (IdeBuildConfigViewAddinClass *klass)
{
}

static void
ide_build_config_view_addin_init (IdeBuildConfigViewAddin *self)
{
}
