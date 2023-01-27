/* gbp-codeui-tree-addin.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-codeui-tree-addin"

#include "config.h"

#include <libide-code.h>
#include <libide-gui.h>
#include <libide-projects.h>
#include <libide-tree.h>

#include "gbp-codeui-tree-addin.h"

struct _GbpCodeuiTreeAddin
{
  GObject                parent_instance;
  IdeContext            *context;
  IdeDiagnosticsManager *diagnostics_manager;
};

static void
gbp_codeui_tree_addin_build_node (IdeTreeAddin *addin,
                                  IdeTreeNode  *node)
{
  GbpCodeuiTreeAddin *self = (GbpCodeuiTreeAddin *)addin;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(GFile) file = NULL;
  IdeProjectFile *project_file;
  gboolean has_error = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));

  if (!ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    return;

  project_file = ide_tree_node_get_item (node);
  file = ide_project_file_ref_file (project_file);
  diagnostics = ide_diagnostics_manager_get_diagnostics_for_file (self->diagnostics_manager, file);

  if (diagnostics != NULL)
    {
      if (ide_diagnostics_get_has_errors (diagnostics))
        has_error = TRUE;
    }

  ide_tree_node_set_has_error (node, has_error);
}

static void
gbp_codeui_tree_addin_load (IdeTreeAddin *addin,
                            IdeTree      *tree)
{
  GbpCodeuiTreeAddin *self = (GbpCodeuiTreeAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));

  self->context = ide_widget_get_context (GTK_WIDGET (tree));
  self->diagnostics_manager = g_object_ref (ide_diagnostics_manager_from_context (self->context));

  IDE_EXIT;
}

static void
gbp_codeui_tree_addin_unload (IdeTreeAddin *addin,
                              IdeTree      *tree)
{
  GbpCodeuiTreeAddin *self = (GbpCodeuiTreeAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));

  g_clear_object (&self->diagnostics_manager);
  self->context = NULL;

  IDE_EXIT;
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->load = gbp_codeui_tree_addin_load;
  iface->unload = gbp_codeui_tree_addin_unload;
  iface->build_node = gbp_codeui_tree_addin_build_node;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodeuiTreeAddin, gbp_codeui_tree_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_codeui_tree_addin_class_init (GbpCodeuiTreeAddinClass *klass)
{
}

static void
gbp_codeui_tree_addin_init (GbpCodeuiTreeAddin *self)
{
}
