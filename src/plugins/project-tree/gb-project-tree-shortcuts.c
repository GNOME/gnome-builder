/* gb-project-tree-shortcuts.c
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "gb-project-tree-shortcuts"

#include "config.h"

#include <glib/gi18n.h>
#include <dazzle.h>

#include "gb-project-tree.h"

#define I_(s) g_intern_static_string(s)

static const DzlShortcutEntry gb_project_tree_entries[] = {
  { "org.gnome.builder.project-tree.rename-file",
    0, NULL,
    NC_("shortcut window", "Editor shortcuts"),
    NC_("shortcut window", "Project tree"),
    NC_("shortcut window", "Rename a file") },

  { "org.gnome.builder.project-tree.move-to-trah",
    0, NULL,
    NC_("shortcut window", "Editor shortcuts"),
    NC_("shortcut window", "Project tree"),
    NC_("shortcut window", "Move a file to the trash") },
};

void
_gb_project_tree_init_shortcuts (GbProjectTree *self)
{
  DzlShortcutController *controller;

  g_assert (GB_IS_PROJECT_TREE (self));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.project-tree.rename-file"),
                                              I_("F2"),
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("project-tree.rename-file"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.project-tree.move-to-trah"),
                                              I_("Delete"),
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("project-tree.move-to-trash"));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             gb_project_tree_entries,
                                             G_N_ELEMENTS (gb_project_tree_entries),
                                             GETTEXT_PACKAGE);

  /* TODO: remove accel from menu.ui and add API to update this from dzl classes */
}
