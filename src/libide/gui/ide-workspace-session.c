/* ide-workspace-session.c
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

#define G_LOG_DOMAIN "ide-workspace-session"

#include "config.h"

#include <libpeas.h>

#include <libide-plugins.h>

#include "ide-frame.h"
#include "ide-grid.h"
#include "ide-gui-global.h"
#include "ide-session.h"
#include "ide-session-item.h"
#include "ide-workspace-addin.h"
#include "ide-workspace-private.h"

G_GNUC_UNUSED static void
dump_position (PanelPosition *position)
{
  GString *str = g_string_new (NULL);

  if (panel_position_get_area_set (position))
    {
      PanelArea area = panel_position_get_area (position);
      g_autoptr(GEnumClass) klass = g_type_class_ref (PANEL_TYPE_AREA);
      GEnumValue *value = g_enum_get_value (klass, area);

      g_string_append_printf (str, "area=%s ", value->value_nick);
    }

  if (panel_position_get_column_set (position))
    {
      guint column = panel_position_get_column (position);
      g_string_append_printf (str, "column=%d ", column);
    }

  if (panel_position_get_row_set (position))
    {
      guint row = panel_position_get_row (position);
      g_string_append_printf (str, "row=%d ", row);
    }

  if (panel_position_get_depth_set (position))
    {
      guint depth = panel_position_get_depth (position);
      g_string_append_printf (str, "depth=%d ", depth);
    }

  if (str->len == 0)
    g_print ("Empty Position\n");
  else
    g_print ("%s\n", str->str);

  g_string_free (str, TRUE);
}

static void
ide_workspace_addin_save_session_cb (IdeExtensionSetAdapter *adapter,
                                     PeasPluginInfo         *plugin_info,
                                     GObject          *exten,
                                     gpointer                user_data)
{
  IdeWorkspaceAddin *addin = (IdeWorkspaceAddin *)exten;
  IdeSession *session = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_SESSION (session));

  ide_workspace_addin_save_session (addin, session);
}

void
_ide_workspace_save_session (IdeWorkspace *self,
                             IdeSession   *session)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_SESSION (session));

  if (IDE_WORKSPACE_GET_CLASS (self)->save_session)
    IDE_WORKSPACE_GET_CLASS (self)->save_session (self, session);

  ide_extension_set_adapter_foreach (_ide_workspace_get_addins (self),
                                     ide_workspace_addin_save_session_cb,
                                     session);

  IDE_EXIT;
}

static void
ide_workspace_save_session_frame_cb (PanelFrame *frame,
                                     gpointer    user_data)
{
  g_autoptr(PanelPosition) position = NULL;
  g_autoptr(IdeSessionItem) item = NULL;
  IdeSession *session = user_data;
  IdeWorkspace *workspace;
  const char *workspace_id;
  int requested_size;
  guint n_pages;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PANEL_IS_FRAME (frame));
  g_assert (IDE_IS_SESSION (session));

  position = panel_frame_get_position (frame);
  workspace = ide_widget_get_workspace (GTK_WIDGET (frame));
  workspace_id = ide_workspace_get_id (workspace);
  requested_size = panel_frame_get_requested_size (frame);

#if 0
  dump_position (position);
#endif

  item = ide_session_item_new ();
  ide_session_item_set_module_name (item, "libide-gui");
  ide_session_item_set_type_hint (item, G_OBJECT_TYPE_NAME (frame));
  ide_session_item_set_position (item, position);
  ide_session_item_set_workspace (item, workspace_id);

  if (requested_size > -1)
    ide_session_item_set_metadata (item, "size", "i", requested_size);

  ide_session_append (session, item);

  n_pages = panel_frame_get_n_pages (frame);

  for (guint i = 0; i < n_pages; i++)
    {
      PanelWidget *widget = panel_frame_get_page (frame, i);

      if (IDE_IS_PANE (widget))
        {
          const char *id = ide_pane_get_id (IDE_PANE (widget));
          g_autoptr(PanelPosition) page_position = panel_widget_get_position (widget);
          g_autoptr(IdeSessionItem) page_item = NULL;

          if (id == NULL)
            continue;

          page_item = ide_session_item_new ();
          ide_session_item_set_id (page_item, id);
          ide_session_item_set_workspace (page_item, workspace_id);
          ide_session_item_set_type_hint (page_item, "IdePane");
          ide_session_item_set_module_name (page_item, "libide-gui");
          ide_session_item_set_position (page_item, page_position);

          if (panel_frame_get_visible_child (frame) == widget)
            ide_session_item_set_metadata (page_item, "is-front", "b", TRUE);

          ide_session_append (session, page_item);
        }
    }

  IDE_EXIT;
}

void
_ide_workspace_save_session_simple (IdeWorkspace     *self,
                                    IdeSession       *session,
                                    IdeWorkspaceDock *dock)
{
  g_autoptr(IdeSessionItem) item = NULL;
  gboolean reveal_start;
  gboolean reveal_end;
  gboolean reveal_bottom;
  int start_width;
  int end_width;
  int bottom_height;
  int width;
  int height;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_SESSION (session));

  gtk_window_get_default_size (GTK_WINDOW (self), &width, &height);

  item = ide_session_item_new ();
  ide_session_item_set_id (item, ide_workspace_get_id (self));
  ide_session_item_set_workspace (item, ide_workspace_get_id (self));
  ide_session_item_set_module_name (item, "libide-gui");
  ide_session_item_set_type_hint (item, G_OBJECT_TYPE_NAME (self));
  ide_session_item_set_metadata (item, "size", "(ii)", width, height);
  if (gtk_window_is_active (GTK_WINDOW (self)))
    ide_session_item_set_metadata (item, "is-active", "b", TRUE);
  if (gtk_window_is_maximized (GTK_WINDOW (self)))
    ide_session_item_set_metadata (item, "is-maximized", "b", TRUE);

  g_object_get (dock->dock,
                "reveal-start", &reveal_start,
                "reveal-end", &reveal_end,
                "reveal-bottom", &reveal_bottom,
                "start-width", &start_width,
                "end-width", &end_width,
                "bottom-height", &bottom_height,
                NULL);

  ide_session_item_set_metadata (item, "reveal-start", "b", reveal_start);
  ide_session_item_set_metadata (item, "reveal-end", "b", reveal_end);
  ide_session_item_set_metadata (item, "reveal-bottom", "b", reveal_bottom);

  ide_session_item_set_metadata (item, "start-width", "i", start_width);
  ide_session_item_set_metadata (item, "end-width", "i", end_width);
  ide_session_item_set_metadata (item, "bottom-height", "i", bottom_height);

#if 0
  g_print ("Saving %d %d %d %d %d %d\n",
           reveal_start, reveal_end, reveal_bottom,
           start_width, end_width, bottom_height);
#endif

  ide_session_prepend (session, item);

  panel_dock_foreach_frame (dock->dock,
                            ide_workspace_save_session_frame_cb,
                            session);
  panel_grid_foreach_frame (PANEL_GRID (dock->grid),
                            ide_workspace_save_session_frame_cb,
                            session);

  IDE_EXIT;
}

static void
ide_workspace_addin_restore_session_cb (IdeExtensionSetAdapter *adapter,
                                        PeasPluginInfo         *plugin_info,
                                        GObject          *exten,
                                        gpointer                user_data)
{
  IdeWorkspaceAddin *addin = (IdeWorkspaceAddin *)exten;
  IdeSession *session = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_SESSION (session));

  ide_workspace_addin_restore_session (addin, session);
}

void
_ide_workspace_restore_session (IdeWorkspace *self,
                                IdeSession   *session)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_SESSION (session));

  if (IDE_WORKSPACE_GET_CLASS (self)->restore_session)
    IDE_WORKSPACE_GET_CLASS (self)->restore_session (self, session);

  ide_extension_set_adapter_foreach (_ide_workspace_get_addins (self),
                                     ide_workspace_addin_restore_session_cb,
                                     session);

  IDE_EXIT;
}

static void
ide_workspace_restore_frame (IdeWorkspace     *self,
                             GType             type,
                             IdeSessionItem   *item,
                             IdeWorkspaceDock *dock)
{
  PanelPosition *position;
  GtkWidget *frame;
  PanelArea area;

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (g_type_is_a (type, PANEL_TYPE_FRAME));
  g_assert (IDE_IS_SESSION_ITEM (item));

  if (!(position = ide_session_item_get_position (item)))
    return;

  if (!panel_position_get_area_set (position))
    return;

  area = panel_position_get_area (position);
  if ((area == PANEL_AREA_CENTER && type != IDE_TYPE_FRAME) ||
      (area != PANEL_AREA_CENTER && type != PANEL_TYPE_FRAME))
    return;

  if (area == PANEL_AREA_START || area == PANEL_AREA_END)
    {
      PanelPaned *paned = area == PANEL_AREA_START ? dock->start_area : dock->end_area;
      int row = panel_position_get_row (position);

      while (panel_paned_get_n_children (paned) <= row)
        {
          frame = panel_frame_new ();
          panel_paned_append (paned, GTK_WIDGET (frame));
        }

      frame = panel_paned_get_nth_child (paned, row);
    }
  else if (area == PANEL_AREA_TOP)
    {
      /* Ignored */
      return;
    }
  else if (area == PANEL_AREA_BOTTOM)
    {
      PanelPaned *paned = dock->bottom_area;
      int column = panel_position_get_column (position);

      while (panel_paned_get_n_children (paned) <= column)
        {
          frame = panel_frame_new ();
          gtk_orientable_set_orientation (GTK_ORIENTABLE (frame),
                                          GTK_ORIENTATION_HORIZONTAL);
          panel_paned_append (paned, GTK_WIDGET (frame));
        }

      frame = panel_paned_get_nth_child (paned, column);
    }
  else
    {
      int column = panel_position_get_column (position);
      int row = panel_position_get_row (position);

      frame = GTK_WIDGET (ide_grid_make_frame (dock->grid, column, row));
    }

  if (ide_session_item_has_metadata_with_type (item, "size", G_VARIANT_TYPE ("i")))
    {
      int size;

      ide_session_item_get_metadata (item, "size", "i", &size);
      panel_frame_set_requested_size (PANEL_FRAME (frame), size);
    }
}

static void
ide_workspace_restore_panels (IdeWorkspace     *self,
                              IdeSessionItem   *item,
                              IdeWorkspaceDock *dock)
{
  gboolean reveal_start = -1;
  gboolean reveal_end = -1;
  gboolean reveal_bottom = -1;
  int start_width = -1;
  int end_width = -1;
  int bottom_height = -1;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_SESSION_ITEM (item));
  g_return_if_fail (dock != NULL);

  ide_session_item_get_metadata (item, "reveal-start", "b", &reveal_start);
  ide_session_item_get_metadata (item, "reveal-end", "b", &reveal_end);
  ide_session_item_get_metadata (item, "reveal-bottom", "b", &reveal_bottom);
  ide_session_item_get_metadata (item, "start-width", "i", &start_width);
  ide_session_item_get_metadata (item, "end-width", "i", &end_width);
  ide_session_item_get_metadata (item, "bottom-height", "i", &bottom_height);

#if 0
  g_print ("Restoring %d %d %d %d %d %d\n",
           reveal_start, reveal_end, reveal_bottom,
           start_width, end_width, bottom_height);
#endif

  if (reveal_start > -1)
    panel_dock_set_reveal_start (dock->dock, reveal_start);

  if (reveal_end > -1)
    panel_dock_set_reveal_end (dock->dock, reveal_end);

  if (reveal_bottom > -1)
    panel_dock_set_reveal_bottom (dock->dock, reveal_bottom);

  if (start_width > -1)
    panel_dock_set_start_width (dock->dock, start_width);

  if (end_width > -1)
    panel_dock_set_end_width (dock->dock, end_width);

  if (bottom_height > -1)
    panel_dock_set_bottom_height (dock->dock, bottom_height);
}

typedef struct
{
  const char  *id;
  PanelWidget *widget;
} FindWidget;

static void
_ide_workspace_find_widget_cb (PanelFrame *frame,
                               gpointer    user_data)
{
  FindWidget *find = user_data;
  guint n_pages;

  g_assert (PANEL_IS_FRAME (frame));
  g_assert (find != NULL);

  if (find->widget != NULL)
    return;

  n_pages = panel_frame_get_n_pages (frame);

  for (guint i = 0; i < n_pages; i++)
    {
      PanelWidget *widget = panel_frame_get_page (frame, i);
      const char *id = panel_widget_get_id (widget);

      if (id == NULL && IDE_IS_PANE (widget))
        id = ide_pane_get_id (IDE_PANE (widget));

      if (ide_str_equal0 (find->id, id))
        {
          find->widget = widget;
          return;
        }
    }
}

static PanelWidget *
_ide_workspace_find_widget (IdeWorkspace     *self,
                            IdeWorkspaceDock *dock,
                            const char       *id)
{
  FindWidget find = {id, NULL};

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (dock != NULL);
  g_assert (id != NULL);

  panel_dock_foreach_frame (dock->dock,
                            _ide_workspace_find_widget_cb,
                            &find);

  if (find.widget == NULL)
    panel_grid_foreach_frame (PANEL_GRID (dock->grid),
                              _ide_workspace_find_widget_cb,
                              &find);

  return find.widget;
}

static void
ide_workspace_restore_pane (IdeWorkspace     *self,
                            IdeSessionItem   *item,
                            IdeWorkspaceDock *dock)
{
  g_autoptr(PanelPosition) current_position = NULL;
  PanelPosition *position;
  PanelWidget *widget;
  const char *id;
  GtkWidget *frame;
  gboolean is_front;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (IDE_IS_SESSION_ITEM (item));
  g_assert (dock != NULL);

  if (!(id = ide_session_item_get_id (item)) ||
      !(position = ide_session_item_get_position (item)) ||
      !(widget = _ide_workspace_find_widget (self, dock, id)))
    return;

  g_object_ref (widget);

  if ((current_position = panel_widget_get_position (widget)) &&
      panel_position_equal (current_position, position))
    goto check_front;

  if ((frame = gtk_widget_get_ancestor (GTK_WIDGET (widget), PANEL_TYPE_FRAME)))
    {
      panel_frame_remove (PANEL_FRAME (frame), widget);
      ide_workspace_add_pane (self, IDE_PANE (widget), position);
    }

check_front:
  if (ide_session_item_get_metadata (item, "is-front", "b", &is_front) && is_front)
    panel_widget_raise (widget);

  g_object_unref (widget);
}

void
_ide_workspace_restore_session_simple (IdeWorkspace     *self,
                                       IdeSession       *session,
                                       IdeWorkspaceDock *dock)
{
  g_autoptr(IdeSessionItem) panels = NULL;
  guint n_items;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_SESSION (session));
  g_return_if_fail (dock != NULL);

  n_items = ide_session_get_n_items (session);

  for (guint i = 0; i < n_items; i++)
    {
      IdeSessionItem *item = ide_session_get_item (session, i);
      const char *module_name = ide_session_item_get_module_name (item);

      if (ide_str_equal0 (module_name, "libide-gui"))
        {
          const char *workspace_id = ide_session_item_get_workspace (item);
          const char *type_hint = ide_session_item_get_type_hint (item);
          GType type = type_hint ? g_type_from_name (type_hint) : G_TYPE_INVALID;

          if (type == G_TYPE_INVALID)
            continue;

          if (!ide_str_equal0 (workspace_id, ide_workspace_get_id (self)))
            continue;

          if (g_type_is_a (type, PANEL_TYPE_FRAME))
            ide_workspace_restore_frame (self, type, item, dock);
          else if (g_type_is_a (type, IDE_TYPE_WORKSPACE) &&
                   type == G_OBJECT_TYPE (self))
            g_set_object (&panels, item);
          else if (ide_str_equal0 (type_hint, "IdePane"))
            ide_workspace_restore_pane (self, item, dock);
        }
    }

  /* Restore panels last so their visibility remains intact */
  if (panels != NULL)
    ide_workspace_restore_panels (self, panels, dock);

  IDE_EXIT;
}
