/* gb-workbench.c
 *
 * Copyright (C) 2014-2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-workbench"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-dnd.h"
#include "gb-editor-document.h"
#include "gb-settings.h"
#include "gb-widget.h"
#include "gb-workbench-actions.h"
#include "gb-workbench-private.h"
#include "gb-workbench.h"
#include "gb-workbench-addin.h"
#include "gb-workspace.h"
#include "gb-workspace-pane.h"
#include "gb-project-file.h"
#include "gb-project-tree.h"
#include "gb-view-grid.h"

G_DEFINE_TYPE (GbWorkbench, gb_workbench, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_ACTIVE_VIEW,
  PROP_BUILDING,
  PROP_CONTEXT,
  LAST_PROP
};

enum {
  UNLOAD,
  LAST_SIGNAL
};

enum {
  TARGET_URI_LIST = 100
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];
static const GtkTargetEntry gDropTypes[] = {
  { "text/uri-list", 0, TARGET_URI_LIST}
};

static void
gb_workbench_save_panel_state (GbWorkbench *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (GB_IS_WORKBENCH (self));

  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = gb_workspace_get_left_pane (self->workspace);
  gtk_container_child_get (GTK_CONTAINER (self->workspace), pane,
                           "reveal", &reveal,
                           "position", &position,
                           NULL);
  g_settings_set_boolean (settings, "left-visible", reveal);
  g_settings_set_int (settings, "left-position", position);

  pane = gb_workspace_get_right_pane (self->workspace);
  gtk_container_child_get (GTK_CONTAINER (self->workspace), pane,
                           "reveal", &reveal,
                           "position", &position,
                           NULL);
  g_settings_set_boolean (settings, "right-visible", reveal);
  g_settings_set_int (settings, "right-position", position);

  pane = gb_workspace_get_bottom_pane (self->workspace);
  gtk_container_child_get (GTK_CONTAINER (self->workspace), pane,
                           "reveal", &reveal,
                           "position", &position,
                           NULL);
  g_settings_set_boolean (settings, "bottom-visible", reveal);
  g_settings_set_int (settings, "bottom-position", position);
}

static void
gb_workbench_restore_panel_state (GbWorkbench *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (GB_IS_WORKBENCH (self));

  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = gb_workspace_get_left_pane (self->workspace);
  reveal = g_settings_get_boolean (settings, "left-visible");
  position = g_settings_get_int (settings, "left-position");
  gtk_container_child_set (GTK_CONTAINER (self->workspace), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);

  pane = gb_workspace_get_right_pane (self->workspace);
  reveal = g_settings_get_boolean (settings, "right-visible");
  position = g_settings_get_int (settings, "right-position");
  gtk_container_child_set (GTK_CONTAINER (self->workspace), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);

  pane = gb_workspace_get_bottom_pane (self->workspace);
  reveal = g_settings_get_boolean (settings, "bottom-visible");
  position = g_settings_get_int (settings, "bottom-position");
  gtk_container_child_set (GTK_CONTAINER (self->workspace), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);
}

static void
gb_workbench__project_notify_name_cb (GbWorkbench *self,
                                      GParamSpec  *pspec,
                                      IdeProject  *project)
{
  g_autofree gchar *title = NULL;
  const gchar *name;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (IDE_IS_PROJECT (project));

  name = ide_project_get_name (project);

  if (!ide_str_empty0 (name))
    title = g_strdup_printf (_("%s - Builder"), name);
  else
    title = g_strdup (_("Builder"));

  gtk_window_set_title (GTK_WINDOW (self), title);
}

static void
gb_workbench__context_restore_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeContext *context = (IdeContext *)object;
  g_autoptr(GbWorkbench) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (IDE_IS_CONTEXT (context));

  if (!ide_context_restore_finish (context, result, &error))
    {
      g_warning ("%s", error->message);
    }

  gtk_widget_grab_focus (GTK_WIDGET (self->workspace));
}

static void
load_buffer_cb (GbWorkbench      *self,
                IdeBuffer        *buffer,
                IdeBufferManager *buffer_manager)
{
  IDE_ENTRY;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GB_IS_EDITOR_DOCUMENT (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  IDE_TRACE_MSG ("Loading %s.", ide_buffer_get_title (buffer));

  gb_view_grid_focus_document (self->view_grid, GB_DOCUMENT (buffer));

  IDE_EXIT;
}

static void
notify_focus_buffer_cb (GbWorkbench      *self,
                        GParamSpec       *pspec,
                        IdeBufferManager *buffer_manager)
{
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  buffer = ide_buffer_manager_get_focus_buffer (buffer_manager);

  if (buffer != NULL)
    {
      IDE_TRACE_MSG ("Focusing %s.", ide_buffer_get_title (buffer));
      gb_view_grid_focus_document (self->view_grid, GB_DOCUMENT (buffer));
    }

  IDE_EXIT;
}

static void
gb_workbench_setup_buffers (GbWorkbench *self,
                            IdeContext  *context)
{
  IdeBufferManager *bufmgr;
  g_autoptr(GPtrArray) buffers = NULL;
  gsize i;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (IDE_IS_CONTEXT (context));

  bufmgr = ide_context_get_buffer_manager (context);
  g_signal_connect_object (bufmgr,
                           "load-buffer",
                           G_CALLBACK (load_buffer_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (bufmgr,
                           "notify::focus-buffer",
                           G_CALLBACK (notify_focus_buffer_cb),
                           self,
                           G_CONNECT_SWAPPED);

  buffers = ide_buffer_manager_get_buffers (bufmgr);

  for (i = 0; i < buffers->len; i++)
    {
      IdeBuffer *buffer = g_ptr_array_index (buffers, i);
      load_buffer_cb (self, buffer, bufmgr);
    }
}

static void
gb_workbench_connect_context (GbWorkbench *self,
                              IdeContext  *context)
{
  IdeProject *project;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (IDE_IS_CONTEXT (context));

  gb_project_tree_set_context (self->project_tree, context);

  gb_workbench_setup_buffers (self, context);

  project = ide_context_get_project (context);

  self->project_notify_name_handler =
    g_signal_connect_object (project,
                             "notify::name",
                             G_CALLBACK (gb_workbench__project_notify_name_cb),
                             self,
                             G_CONNECT_SWAPPED);
  gb_workbench__project_notify_name_cb (self, NULL, project);
}

static void
gb_workbench_disconnect_context (GbWorkbench *self,
                                 IdeContext  *context)
{
  IdeProject *project;

  g_assert (GB_IS_WORKBENCH (self));
  g_assert (IDE_IS_CONTEXT (context));

  project = ide_context_get_project (context);
  ide_clear_signal_handler (project, &self->project_notify_name_handler);
}

static void
gb_workbench_set_context (GbWorkbench *self,
                          IdeContext  *context)
{
  g_return_if_fail (GB_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  if (context != self->context)
    {
      if (self->context != NULL)
        {
          gb_workbench_disconnect_context (self, context);
          g_clear_object (&self->context);
        }

      if (context != NULL)
        {
          self->context = g_object_ref (context);
          gb_workbench_connect_context (self, context);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_CONTEXT]);
    }
}

static void
gb_workbench__unload_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeContext *context = (IdeContext *)object;
  g_autoptr(GbWorkbench) self = user_data;
  GError *error = NULL;

  if (!ide_context_unload_finish (context, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  self->unloading = FALSE;
  g_clear_object (&self->context);
  gtk_window_close (GTK_WINDOW (self));
}

static gboolean
gb_workbench_delete_event (GtkWidget   *widget,
                           GdkEventAny *event)
{
  GbWorkbench *self = (GbWorkbench *)widget;

  g_assert (GB_IS_WORKBENCH (self));

  if (self->unloading)
    {
      /* Second attempt to kill things, cancel clean shutdown */
      if (!g_cancellable_is_cancelled (self->unload_cancellable))
        {
          g_cancellable_cancel (self->unload_cancellable);
          return TRUE;
        }

      /* third attempt, kill it */
      return FALSE;
    }

  g_assert (self->unloading == FALSE);

  if (self->context != NULL)
    {
      g_assert (self->unload_cancellable == NULL);

      self->unloading = TRUE;
      self->unload_cancellable = g_cancellable_new ();
      g_signal_emit (self, gSignals [UNLOAD], 0, self->context);
      ide_context_unload_async (self->context,
                                self->unload_cancellable,
                                gb_workbench__unload_cb,
                                g_object_ref (self));
      return TRUE;
    }

  gb_workbench_save_panel_state (self);

  return FALSE;
}

static gboolean
gb_workbench_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  GbWorkbench *self = (GbWorkbench *)widget;
  GtkStyleContext *style_context;
  gboolean ret;

  g_assert (GB_IS_WORKBENCH (self));

  style_context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (style_context);
  if (self->building)
    gtk_style_context_add_class (style_context, "building");
  ret = GTK_WIDGET_CLASS (gb_workbench_parent_class)->draw (widget, cr);
  gtk_style_context_restore (style_context);

  return ret;
}

static void
gb_workbench_drag_data_received (GtkWidget        *widget,
                                 GdkDragContext   *context,
                                 gint              x,
                                 gint              y,
                                 GtkSelectionData *selection_data,
                                 guint             info,
                                 guint             timestamp)
{
  GbWorkbench *self = (GbWorkbench *)widget;
  gchar **uri_list;
  gboolean handled = FALSE;

  g_assert (GB_IS_WORKBENCH (self));

  switch (info)
    {
    case TARGET_URI_LIST:
      uri_list = gb_dnd_get_uri_list (selection_data);

      if (uri_list)
        {
          gb_workbench_open_uri_list (self, (const gchar * const *)uri_list);
          g_strfreev (uri_list);
        }

      handled = TRUE;
      break;

    default:
      break;
    }

  gtk_drag_finish (context, handled, FALSE, timestamp);
}

static void
gb_workbench_grab_focus (GtkWidget *widget)
{
  GbWorkbench *self = (GbWorkbench *)widget;

  g_assert (GB_IS_WORKBENCH (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->workspace));
}

static void
gb_workbench_realize (GtkWidget *widget)
{
  GbWorkbench *self = (GbWorkbench *)widget;

  gb_workbench_restore_panel_state (self);

  if (GTK_WIDGET_CLASS (gb_workbench_parent_class)->realize)
    GTK_WIDGET_CLASS (gb_workbench_parent_class)->realize (widget);

  gtk_widget_grab_focus (GTK_WIDGET (self->workspace));

  ide_context_restore_async (self->context,
                             NULL,
                             gb_workbench__context_restore_cb,
                             g_object_ref (self));
}

static void
gb_workbench__extension_added (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               GbWorkbenchAddin *addin)
{
  gb_workbench_addin_load (addin);
}

static void
gb_workbench__extension_removed (PeasExtensionSet *set,
                                 PeasPluginInfo   *plugin_info,
                                 GbWorkbenchAddin *addin)
{
  gb_workbench_addin_unload (addin);
}


static void
gb_workbench_constructed (GObject *object)
{
  GbWorkbench *self = (GbWorkbench *)object;
  GtkApplication *app;
  GMenu *menu;

  IDE_ENTRY;

  G_OBJECT_CLASS (gb_workbench_parent_class)->constructed (object);

  gb_workbench_actions_init (self);

  app = GTK_APPLICATION (g_application_get_default ());
  menu = gtk_application_get_menu_by_id (app, "gear-menu");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->gear_menu_button), G_MENU_MODEL (menu));

  self->extensions = peas_extension_set_new (peas_engine_get_default (),
                                             GB_TYPE_WORKBENCH_ADDIN,
                                             "workbench", self,
                                             NULL);
  peas_extension_set_foreach (self->extensions,
                              (PeasExtensionSetForeachFunc)gb_workbench__extension_added,
                              NULL);
  g_signal_connect (self->extensions,
                    "extension-added",
                    G_CALLBACK (gb_workbench__extension_added),
                    self);
  g_signal_connect (self->extensions,
                    "extension-removed",
                    G_CALLBACK (gb_workbench__extension_removed),
                    self);

  gtk_widget_grab_focus (GTK_WIDGET (self->workspace));

  IDE_EXIT;
}

static void
gb_workbench_active_view_unref (gpointer  data,
                                GObject  *where_object_was)
{
  GbWorkbench *self = data;

  g_assert (GB_IS_WORKBENCH (self));

  self->active_view = NULL;
  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ACTIVE_VIEW]);
}

static void
gb_workbench_set_focus (GtkWindow *window,
                        GtkWidget *widget)
{
  GbWorkbench *self = (GbWorkbench *)window;
  GtkWidget *active_view = NULL;

  g_assert (GTK_IS_WINDOW (window));
  g_assert (!widget || GTK_IS_WIDGET (widget));

  if (widget != NULL)
    active_view = gtk_widget_get_ancestor (widget, GB_TYPE_VIEW);

  if ((active_view == NULL) || (active_view == self->active_view))
    goto chainup;

  if (self->active_view != NULL)
    {
      g_object_weak_unref (G_OBJECT (self->active_view),
                           gb_workbench_active_view_unref,
                           self);
      self->active_view = NULL;
    }

  if (active_view != NULL)
    {
      self->active_view = active_view;
      g_object_weak_ref (G_OBJECT (self->active_view),
                         gb_workbench_active_view_unref,
                         self);
    }

  g_object_notify_by_pspec (G_OBJECT (window), gParamSpecs [PROP_ACTIVE_VIEW]);

chainup:
  GTK_WINDOW_CLASS (gb_workbench_parent_class)->set_focus (window, widget);
}

static void
gb_workbench_dispose (GObject *object)
{
  GbWorkbench *self = (GbWorkbench *)object;

  IDE_ENTRY;

  self->disposing++;

  g_clear_object (&self->unload_cancellable);
  ide_clear_weak_pointer (&self->active_view);

  G_OBJECT_CLASS (gb_workbench_parent_class)->dispose (object);

  self->disposing--;

  IDE_EXIT;
}

static void
gb_workbench_finalize (GObject *object)
{
  GbWorkbench *self = (GbWorkbench *)object;

  IDE_ENTRY;

  g_clear_object (&self->context);
  g_clear_pointer (&self->current_folder_uri, g_free);
  g_clear_object (&self->extensions);

  G_OBJECT_CLASS (gb_workbench_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
gb_workbench_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbWorkbench *self = (GbWorkbench *)object;

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_object (value, self->active_view);
      break;

    case PROP_BUILDING:
      g_value_set_boolean (value, self->building);
      break;

    case PROP_CONTEXT:
      g_value_set_object (value, gb_workbench_get_context (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workbench_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GbWorkbench *self = (GbWorkbench *)object;

  switch (prop_id)
    {
    case PROP_CONTEXT:
      gb_workbench_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_workbench_class_init (GbWorkbenchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->constructed = gb_workbench_constructed;
  object_class->dispose = gb_workbench_dispose;
  object_class->finalize = gb_workbench_finalize;
  object_class->get_property = gb_workbench_get_property;
  object_class->set_property = gb_workbench_set_property;

  widget_class->delete_event = gb_workbench_delete_event;
  widget_class->drag_data_received = gb_workbench_drag_data_received;
  widget_class->draw = gb_workbench_draw;
  widget_class->grab_focus = gb_workbench_grab_focus;
  widget_class->realize = gb_workbench_realize;

  window_class->set_focus = gb_workbench_set_focus;

  gParamSpecs [PROP_ACTIVE_VIEW] =
    g_param_spec_object ("active-view",
                         _("Active View"),
                         _("Active View"),
                         GB_TYPE_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_BUILDING] =
    g_param_spec_boolean ("building",
                          _("Building"),
                          _("If the project is currently building."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GbWorkbench:context:
   *
   * The "context" property is the #IdeContext that shall be worked upon in
   * the #GbWorkbench. This must be set during workbench creation. Use
   * another window or dialog to choose the project information before
   * creating a workbench window.
   */
  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The IdeContext for the workbench."),
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gSignals [UNLOAD] =
    g_signal_new ("unload",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_CONTEXT);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-workbench.ui");
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, gear_menu_button);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, search_box);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, slider);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, workspace);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, project_tree);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, view_grid);

  g_type_ensure (GB_TYPE_PROJECT_TREE);
  g_type_ensure (GB_TYPE_SEARCH_BOX);
  g_type_ensure (GB_TYPE_SLIDER);
  g_type_ensure (GB_TYPE_VIEW_GRID);
  g_type_ensure (GB_TYPE_WORKSPACE);
  g_type_ensure (GB_TYPE_WORKSPACE_PANE);
  g_type_ensure (GEDIT_TYPE_MENU_STACK_SWITCHER);
}

static void
gb_workbench_init (GbWorkbench *self)
{
  IDE_ENTRY;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Drag and drop support*/
  gtk_drag_dest_set (GTK_WIDGET (self),
                     (GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP),
                     gDropTypes, G_N_ELEMENTS (gDropTypes), GDK_ACTION_COPY);

  gb_settings_init_window (GTK_WINDOW (self));

  IDE_EXIT;
}

/**
 * gb_workbench_get_context:
 * @self: A #GbWorkbench.
 *
 * Gets the #IdeContext for the workbench.
 *
 * Returns: (transfer none): An #IdeContext.
 */
IdeContext *
gb_workbench_get_context (GbWorkbench *self)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), NULL);

  return self->context;
}

static gboolean
supports_content_type (const gchar *filename,
                       const gchar *content_type)
{
  GtkSourceLanguageManager *languages;
  GtkSourceLanguage *language;
  g_autofree gchar *text_type = NULL;

  /* TODO: This really belongs in it's own module, or as part of buffermanager */

  languages = gtk_source_language_manager_get_default ();
  language = gtk_source_language_manager_guess_language (languages, filename, content_type);

  if (language != NULL)
    return TRUE;

  text_type = g_content_type_from_mime_type ("text/plain");
  return g_content_type_is_a (content_type, text_type);
}

static void
gb_workbench__query_info_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  IdeBufferManager *buffer_manager;
  GFile *file = (GFile *)object;
  IdeProject *project;
  g_autoptr(IdeFile) idefile = NULL;
  g_autoptr(GbWorkbench) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  const gchar *content_type;
  const gchar *name;

  file_info = g_file_query_info_finish (file, result, &error);

  if (error)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        goto open_buffer;
      g_warning ("%s", error->message);
      return;
    }

  g_assert (G_IS_FILE_INFO (file_info));

  name = g_file_info_get_name (file_info);
  content_type = g_file_info_get_attribute_string (file_info,
                                                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

  /* Hrmm, what to do .. */
  if (!content_type)
    return;

  g_debug ("Open with content_type=\"%s\"", content_type);

  /* If this doesn't look like text, let's open it with xdg-open */
  if (!supports_content_type (name, content_type))
    {
      g_autofree gchar *uri = NULL;

      uri = g_file_get_uri (file);
      g_app_info_launch_default_for_uri (uri, NULL, NULL);
      return;
    }

open_buffer:
  if (self->context == NULL)
    {
      /* Must be shutting down. */
      return;
    }

  buffer_manager = ide_context_get_buffer_manager (self->context);
  project = ide_context_get_project (self->context);
  idefile = ide_project_get_project_file (project, file);
  ide_buffer_manager_load_file_async (buffer_manager, idefile, FALSE, NULL, NULL, NULL, NULL);
}

void
gb_workbench_open (GbWorkbench *self,
                   GFile       *file)
{
  g_return_if_fail (GB_IS_WORKBENCH (self));
  g_return_if_fail (self->unloading == FALSE);
  g_return_if_fail (self->context);

  self->has_opened = TRUE;

  /*
   * TODO: We probably want to dispatch this based on the type. But for now,
   *       we will just try to open it with the buffer manager.
   */

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           NULL,
                           gb_workbench__query_info_cb,
                           g_object_ref (self));
}

void
gb_workbench_open_with_editor (GbWorkbench *self,
                               GFile       *file)
{
  IdeBufferManager *buffer_manager;
  IdeProject *project;
  g_autoptr(IdeFile) idefile = NULL;

  g_return_if_fail (GB_IS_WORKBENCH (self));
  g_return_if_fail (self->unloading == FALSE);
  g_return_if_fail (self->context);

  buffer_manager = ide_context_get_buffer_manager (self->context);
  project = ide_context_get_project (self->context);
  idefile = ide_project_get_project_file (project, file);
  ide_buffer_manager_load_file_async (buffer_manager, idefile, FALSE, NULL, NULL, NULL, NULL);
}

void
gb_workbench_open_uri_list (GbWorkbench         *self,
                            const gchar * const *uri_list)
{
  gsize i;

  g_return_if_fail (GB_IS_WORKBENCH (self));

  for (i = 0; uri_list [i]; i++)
    {
      g_autoptr(GFile) file = NULL;

      file = g_file_new_for_uri (uri_list [i]);
      gb_workbench_open (self, file);
    }
}

static void
gb_workbench__builder_build_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr(GbWorkbench) self = user_data;
  g_autoptr(GError) error = NULL;
  IdeBuilder *builder = (IdeBuilder *)object;
  IdeBuildResult *build_result;

  g_assert (IDE_IS_BUILDER (builder));
  g_assert (GB_IS_WORKBENCH (self));

  self->building = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_BUILDING]);

  build_result = ide_builder_build_finish (builder, result, &error);

  if (error)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Build Failure"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_window_present (GTK_WINDOW (dialog));
    }

  g_clear_object (&build_result);
}

void
gb_workbench_build_async (GbWorkbench         *self,
                          gboolean             force_rebuild,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  IdeDeviceManager *device_manager;
  IdeBuildSystem *build_system;
  IdeContext *context;
  IdeDevice *device;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(GKeyFile) config = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (GB_IS_WORKBENCH (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = gb_workbench_get_context (self);

  /* TODO: Get build device from workbench combo? */
  device_manager = ide_context_get_device_manager (context);
  device = ide_device_manager_get_device (device_manager, "local");

  build_system = ide_context_get_build_system (context);

  config = g_key_file_new ();

  builder = ide_build_system_get_builder (build_system, config, device, &error);

  if (builder == NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Project build system does not support building"));
      if (error && error->message)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", error->message);
      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_window_present (GTK_WINDOW (dialog));
      return;
    }

  self->building = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_BUILDING]);

  ide_builder_build_async (builder,
                           force_rebuild ? IDE_BUILDER_BUILD_FLAGS_FORCE_REBUILD : 0,
                           NULL, /* &IdeProgress */
                           cancellable,
                           gb_workbench__builder_build_cb,
                           g_object_ref (self));
}

gboolean
gb_workbench_build_finish (GbWorkbench   *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (GB_IS_WORKBENCH (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

void
gb_workbench_add_temporary_buffer (GbWorkbench *self)
{
  IdeContext *context;
  IdeBufferManager *buffer_manager;
  IdeBuffer *buffer;

  g_return_if_fail (GB_IS_WORKBENCH (self));

  context = gb_workbench_get_context (self);
  buffer_manager = ide_context_get_buffer_manager (context);
  buffer = ide_buffer_manager_create_buffer (buffer_manager);

  g_clear_object (&buffer);
}

gboolean
gb_workbench_get_closing (GbWorkbench *self)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), FALSE);

  return (self->unloading || (self->disposing > 0));
}

void
gb_workbench_views_foreach (GbWorkbench *self,
                            GtkCallback  callback,
                            gpointer     callback_data)
{
  g_return_if_fail (GB_IS_WORKBENCH (self));
  g_return_if_fail (callback != NULL);

  //gb_workspace_views_foreach (GB_WORKSPACE (self->editor_workspace), callback, callback_data);
}

GtkWidget *
gb_workbench_get_workspace (GbWorkbench *self)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), NULL);

  return GTK_WIDGET (self->workspace);
}

GtkWidget *
gb_workbench_get_view_grid (GbWorkbench *self)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), NULL);

  return GTK_WIDGET (self->view_grid);
}

static gboolean
find_files_node (GbTree     *tree,
                 GbTreeNode *node,
                 GbTreeNode *child,
                 gpointer    user_data)
{
  GObject *item;

  g_assert (GB_IS_TREE (tree));
  g_assert (GB_IS_TREE_NODE (node));
  g_assert (GB_IS_TREE_NODE (child));

  item = gb_tree_node_get_item (child);

  return GB_IS_PROJECT_FILE (item);
}

static gboolean
find_child_node (GbTree     *tree,
                 GbTreeNode *node,
                 GbTreeNode *child,
                 gpointer    user_data)
{
  const gchar *name = user_data;
  GObject *item;

  g_assert (GB_IS_TREE (tree));
  g_assert (GB_IS_TREE_NODE (node));
  g_assert (GB_IS_TREE_NODE (child));

  item = gb_tree_node_get_item (child);

  if (GB_IS_PROJECT_FILE (item))
    {
      const gchar *item_name;

      item_name = gb_project_file_get_display_name (GB_PROJECT_FILE (item));
      return ide_str_equal0 (item_name, name);
    }

  return FALSE;
}

void
gb_workbench_reveal_file (GbWorkbench *self,
                          GFile       *file)
{
  g_autofree gchar *relpath = NULL;
  g_auto(GStrv) parts = NULL;
  GbTreeNode *node;
  GbTree *tree;
  IdeVcs *vcs;
  GFile *workdir;
  gsize i;

  g_return_if_fail (GB_IS_WORKBENCH (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (self->context != NULL);

  vcs = ide_context_get_vcs (self->context);
  workdir = ide_vcs_get_working_directory (vcs);
  relpath = g_file_get_relative_path (workdir, file);
  tree = GB_TREE (self->project_tree);

  if (relpath == NULL)
    return;

  node = gb_tree_find_child_node (tree, NULL, find_files_node, NULL);
  if (node == NULL)
    return;

  parts = g_strsplit (relpath, G_DIR_SEPARATOR_S, 0);

  for (i = 0; parts [i]; i++)
    {
      node = gb_tree_find_child_node (tree, node, find_child_node, parts [i]);
      if (node == NULL)
        return;
    }

  gb_tree_expand_to_node (tree, node);
  gb_tree_scroll_to_node (tree, node);
  gb_tree_node_select (node);
}

GtkWidget *
gb_workbench_get_slider (GbWorkbench *self)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), NULL);

  return GTK_WIDGET (self->slider);
}

GtkWidget *
gb_workbench_get_active_view (GbWorkbench *self)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), NULL);

  return self->active_view;
}
