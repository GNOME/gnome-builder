/* ide-editor-surface.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-surface"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-io.h>
#include <libide-code.h>

#include "ide-buffer-private.h"
#include "ide-gfile-private.h"

#include "ide-editor-addin.h"
#include "ide-editor-surface.h"
#include "ide-editor-private.h"
#include "ide-editor-sidebar.h"
#include "ide-editor-utilities.h"
#include "ide-editor-page.h"

typedef struct
{
  IdeEditorSurface *self;
  IdeLocation      *location;
} FocusLocation;

enum {
  PROP_0,
  PROP_RESTORE_PANEL,
  N_PROPS
};

static void ide_editor_surface_focus_location_full (IdeEditorSurface *self,
                                                    IdeLocation      *location,
                                                    gboolean          open_if_not_found);

static GParamSpec *properties [N_PROPS];

G_DEFINE_TYPE (IdeEditorSurface, ide_editor_surface, IDE_TYPE_SURFACE)

static void
ide_editor_surface_foreach_page (IdeSurface  *surface,
                                 GtkCallback  callback,
                                 gpointer     user_data)
{
  IdeEditorSurface *self = (IdeEditorSurface *)surface;

  g_assert (IDE_IS_EDITOR_SURFACE (self));
  g_assert (callback != NULL);

  ide_grid_foreach_page (self->grid, callback, user_data);
}

static void
set_reveal_child_without_transition (DzlDockRevealer *revealer,
                                     gboolean         reveal)
{
  DzlDockRevealerTransitionType type;

  g_assert (DZL_IS_DOCK_REVEALER (revealer));

  type = dzl_dock_revealer_get_transition_type (revealer);
  dzl_dock_revealer_set_transition_type (revealer, DZL_DOCK_REVEALER_TRANSITION_TYPE_NONE);
  dzl_dock_revealer_set_reveal_child (revealer, reveal);
  dzl_dock_revealer_set_transition_type (revealer, type);
}

static void
ide_editor_surface_restore_panel_state (IdeEditorSurface *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (IDE_IS_EDITOR_SURFACE (self));

  /* TODO: This belongs in editor settings probably */

  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self));
  reveal = self->restore_panel ? g_settings_get_boolean (settings, "left-visible") : FALSE;
  position = g_settings_get_int (settings, "left-position");
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);
  set_reveal_child_without_transition (DZL_DOCK_REVEALER (pane), reveal);

  pane = dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self));
  position = g_settings_get_int (settings, "right-position");
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);
  set_reveal_child_without_transition (DZL_DOCK_REVEALER (pane), FALSE);

  pane = dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self));
  reveal = self->restore_panel ? g_settings_get_boolean (settings, "bottom-visible") : FALSE;
  position = g_settings_get_int (settings, "bottom-position");
  dzl_dock_revealer_set_position (DZL_DOCK_REVEALER (pane), position);
  set_reveal_child_without_transition (DZL_DOCK_REVEALER (pane), reveal);
}

static void
ide_editor_surface_save_panel_state (IdeEditorSurface *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (IDE_IS_EDITOR_SURFACE (self));

  if (!self->restore_panel)
    return;

  /* TODO: possibly belongs in editor settings */
  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "left-visible", reveal);
  g_settings_set_int (settings, "left-position", position);

  pane = dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "right-visible", reveal);
  g_settings_set_int (settings, "right-position", position);

  pane = dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self));
  position = dzl_dock_revealer_get_position (DZL_DOCK_REVEALER (pane));
  reveal = dzl_dock_revealer_get_reveal_child (DZL_DOCK_REVEALER (pane));
  g_settings_set_boolean (settings, "bottom-visible", reveal);
  g_settings_set_int (settings, "bottom-position", position);
}

static gboolean
ide_editor_surface_agree_to_shutdown (IdeSurface *surface)
{
  IdeEditorSurface *self = (IdeEditorSurface *)surface;

  g_assert (IDE_IS_EDITOR_SURFACE (self));

  ide_editor_surface_save_panel_state (self);

  return TRUE;
}

static void
ide_editor_surface_set_fullscreen (IdeSurface *surface,
                                   gboolean    fullscreen)
{
  IdeEditorSurface *self = (IdeEditorSurface *)surface;

  g_assert (IDE_IS_EDITOR_SURFACE (self));

  if (fullscreen)
    {
      gboolean left_visible;
      gboolean bottom_visible;

      g_object_get (self,
                    "left-visible", &left_visible,
                    "bottom-visible", &bottom_visible,
                    NULL);

      self->prefocus_had_left = left_visible;
      self->prefocus_had_bottom = bottom_visible;

      g_object_set (self,
                    "left-visible", FALSE,
                    "bottom-visible", FALSE,
                    NULL);
    }
  else
    {
      g_object_set (self,
                    "left-visible", self->prefocus_had_left,
                    "bottom-visible", self->prefocus_had_bottom,
                    NULL);
    }
}


static void
ide_editor_surface_addin_added (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                PeasExtension    *exten,
                                gpointer          user_data)
{
  IdeEditorSurface *self = user_data;
  IdeEditorAddin *addin = (IdeEditorAddin *)exten;
  IdePage *page;

  g_assert (IDE_IS_EDITOR_SURFACE (self));
  g_assert (IDE_IS_EDITOR_ADDIN (addin));
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);

  ide_editor_addin_load (addin, self);

  page = ide_grid_get_current_page (self->grid);
  if (page != NULL)
    ide_editor_addin_page_set (addin, page);
}

static void
ide_editor_surface_addin_removed (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      PeasExtension    *exten,
                                      gpointer          user_data)
{
  IdeEditorSurface *self = user_data;
  IdeEditorAddin *addin = (IdeEditorAddin *)exten;
  IdePage *page;

  g_assert (IDE_IS_EDITOR_SURFACE (self));
  g_assert (IDE_IS_EDITOR_ADDIN (addin));
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);

  page = ide_grid_get_current_page (self->grid);
  if (page != NULL)
    ide_editor_addin_page_set (addin, NULL);

  ide_editor_addin_unload (addin, self);
}

static void
ide_editor_surface_hierarchy_changed (GtkWidget *widget,
                                      GtkWidget *old_toplevel)
{
  IdeEditorSurface *self = (IdeEditorSurface *)widget;

  g_assert (IDE_IS_EDITOR_SURFACE (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  if (self->addins == NULL)
    {
      GtkWidget *toplevel;

      /*
       * If we just got a new toplevel and it is a workbench,
       * and we have not yet created our addins, do so now.
       */

      toplevel = gtk_widget_get_ancestor (widget, IDE_TYPE_WORKSPACE);

      if (toplevel != NULL)
        {
          self->addins = peas_extension_set_new (peas_engine_get_default (),
                                                 IDE_TYPE_EDITOR_ADDIN,
                                                 NULL);
          g_signal_connect (self->addins,
                            "extension-added",
                            G_CALLBACK (ide_editor_surface_addin_added),
                            self);
          g_signal_connect (self->addins,
                            "extension-removed",
                            G_CALLBACK (ide_editor_surface_addin_removed),
                            self);
          peas_extension_set_foreach (self->addins,
                                      ide_editor_surface_addin_added,
                                      self);
        }
    }
}

static void
ide_editor_surface_addins_page_set (PeasExtensionSet *set,
                                    PeasPluginInfo   *plugin_info,
                                    PeasExtension    *exten,
                                    gpointer          user_data)
{
  IdeEditorAddin *addin = (IdeEditorAddin *)exten;
  IdePage *page = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_ADDIN (addin));
  g_assert (!page || IDE_IS_PAGE (page));

  ide_editor_addin_page_set (addin, page);
}

static void
ide_editor_surface_notify_current_page (IdeEditorSurface *self,
                                        GParamSpec       *pspec,
                                        IdeGrid          *grid)
{
  IdePage *page;

  g_assert (IDE_IS_EDITOR_SURFACE (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_GRID (grid));

  page = ide_grid_get_current_page (grid);

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins,
                                ide_editor_surface_addins_page_set,
                                page);
}

static void
ide_editor_surface_add (GtkContainer *container,
                            GtkWidget    *widget)
{
  IdeEditorSurface *self = (IdeEditorSurface *)container;

  g_assert (IDE_IS_EDITOR_SURFACE (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (IDE_IS_PAGE (widget))
    gtk_container_add (GTK_CONTAINER (self->grid), widget);
  else
    GTK_CONTAINER_CLASS (ide_editor_surface_parent_class)->add (container, widget);
}

static GtkWidget *
ide_editor_surface_create_edge (DzlDockBin      *dock_bin,
                                GtkPositionType  edge)
{
  g_assert (DZL_IS_DOCK_BIN (dock_bin));
  g_assert (edge >= GTK_POS_LEFT);
  g_assert (edge <= GTK_POS_BOTTOM);

  if (edge == GTK_POS_LEFT)
    return g_object_new (IDE_TYPE_EDITOR_SIDEBAR,
                         "edge", edge,
                         "transition-duration", 333,
                         "reveal-child", FALSE,
                         "visible", TRUE,
                         NULL);

  if (edge == GTK_POS_RIGHT)
    return g_object_new (IDE_TYPE_TRANSIENT_SIDEBAR,
                         "edge", edge,
                         "reveal-child", FALSE,
                         "transition-duration", 333,
                         "visible", FALSE,
                         NULL);

  if (edge == GTK_POS_BOTTOM)
    return g_object_new (IDE_TYPE_EDITOR_UTILITIES,
                         "edge", edge,
                         "reveal-child", FALSE,
                         "transition-duration", 333,
                         "visible", TRUE,
                         NULL);

  return DZL_DOCK_BIN_CLASS (ide_editor_surface_parent_class)->create_edge (dock_bin, edge);
}

static void
ide_editor_surface_load_file_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeBuffer) buffer = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));

  buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error);

  if (error != NULL)
    g_warning ("%s", error->message);

  /* TODO: Ensure that the page is marked as failed */

  IDE_EXIT;
}

static IdePage *
ide_editor_surface_create_page (IdeEditorSurface *self,
                                const gchar      *uri,
                                IdeGrid          *grid)
{
  g_autoptr(GFile) file = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;
  IdeBuffer *buffer;

  g_assert (IDE_IS_EDITOR_SURFACE (self));
  g_assert (uri != NULL);
  g_assert (IDE_IS_GRID (grid));

  g_debug ("Creating page for %s", uri);

  context = ide_widget_get_context (GTK_WIDGET (self));

  file = g_file_new_for_uri (uri);
  bufmgr = ide_buffer_manager_from_context (context);
  buffer = ide_buffer_manager_find_buffer (bufmgr, file);

  /*
   * If we failed to locate an already loaded buffer, we need to start
   * loading the buffer. But that could take some time. Either way, after
   * we start the loading process, we can access the buffer and we'll
   * display it while it loads.
   */

  if (buffer == NULL)
    {
      ide_buffer_manager_load_file_async (bufmgr,
                                          file,
                                          IDE_BUFFER_OPEN_FLAGS_NO_VIEW,
                                          NULL,
                                          NULL,
                                          ide_editor_surface_load_file_cb,
                                          g_object_ref (self));
      buffer = ide_buffer_manager_find_buffer (bufmgr, file);
    }

  return g_object_new (IDE_TYPE_EDITOR_PAGE,
                       "buffer", buffer,
                       "visible", TRUE,
                       NULL);
}

static void
ide_editor_surface_grab_focus (GtkWidget *widget)
{
  IdeEditorSurface *self = (IdeEditorSurface *)widget;

  g_assert (IDE_IS_EDITOR_SURFACE (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->grid));
}

static void
ide_editor_surface_destroy (GtkWidget *widget)
{
  IdeEditorSurface *self = (IdeEditorSurface *)widget;

  g_assert (IDE_IS_EDITOR_SURFACE (self));

  g_clear_object (&self->addins);

  GTK_WIDGET_CLASS (ide_editor_surface_parent_class)->destroy (widget);
}

static void
ide_editor_surface_realize (GtkWidget *widget)
{
  IdeEditorSurface *self = (IdeEditorSurface *)widget;

  g_assert (IDE_IS_EDITOR_SURFACE (self));

  ide_editor_surface_restore_panel_state (self);

  GTK_WIDGET_CLASS (ide_editor_surface_parent_class)->realize (widget);
}

static void
ide_editor_surface_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeEditorSurface *self = IDE_EDITOR_SURFACE (object);

  switch (prop_id)
    {
    case PROP_RESTORE_PANEL:
      g_value_set_boolean (value, self->restore_panel);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_surface_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeEditorSurface *self = IDE_EDITOR_SURFACE (object);

  switch (prop_id)
    {
    case PROP_RESTORE_PANEL:
      self->restore_panel = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_surface_class_init (IdeEditorSurfaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  DzlDockBinClass *dock_bin_class = DZL_DOCK_BIN_CLASS (klass);
  IdeSurfaceClass *surface_class = IDE_SURFACE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_editor_surface_get_property;
  object_class->set_property = ide_editor_surface_set_property;

  widget_class->destroy = ide_editor_surface_destroy;
  widget_class->hierarchy_changed = ide_editor_surface_hierarchy_changed;
  widget_class->grab_focus = ide_editor_surface_grab_focus;
  widget_class->realize = ide_editor_surface_realize;

  container_class->add = ide_editor_surface_add;

  dock_bin_class->create_edge = ide_editor_surface_create_edge;

  surface_class->agree_to_shutdown = ide_editor_surface_agree_to_shutdown;
  surface_class->foreach_page = ide_editor_surface_foreach_page;
  surface_class->set_fullscreen = ide_editor_surface_set_fullscreen;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ui/ide-editor-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSurface, grid);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSurface, overlay);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSurface, loading_stack);

  properties [PROP_RESTORE_PANEL] =
    g_param_spec_boolean ("restore-panel", NULL, NULL,
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_type_ensure (IDE_TYPE_EDITOR_SIDEBAR);
  g_type_ensure (IDE_TYPE_GRID);
}

static void
ide_editor_surface_init (IdeEditorSurface *self)
{
  IdeEditorSidebar *sidebar;

  self->restore_panel = TRUE;

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_surface_set_icon_name (IDE_SURFACE (self), "builder-editor-symbolic");
  ide_surface_set_title (IDE_SURFACE (self), _("Editor"));

  _ide_editor_surface_init_actions (self);
  _ide_editor_surface_init_shortcuts (self);

  /* ensure we default to the grid visible */
  _ide_editor_surface_set_loading (self, FALSE);

  g_signal_connect_swapped (self->grid,
                            "notify::current-page",
                            G_CALLBACK (ide_editor_surface_notify_current_page),
                            self);

  g_signal_connect_swapped (self->grid,
                            "create-page",
                            G_CALLBACK (ide_editor_surface_create_page),
                            self);

  sidebar = ide_editor_surface_get_sidebar (self);
  _ide_editor_sidebar_set_open_pages (sidebar, G_LIST_MODEL (self->grid));
}

/**
 * ide_editor_surface_get_grid:
 * @self: a #IdeEditorSurface
 *
 * Gets the grid for the surface. This is the area containing
 * grid columns, stacks, and pages.
 *
 * Returns: (transfer none): An #IdeGrid.
 *
 * Since: 3.32
 */
IdeGrid *
ide_editor_surface_get_grid (IdeEditorSurface *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SURFACE (self), NULL);

  return self->grid;
}

static void
ide_editor_surface_find_source_location (GtkWidget *widget,
                                         gpointer   user_data)
{
  struct {
    GFile *file;
    IdeEditorPage *page;
  } *lookup = user_data;
  IdeBuffer *buffer;

  g_return_if_fail (IDE_IS_PAGE (widget));

  if (lookup->page != NULL)
    return;

  if (!IDE_IS_EDITOR_PAGE (widget))
    return;

  buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (widget));

  if (_ide_buffer_is_file (buffer, lookup->file))
    lookup->page = IDE_EDITOR_PAGE (widget);
}

static void
ide_editor_surface_focus_location_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;
  FocusLocation *state = user_data;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (state != NULL);
  g_assert (IDE_IS_EDITOR_SURFACE (state->self));
  g_assert (state->location != NULL);

  if (!(buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    {
      /* TODO: display warning breifly to the user in the frame? */
      g_warning ("%s", error->message);
      g_clear_error (&error);
      IDE_GOTO (cleanup);
    }

  /* try again now that we have loaded */
  ide_editor_surface_focus_location_full (state->self, state->location, FALSE);

cleanup:
  g_clear_object (&state->self);
  g_clear_object (&state->location);
  g_slice_free (FocusLocation, state);

  IDE_EXIT;
}

static void
ide_editor_surface_focus_location_full (IdeEditorSurface *self,
                                        IdeLocation      *location,
                                        gboolean          open_if_not_found)
{
  g_autoptr(GFile) translated = NULL;
  struct {
    GFile *file;
    IdeEditorPage *page;
  } lookup = { 0 };
  GtkWidget *stack;
  gint line;
  gint line_offset;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_SURFACE (self));
  g_assert (location != NULL);

  /* Remove symlinks to increase chance we find a match */
  translated = _ide_g_file_readlink (ide_location_get_file (location));

  lookup.file = translated;
  lookup.page = NULL;

  if (lookup.file == NULL)
    {
      g_warning ("IdeLocation does not contain a file");
      IDE_EXIT;
    }

#ifdef IDE_ENABLE_TRACE
  {
    const gchar *path = g_file_peek_path (lookup.file);
    IDE_TRACE_MSG ("Locating %s, open_if_not_found=%d",
                   path, open_if_not_found);
  }
#endif

  ide_surface_foreach_page (IDE_SURFACE (self),
                            ide_editor_surface_find_source_location,
                            &lookup);

  if (!open_if_not_found && lookup.page == NULL)
    IDE_EXIT;

  if (lookup.page == NULL)
    {
      FocusLocation *state;
      IdeBufferManager *bufmgr;
      IdeWorkbench *workbench;
      IdeContext *context;

      workbench = ide_widget_get_workbench (GTK_WIDGET (self));
      context = ide_workbench_get_context (workbench);
      bufmgr = ide_buffer_manager_from_context (context);

      state = g_slice_new0 (FocusLocation);
      state->self = g_object_ref (self);
      state->location = g_object_ref (location);

      ide_buffer_manager_load_file_async (bufmgr,
                                          lookup.file,
                                          IDE_BUFFER_OPEN_FLAGS_NONE,
                                          NULL,
                                          NULL,
                                          ide_editor_surface_focus_location_cb,
                                          state);
      IDE_EXIT;
    }

  line = ide_location_get_line (location);
  line_offset = ide_location_get_line_offset (location);

  stack = gtk_widget_get_ancestor (GTK_WIDGET (lookup.page), IDE_TYPE_FRAME);
  ide_frame_set_visible_child (IDE_FRAME (stack), IDE_PAGE (lookup.page));

  /*
   * Ignore 0:0 so that we don't jump from the previous cursor position,
   * if any. It's somewhat problematic if we know we need to go to 0:0,
   * but that is less likely.
   */
  if (line > 0 || line_offset > 0)
    ide_editor_page_scroll_to_line_offset (lookup.page,
                                           MAX (line, 0),
                                           MAX (line_offset, 0));
  else
    gtk_widget_grab_focus (GTK_WIDGET (lookup.page));

  IDE_EXIT;
}

void
ide_editor_surface_focus_location (IdeEditorSurface  *self,
                                   IdeLocation *location)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITOR_SURFACE (self));
  g_return_if_fail (location != NULL);

  ide_editor_surface_focus_location_full (self, location, TRUE);

  IDE_EXIT;
}

static void
locate_page_for_buffer (GtkWidget *widget,
                        gpointer   user_data)
{
  struct {
    IdeBuffer *buffer;
    IdePage   *page;
  } *lookup = user_data;

  if (lookup->page != NULL)
    return;

  if (IDE_IS_EDITOR_PAGE (widget))
    {
      if (ide_editor_page_get_buffer (IDE_EDITOR_PAGE (widget)) == lookup->buffer)
        lookup->page = IDE_PAGE (widget);
    }
}

static gboolean
ide_editor_surface_focus_if_found (IdeEditorSurface *self,
                                   IdeBuffer        *buffer,
                                   gboolean          any_stack)
{
  IdeFrame *stack;
  struct {
    IdeBuffer *buffer;
    IdePage   *page;
  } lookup = { buffer };

  g_return_val_if_fail (IDE_IS_EDITOR_SURFACE (self), FALSE);
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), FALSE);

  stack = ide_grid_get_current_stack (self->grid);

  if (any_stack)
    ide_grid_foreach_page (self->grid, locate_page_for_buffer, &lookup);
  else
    ide_frame_foreach_page (stack, locate_page_for_buffer, &lookup);

  if (lookup.page != NULL)
    {
      stack = IDE_FRAME (gtk_widget_get_ancestor (GTK_WIDGET (lookup.page), IDE_TYPE_FRAME));
      ide_frame_set_visible_child (stack, lookup.page);
      gtk_widget_grab_focus (GTK_WIDGET (lookup.page));
      return TRUE;
    }

  return FALSE;
}

void
ide_editor_surface_focus_buffer (IdeEditorSurface *self,
                                 IdeBuffer        *buffer)
{
  IdeEditorPage *page;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITOR_SURFACE (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (ide_editor_surface_focus_if_found (self, buffer, TRUE))
    IDE_EXIT;

  page = g_object_new (IDE_TYPE_EDITOR_PAGE,
                       "buffer", buffer,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (self->grid), GTK_WIDGET (page));

  IDE_EXIT;
}

void
ide_editor_surface_focus_buffer_in_current_stack (IdeEditorSurface *self,
                                                  IdeBuffer        *buffer)
{
  IdeFrame *stack;
  IdeEditorPage *page;

  g_return_if_fail (IDE_IS_EDITOR_SURFACE (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  if (ide_editor_surface_focus_if_found (self, buffer, FALSE))
    return;

  stack = ide_grid_get_current_stack (self->grid);

  page = g_object_new (IDE_TYPE_EDITOR_PAGE,
                       "buffer", buffer,
                       "visible", TRUE,
                       NULL);

  gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (page));
}

/**
 * ide_editor_surface_get_active_page:
 * @self: a #IdeEditorSurface
 *
 * Gets the active page for the surface, or %NULL if there is not one.
 *
 * Returns: (nullable) (transfer none): An #IdePage or %NULL.
 *
 * Since: 3.32
 */
IdePage *
ide_editor_surface_get_active_page (IdeEditorSurface *self)
{
  IdeFrame *stack;

  g_return_val_if_fail (IDE_IS_EDITOR_SURFACE (self), NULL);

  stack = ide_grid_get_current_stack (self->grid);

  return ide_frame_get_visible_child (stack);
}

/**
 * ide_editor_surface_get_sidebar:
 * @self: a #IdeEditorSurface
 *
 * Gets the #IdeEditorSidebar for the editor surface.
 *
 * Returns: (transfer none): an #IdeEditorSidebar
 *
 * Since: 3.32
 */
IdeEditorSidebar *
ide_editor_surface_get_sidebar (IdeEditorSurface *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SURFACE (self), NULL);

  return IDE_EDITOR_SIDEBAR (dzl_dock_bin_get_left_edge (DZL_DOCK_BIN (self)));
}

/**
 * ide_editor_surface_get_transient_sidebar:
 * @self: a #IdeEditorSurface
 *
 * Gets the transient sidebar for the editor surface.
 *
 * The transient sidebar is a sidebar on the right side of the surface. It
 * is displayed only when necessary. It animates in and out of page based on
 * focus tracking and other heuristics.
 *
 * Returns: (transfer none): An #IdeTransientSidebar
 *
 * Since: 3.32
 */
IdeTransientSidebar *
ide_editor_surface_get_transient_sidebar (IdeEditorSurface *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SURFACE (self), NULL);

  return IDE_TRANSIENT_SIDEBAR (dzl_dock_bin_get_right_edge (DZL_DOCK_BIN (self)));
}

/**
 * ide_editor_surface_get_utilities:
 *
 * Returns: (transfer none): An #IdeEditorUtilities
 *
 * Since: 3.32
 */
GtkWidget *
ide_editor_surface_get_utilities (IdeEditorSurface *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SURFACE (self), NULL);

  return dzl_dock_bin_get_bottom_edge (DZL_DOCK_BIN (self));
}

/**
 * ide_editor_surface_get_overlay:
 * @self: a #IdeEditorSurface
 *
 * Gets the overlay widget which can be used to layer things above all
 * items in the layout grid.
 *
 * Returns: (transfer none) (type Gtk.Overlay): a #GtkWidget
 *
 * Since: 3.32
 */
GtkWidget *
ide_editor_surface_get_overlay (IdeEditorSurface *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SURFACE (self), NULL);

  return GTK_WIDGET (self->overlay);
}

void
_ide_editor_surface_set_loading (IdeEditorSurface *self,
                                 gboolean          loading)
{
  g_return_if_fail (IDE_IS_EDITOR_SURFACE (self));

  gtk_widget_set_visible (GTK_WIDGET (self->grid), !loading);
  gtk_stack_set_visible_child_name (self->loading_stack,
                                    loading ? "empty_state" : "grid");
}

/**
 * ide_editor_surface_new:
 *
 * Returns: (transfer full): Creates a new #IdeEditorSurface
 *
 * Since: 3.32
 */
IdeSurface *
ide_editor_surface_new (void)
{
  return g_object_new (IDE_TYPE_EDITOR_SURFACE, NULL);
}
