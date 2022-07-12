/* gbp-valgrind-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-valgrind-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <errno.h>
#include <unistd.h>

#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-valgrind-workbench-addin.h"

struct _GbpValgrindWorkbenchAddin
{
  GObject       parent_instance;
  GActionGroup *actions;
};

static void
gbp_valgrind_workbench_addin_load (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;
  g_autoptr(GSettings) settings = NULL;
  static const char *keys[] = {
    "leak-check",
    "leak-kind-definite",
    "leak-kind-indirect",
    "leak-kind-possible",
    "leak-kind-reachable",
    "track-origins",
  };

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  settings = g_settings_new ("org.gnome.builder.valgrind");
  self->actions = G_ACTION_GROUP (g_simple_action_group_new ());

  for (guint i = 0; i < G_N_ELEMENTS (keys); i++)
    {
      g_autoptr(GAction) action = g_settings_create_action (settings, keys[i]);
      g_action_map_add_action (G_ACTION_MAP (self->actions), action);
    }

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_clear_object (&self->actions);

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_workspace_added (IdeWorkbenchAddin *addin,
                                              IdeWorkspace      *workspace)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  if (IDE_IS_PRIMARY_WORKSPACE (workspace))
    gtk_widget_insert_action_group (GTK_WIDGET (workspace),
                                    "valgrind",
                                    self->actions);

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_workspace_removed (IdeWorkbenchAddin *addin,
                                                IdeWorkspace      *workspace)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "valgrind", NULL);

  IDE_EXIT;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_valgrind_workbench_addin_load;
  iface->unload = gbp_valgrind_workbench_addin_unload;
  iface->workspace_added = gbp_valgrind_workbench_addin_workspace_added;
  iface->workspace_removed = gbp_valgrind_workbench_addin_workspace_removed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpValgrindWorkbenchAddin, gbp_valgrind_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_valgrind_workbench_addin_class_init (GbpValgrindWorkbenchAddinClass *klass)
{
}

static void
gbp_valgrind_workbench_addin_init (GbpValgrindWorkbenchAddin *self)
{
}
