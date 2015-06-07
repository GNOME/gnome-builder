/* gb-workbench-private.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef GB_WORKBENCH_PRIVATE_H
#define GB_WORKBENCH_PRIVATE_H

#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <ide.h>

#include "gb-editor-workspace.h"
#include "gb-project-tree.h"
#include "gb-search-box.h"
#include "gb-view-grid.h"
#include "gb-workbench-types.h"
#include "gedit-menu-stack-switcher.h"

G_BEGIN_DECLS

struct _GbWorkbench
{
  GtkApplicationWindow    parent_instance;

  /* Owned reference */
  IdeContext             *context;
  GCancellable           *unload_cancellable;
  gchar                  *current_folder_uri;
  PeasExtensionSet       *extensions;

  /* Template references */
  GeditMenuStackSwitcher *gear_menu_button;
  GbProjectTree          *project_tree;
  GbSearchBox            *search_box;
  GbViewGrid             *view_grid;
  GbWorkspace            *workspace;

  gulong                  project_notify_name_handler;

  guint                   disposing;

  guint                   building : 1;
  guint                   unloading : 1;
  guint                   has_opened : 1;
};

G_END_DECLS

#endif /* GB_WORKBENCH_PRIVATE_H */
