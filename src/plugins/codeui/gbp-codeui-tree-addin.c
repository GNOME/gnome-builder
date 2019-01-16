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
#include <libide-projects.h>
#include <libide-tree.h>

#include "gbp-codeui-tree-addin.h"

struct _GbpCodeuiTreeAddin
{
  GObject parent_instance;
};

static void
gbp_codeui_tree_addin_cell_data_func (IdeTreeAddin    *addin,
                                      IdeTreeNode     *node,
                                      GtkCellRenderer *cell)
{
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) file = NULL;
  IdeDiagnosticsManager *diagmgr;
  IdeProjectFile *project_file;
  gboolean has_error = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODEUI_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (GTK_IS_CELL_RENDERER (cell));

  if (!GTK_IS_CELL_RENDERER_TEXT (cell) ||
      !ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    return;

  project_file = ide_tree_node_get_item (node);
  file = ide_project_file_ref_file (project_file);
  context = ide_object_ref_context (IDE_OBJECT (project_file));
  diagmgr = ide_diagnostics_manager_from_context (context);
  diagnostics = ide_diagnostics_manager_get_diagnostics_for_file (diagmgr, file);

  if (diagnostics != NULL)
    {
      if (ide_diagnostics_get_has_errors (diagnostics))
        has_error = TRUE;
    }

  ide_tree_node_set_has_error (node, has_error);
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->cell_data_func = gbp_codeui_tree_addin_cell_data_func;
}

G_DEFINE_TYPE_WITH_CODE (GbpCodeuiTreeAddin, gbp_codeui_tree_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_codeui_tree_addin_class_init (GbpCodeuiTreeAddinClass *klass)
{
}

static void
gbp_codeui_tree_addin_init (GbpCodeuiTreeAddin *self)
{
}
