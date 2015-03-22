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

#include "gb-command-gaction-provider.h"
#include "gb-command-vim-provider.h"
#include "gb-dnd.h"
#include "gb-widget.h"
#include "gb-workbench-actions.h"
#include "gb-workbench-private.h"
#include "gb-workbench.h"
#include "gb-workspace.h"

G_DEFINE_TYPE (GbWorkbench, gb_workbench, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_ACTIVE_WORKSPACE,
  PROP_COMMAND_MANAGER,
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
gb_workbench_set_context (GbWorkbench *self,
                          IdeContext  *context)
{
  g_return_if_fail (GB_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  if (g_set_object (&self->context, context))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_CONTEXT]);
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
      g_cancellable_cancel (self->unload_cancellable);
      return TRUE;
    }

  if (self->context != NULL)
    {
      g_assert (!self->unload_cancellable);

      self->unloading = TRUE;
      self->unload_cancellable = g_cancellable_new ();
      g_signal_emit (self, gSignals [UNLOAD], 0, self->context);
      ide_context_unload_async (self->context,
                                self->unload_cancellable,
                                gb_workbench__unload_cb,
                                g_object_ref (self));
      return TRUE;
    }

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
gb_workbench_realize (GtkWidget *widget)
{
  GbWorkbench *self = (GbWorkbench *)widget;

  if (GTK_WIDGET_CLASS (gb_workbench_parent_class)->realize)
    GTK_WIDGET_CLASS (gb_workbench_parent_class)->realize (widget);

  gtk_widget_grab_focus (GTK_WIDGET (self->editor_workspace));
}

static void
gb_workbench_set_focus (GtkWindow *window,
                        GtkWidget *widget)
{
  GbWorkbench *self = (GbWorkbench *)window;

  g_return_if_fail (GB_IS_WORKBENCH (self));

  /*
   * The goal here is to focus the current workspace if we are trying to
   * clear the workbench focus (from something like the global search).
   */

  GTK_WINDOW_CLASS (gb_workbench_parent_class)->set_focus (window, widget);

  if (!widget && !self->disposing)
    {
      GbWorkspace *workspace;

      /*
       * Sadly we can't just set @widget before calling the parent set_focus()
       * implementation. It doesn't actually do anything. So instead we grab
       * the focus of the active workspace directly. We might need to check
       * for reentrancy later, but if that happens, we are probably doing
       * something else wrong.
       */
      workspace = gb_workbench_get_active_workspace (self);
      if (workspace)
        gtk_widget_grab_focus (GTK_WIDGET (workspace));
    }
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

  if (self->active_workspace)
    gtk_widget_grab_focus (GTK_WIDGET (self->active_workspace));
  else
    gtk_widget_grab_focus (GTK_WIDGET (self->editor_workspace));

  IDE_EXIT;
}

static void
gb_workbench_dispose (GObject *object)
{
  GbWorkbench *self = (GbWorkbench *)object;

  IDE_ENTRY;

  self->disposing++;

  g_clear_object (&self->command_manager);
  g_clear_object (&self->unload_cancellable);

  G_OBJECT_CLASS (gb_workbench_parent_class)->dispose (object);

  self->disposing--;

  IDE_EXIT;
}

static void
gb_workbench_finalize (GObject *object)
{
  GbWorkbench *self = (GbWorkbench *)object;

  IDE_ENTRY;

  ide_clear_weak_pointer (&self->active_workspace);
  g_clear_object (&self->context);
  g_clear_pointer (&self->current_folder_uri, g_free);

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
    case PROP_ACTIVE_WORKSPACE:
      g_value_set_object (value, gb_workbench_get_active_workspace (self));
      break;

    case PROP_COMMAND_MANAGER:
      g_value_set_object (value, gb_workbench_get_command_manager (self));
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
    case PROP_ACTIVE_WORKSPACE:
      gb_workbench_set_active_workspace (self, g_value_get_object (value));
      break;

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

  widget_class->drag_data_received = gb_workbench_drag_data_received;
  widget_class->draw = gb_workbench_draw;
  widget_class->realize = gb_workbench_realize;
  widget_class->delete_event = gb_workbench_delete_event;

  window_class->set_focus = gb_workbench_set_focus;

  gParamSpecs [PROP_ACTIVE_WORKSPACE] =
    g_param_spec_object ("active-workspace",
                         _("Active Workspace"),
                         _("The active workspace"),
                         GB_TYPE_WORKSPACE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACTIVE_WORKSPACE,
                                   gParamSpecs [PROP_ACTIVE_WORKSPACE]);

  gParamSpecs [PROP_COMMAND_MANAGER] =
    g_param_spec_object ("command-manager",
                         _("Command Manager"),
                         _("The command manager for the workbench"),
                         GB_TYPE_COMMAND_MANAGER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_COMMAND_MANAGER,
                                   gParamSpecs [PROP_COMMAND_MANAGER]);

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
  g_object_class_install_property (object_class, PROP_CONTEXT, gParamSpecs [PROP_CONTEXT]);

  gSignals [UNLOAD] =
    g_signal_new ("unload",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_CONTEXT);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-workbench.ui");
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, command_bar);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, editor_workspace);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, gear_menu_button);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, search_box);
  GB_WIDGET_CLASS_BIND (klass, GbWorkbench, stack);

  g_type_ensure (GB_TYPE_COMMAND_BAR);
  g_type_ensure (GB_TYPE_EDITOR_WORKSPACE);
  g_type_ensure (GB_TYPE_SEARCH_BOX);
  g_type_ensure (GEDIT_TYPE_MENU_STACK_SWITCHER);
}

static void
gb_workbench_init (GbWorkbench *self)
{
  g_autoptr(GbCommandProvider) gaction_provider = NULL;
  g_autoptr(GbCommandProvider) vim_provider = NULL;

  IDE_ENTRY;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->command_manager = gb_command_manager_new ();

  gaction_provider = g_object_new (GB_TYPE_COMMAND_GACTION_PROVIDER,
                                   "workbench", self,
                                   NULL);
  vim_provider = g_object_new (GB_TYPE_COMMAND_VIM_PROVIDER,
                               "workbench", self,
                               NULL);
  gb_command_manager_add_provider (self->command_manager, gaction_provider);
  gb_command_manager_add_provider (self->command_manager, vim_provider);

  /* Drag and drop support*/
  gtk_drag_dest_set (GTK_WIDGET (self),
                     (GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP),
                     gDropTypes, G_N_ELEMENTS (gDropTypes), GDK_ACTION_COPY);

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

/**
 * gb_workbench_get_active_workspace:
 * @self: A #GbWorkbench.
 *
 * Gets the currently selected workspace.
 *
 * Returns: (transfer none): An #GbWorkspace.
 */
GbWorkspace *
gb_workbench_get_active_workspace (GbWorkbench *self)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), NULL);

  return self->active_workspace;
}

void
gb_workbench_set_active_workspace (GbWorkbench *self,
                                   GbWorkspace *workspace)
{
  g_return_if_fail (GB_IS_WORKBENCH (self));
  g_return_if_fail (GB_IS_WORKSPACE (workspace));

  if (ide_set_weak_pointer (&self->active_workspace, workspace))
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (workspace));
}

static gboolean
supports_content_type (const gchar *content_type)
{
  return (g_str_has_prefix (content_type, "text/") ||
          g_str_equal (content_type, "application/x-ruby") ||
          g_str_equal (content_type, "application/xml") ||
          g_str_equal (content_type, "application/x-ruby") ||
          g_str_equal (content_type, "application/javascript") ||
          g_str_equal (content_type, "application/x-gtk-builder"));
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

  file_info = g_file_query_info_finish (file, result, &error);

  if (error)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        goto open_buffer;
      g_warning ("%s", error->message);
      return;
    }

  g_assert (G_IS_FILE_INFO (file_info));

  content_type = g_file_info_get_attribute_string (file_info,
                                                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

  /* Hrmm, what to do .. */
  if (!content_type)
    return;

  g_debug ("Open with content_type=\"%s\"", content_type);

  /* If this doesn't look like text, let's open it with xdg-open */
  if (!supports_content_type (content_type))
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

/**
 * gb_workbench_get_command_manager:
 * @self: A #GbWorkbench.
 *
 * Gets the command manager for the workbench. This may be moved into libide.
 *
 * Returns: (transfer none): A #GbCommandManager.
 */
GbCommandManager *
gb_workbench_get_command_manager (GbWorkbench *self)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), NULL);

  return self->command_manager;
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

/**
 * gb_workbench_get_workspace_typed:
 * @self: A #GbWorkbench.
 *
 * Gets the workspace matching @workspace_type
 *
 * Returns: (transfer none): A #GbWorkspace.
 */
gpointer
gb_workbench_get_workspace_typed (GbWorkbench *self,
                                  GType        workspace_type)
{
  g_return_val_if_fail (GB_IS_WORKBENCH (self), NULL);

  if (workspace_type == GB_TYPE_EDITOR_WORKSPACE)
    return self->editor_workspace;

  return NULL;
}
