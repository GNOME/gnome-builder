/* ide-workspace.c
 *
 * Copyright 2014-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-workspace"

#include "config.h"

#include <glib/gi18n.h>

#include <libpanel.h>

#include <libide-search.h>
#include <libide-plugins.h>

#include "ide-application.h"
#include "ide-gui-global.h"
#include "ide-page-private.h"
#include "ide-search-popover-private.h"
#include "ide-shortcut-bundle-private.h"
#include "ide-shortcut-window-private.h"
#include "ide-workspace-addin.h"
#include "ide-workspace-private.h"
#include "ide-workbench-private.h"

#define MUX_ACTIONS_KEY "IDE_WORKSPACE_MUX_ACTIONS"
#define GET_PRIORITY(w)   GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w),"PRIORITY"))
#define SET_PRIORITY(w,i) g_object_set_data(G_OBJECT(w),"PRIORITY",GINT_TO_POINTER(i))

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GActionGroup, g_object_unref)

typedef struct
{
  /* Used as a link in IdeWorkbench's GQueue to track the most-recently-used
   * workspaces based on recent focus.
   */
  GList mru_link;

  /* This cancellable auto-cancels when the window is destroyed using
   * ::delete-event() so that async operations can be made to auto-cancel.
   */
  GCancellable *cancellable;

  /* The context for our workbench. It may not have a project loaded until
   * the ide_workbench_load_project_async() workflow has been called, but it
   * is usable without a project (albeit restricted).
   */
  IdeContext *context;

  /* Our addins for the workspace window, that are limited by the "kind" of
   * workspace that is loaded. Plugin files can specify X-Workspace-Kind to
   * limit the plugin to specific type(s) of workspace.
   */
  IdeExtensionSetAdapter *addins;

  /* The global search for the workspace, if any */
  IdeSearchPopover *search_popover;

  /* GListModel of GtkShortcut w/ capture/bubble filters */
  GtkFilterListModel *shortcut_model_bubble;
  GtkFilterListModel *shortcut_model_capture;
  GListModel *shortcuts;

  /* A MRU that is updated as pages are focused. It allows us to move through
   * the pages in the order they've been most-recently focused.
   */
  GQueue page_mru;

  /* Queued source to save window size/etc */
  guint queued_window_save;

  /* Contains children */
  AdwToolbarView *toolbar_view;
  GtkBox *content_box;

  /* Weak pointer to the current page. */
  gpointer current_page_ptr;

  /* Inhibit desktop session logout */
  guint inhibit_logout_count;
  guint inhibit_logout_cookie;

  /* The identifier for the workspace window */
  char *id;

  /* If GSetting should be ignored for size */
  guint ignore_size_setting : 1;
} IdeWorkspacePrivate;

typedef struct
{
  IdePageCallback callback;
  gpointer        user_data;
} ForeachPage;

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_ID,
  PROP_SEARCH_POPOVER,
  PROP_TOOLBAR_STYLE,
  PROP_WORKBENCH,
  N_PROPS
};

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeWorkspace, ide_workspace, ADW_TYPE_APPLICATION_WINDOW,
                                  G_ADD_PRIVATE (IdeWorkspace)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GParamSpec *properties [N_PROPS];
static GSettings *settings;

static void
ide_workspace_attach_shortcuts (IdeWorkspace *self,
                                GtkWidget    *widget)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkEventController *controller;

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (G_IS_LIST_MODEL (priv->shortcut_model_bubble));
  g_assert (G_IS_LIST_MODEL (priv->shortcut_model_capture));

  controller = gtk_shortcut_controller_new_for_model (G_LIST_MODEL (priv->shortcut_model_capture));
  gtk_event_controller_set_name (controller, "ide-shortcuts-capture");
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  gtk_event_controller_set_propagation_limit (controller, GTK_LIMIT_NONE);
  gtk_widget_add_controller (widget, g_steal_pointer (&controller));

  controller = gtk_shortcut_controller_new_for_model (G_LIST_MODEL (priv->shortcut_model_bubble));
  gtk_event_controller_set_name (controller, "ide-shortcuts-bubble");
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
  gtk_event_controller_set_propagation_limit (controller, GTK_LIMIT_NONE);
  gtk_widget_add_controller (widget, g_steal_pointer (&controller));
}

static IdePage *
ide_workspace_get_focus_page (IdeWorkspace *self)
{
  GtkWidget *focus;

  g_assert (IDE_IS_WORKSPACE (self));

  if ((focus = gtk_root_get_focus (GTK_ROOT (self))))
    {
      if (!IDE_IS_PAGE (focus))
        focus = gtk_widget_get_ancestor (focus, IDE_TYPE_PAGE);
    }

  return IDE_PAGE (focus);
}

static void
ide_workspace_addin_added_cb (IdeExtensionSetAdapter *set,
                              PeasPluginInfo         *plugin_info,
                              GObject          *exten,
                              gpointer                user_data)
{
  IdeWorkspaceAddin *addin = (IdeWorkspaceAddin *)exten;
  g_autoptr(GActionGroup) action_group = NULL;
  IdeWorkspace *self = user_data;
  IdePage *page;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (self));

  g_debug ("Loading workspace addin from module %s",
           peas_plugin_info_get_module_name (plugin_info));

  g_object_set_data (G_OBJECT (addin), "PEAS_PLUGIN_INFO", plugin_info);

  ide_workspace_addin_load (addin, self);

  if ((action_group = ide_workspace_addin_ref_action_group (addin)))
    {
      IdeActionMuxer *muxer = ide_action_mixin_get_action_muxer (self);
      ide_action_muxer_insert_action_group (muxer,
                                            peas_plugin_info_get_module_name (plugin_info),
                                            action_group);
    }

  if ((page = ide_workspace_get_focus_page (self)))
    ide_workspace_addin_page_changed (addin, page);
}

static void
ide_workspace_addin_removed_cb (IdeExtensionSetAdapter *set,
                                PeasPluginInfo         *plugin_info,
                                GObject          *exten,
                                gpointer                user_data)
{
  IdeWorkspaceAddin *addin = (IdeWorkspaceAddin *)exten;
  IdeWorkspace *self = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (self));

  g_debug ("Unloading workspace addin from module %s",
           peas_plugin_info_get_module_name (plugin_info));

  ide_action_muxer_insert_action_group (ide_action_mixin_get_action_muxer (self),
                                        peas_plugin_info_get_module_name (plugin_info),
                                        NULL);

  ide_workspace_addin_page_changed (addin, NULL);
  ide_workspace_addin_unload (addin, self);
}

static void
ide_workspace_addin_page_changed_cb (IdeExtensionSetAdapter *set,
                                     PeasPluginInfo         *plugin_info,
                                     GObject          *exten,
                                     gpointer                user_data)
{
  IdeWorkspaceAddin *addin = (IdeWorkspaceAddin *)exten;
  IdePage *page = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (!page || IDE_IS_PAGE (page));

  ide_workspace_addin_page_changed (addin, page);
}

static void
ide_workspace_real_context_set (IdeWorkspace *self,
                                IdeContext   *context)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (IDE_IS_CONTEXT (context));

  priv->addins = ide_extension_set_adapter_new (NULL,
                                                NULL,
                                                IDE_TYPE_WORKSPACE_ADDIN,
                                                "Workspace-Kind",
                                                IDE_WORKSPACE_GET_CLASS (self)->kind);

  g_signal_connect (priv->addins,
                    "extension-added",
                    G_CALLBACK (ide_workspace_addin_added_cb),
                    self);

  g_signal_connect (priv->addins,
                    "extension-removed",
                    G_CALLBACK (ide_workspace_addin_removed_cb),
                    self);

  ide_extension_set_adapter_foreach (priv->addins,
                                     ide_workspace_addin_added_cb,
                                     self);
}

static void
ide_workspace_close_request_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeWorkspace *self = (IdeWorkspace *)object;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (IDE_WORKSPACE_GET_CLASS (self)->agree_to_close_finish (self, result, &error))
    {
      IdeWorkbench *workbench = IDE_WORKBENCH (gtk_window_get_group (GTK_WINDOW (self)));

      if (ide_workbench_has_project (workbench) &&
          _ide_workbench_is_last_workspace (workbench, self))
        {
          gtk_widget_hide (GTK_WIDGET (self));
          ide_workbench_unload_async (workbench, NULL, NULL, NULL);
          return;
        }

      g_cancellable_cancel (priv->cancellable);
      ide_workbench_remove_workspace (workbench, self);
      gtk_window_destroy (GTK_WINDOW (self));
    }
}

static gboolean
ide_workspace_close_request (GtkWindow *window)
{
  IdeWorkspace *self = (IdeWorkspace *)window;

  g_assert (IDE_IS_WORKSPACE (self));

  IDE_WORKSPACE_GET_CLASS (self)->agree_to_close_async (self,
                                                        NULL,
                                                        ide_workspace_close_request_cb,
                                                        NULL);

  return TRUE;
}

static void
ide_workspace_notify_focus_widget (IdeWorkspace *self,
                                   GParamSpec   *pspec,
                                   gpointer      user_data)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  IdePage *focus;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (pspec != NULL);
  g_assert (user_data == NULL);

  focus = ide_workspace_get_focus_page (self);

  if (priv->current_page_ptr != (gpointer)focus)
    {
      /* Focus changed, but old page is still valid */
      if (focus == NULL)
        IDE_EXIT;

      /* Focus changed, and we have a new widget */
      g_set_weak_pointer (&priv->current_page_ptr, focus);

      /* And move this page to the front of the MRU */
      _ide_workspace_move_front_page_mru (self, _ide_page_get_mru_link (focus));

      if (priv->addins != NULL)
        {
          g_object_ref (focus);
          ide_extension_set_adapter_foreach (priv->addins,
                                             ide_workspace_addin_page_changed_cb,
                                             focus);
          g_object_unref (focus);
        }
    }

  IDE_EXIT;
}

/**
 * ide_workspace_class_set_kind:
 * @klass: a #IdeWorkspaceClass
 *
 * Sets the shorthand name for the kind of workspace. This is used to limit
 * what #IdeWorkspaceAddin may load within the workspace.
 */
void
ide_workspace_class_set_kind (IdeWorkspaceClass *klass,
                              const gchar       *kind)
{
  g_return_if_fail (IDE_IS_WORKSPACE_CLASS (klass));

  klass->kind = g_intern_string (kind);
}

static void
ide_workspace_real_foreach_page (IdeWorkspace    *self,
                                 IdePageCallback  callback,
                                 gpointer         user_data)
{
  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (callback != NULL);
}

static void
ide_workspace_agree_to_close_async (IdeWorkspace        *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_workspace_agree_to_close_finish (IdeWorkspace *self,
                                     GAsyncResult *result,
                                     GError **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
ide_workspace_save_settings (gpointer data)
{
  IdeWorkspace *self = data;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GdkRectangle geom = {0};
  gboolean maximized;

  g_assert (IDE_IS_WORKSPACE (self));

  priv->queued_window_save = 0;

  if (!gtk_widget_get_realized (GTK_WIDGET (self)) ||
      !gtk_widget_get_visible (GTK_WIDGET (self)) ||
      !IDE_WORKSPACE_GET_CLASS (self)->save_size (self, &geom.width, &geom.height))
    return G_SOURCE_REMOVE;

  if (settings == NULL)
    settings = g_settings_new ("org.gnome.builder");

  maximized = gtk_window_is_maximized (GTK_WINDOW (self));

  g_settings_set (settings, "window-size", "(ii)", geom.width, geom.height);
  g_settings_set_boolean (settings, "window-maximized", maximized);

  return G_SOURCE_REMOVE;
}

static void
ide_workspace_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  IdeWorkspace *self = (IdeWorkspace *)widget;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_assert (IDE_IS_WORKSPACE (self));

  GTK_WIDGET_CLASS (ide_workspace_parent_class)->size_allocate (widget, width, height, baseline);

  if (priv->queued_window_save == 0 &&
      IDE_WORKSPACE_GET_CLASS (self)->save_size != NULL)
    priv->queued_window_save = g_timeout_add_seconds (1, ide_workspace_save_settings, self);
}

void
_ide_workspace_set_ignore_size_setting (IdeWorkspace *self,
                                        gboolean      ignore_size_setting)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));

  priv->ignore_size_setting = !!ignore_size_setting;
}

static void
ide_workspace_restore_size (IdeWorkspace *workspace,
                            int           width,
                            int           height)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (workspace);

  g_assert (IDE_IS_WORKSPACE (workspace));

  if (!priv->ignore_size_setting)
    gtk_window_set_default_size (GTK_WINDOW (workspace), width, height);
}

static gboolean
ide_workspace_save_size (IdeWorkspace *workspace,
                         int          *width,
                         int          *height)
{
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_window_get_default_size (GTK_WINDOW (workspace), width, height);

  return TRUE;
}

static void
ide_workspace_realize (GtkWidget *widget)
{
  IdeWorkspace *self = (IdeWorkspace *)widget;
  GdkRectangle geom = {0};
  gboolean maximized = FALSE;

  g_assert (IDE_IS_WORKSPACE (self));

  if (settings == NULL)
    settings = g_settings_new ("org.gnome.builder");

  g_settings_get (settings, "window-size", "(ii)", &geom.width, &geom.height);
  g_settings_get (settings, "window-maximized", "b", &maximized);

  if (IDE_WORKSPACE_GET_CLASS (self)->restore_size)
    IDE_WORKSPACE_GET_CLASS (self)->restore_size (self, geom.width, geom.height);

  GTK_WIDGET_CLASS (ide_workspace_parent_class)->realize (widget);

  if (IDE_WORKSPACE_GET_CLASS (self)->restore_size)
    {
      if (maximized)
        gtk_window_maximize (GTK_WINDOW (self));
    }
}

static IdeFrame *
ide_workspace_real_get_most_recent_frame (IdeWorkspace *self)
{
  IdePage *page;

  g_assert (IDE_IS_WORKSPACE (self));

  if (!(page = ide_workspace_get_most_recent_page (self)))
    return NULL;

  return IDE_FRAME (gtk_widget_get_ancestor (GTK_WIDGET (page), IDE_TYPE_FRAME));
}

static gboolean
ide_workspace_real_can_search (IdeWorkspace *self)
{
  return FALSE;
}

static IdeHeaderBar *
ide_workspace_real_get_header_bar (IdeWorkspace *workspace)
{
  return NULL;
}

static void
ide_workspace_action_close (gpointer    instance,
                            const char *action_name,
                            GVariant   *param)
{
  IdeWorkspace *self = instance;

  g_assert (IDE_IS_WORKSPACE (self));

  gtk_window_close (GTK_WINDOW (self));
}

static void
ide_workspace_action_help_overlay (gpointer    instance,
                                   const char *action_name,
                                   GVariant   *param)
{
  IdeWorkspace *self = instance;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkWidget *window;

  g_assert (IDE_IS_WORKSPACE (self));

  if ((window = ide_shortcut_window_new (priv->shortcuts)))
    adw_dialog_present (ADW_DIALOG (window), GTK_WIDGET (self));
}

static void
ide_workspace_action_focus_last_page (gpointer    instance,
                                      const char *action_name,
                                      GVariant   *param)
{
  IdeWorkspace *self = instance;
  IdePage *page;

  g_assert (IDE_IS_WORKSPACE (self));

  if ((page = ide_workspace_get_most_recent_page (self)))
    {
      panel_widget_raise (PANEL_WIDGET (page));
      gtk_widget_grab_focus (GTK_WIDGET (page));
    }
}

static void
ide_workspace_constructed (GObject *object)
{
  IdeWorkspace *self = (IdeWorkspace *)object;
  IdeActionMuxer *muxer;

  G_OBJECT_CLASS (ide_workspace_parent_class)->constructed (object);

  ide_action_mixin_constructed (&IDE_WORKSPACE_GET_CLASS (self)->action_mixin, object);
  muxer = ide_action_mixin_get_action_muxer (self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "workspace", G_ACTION_GROUP (muxer));
}

static void
ide_workspace_dispose (GObject *object)
{
  IdeWorkspace *self = (IdeWorkspace *)object;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkWindowGroup *group;

  g_assert (IDE_IS_WORKSPACE (self));

  g_clear_object (&priv->search_popover);

  g_clear_weak_pointer (&priv->current_page_ptr);

  /* Unload addins immediately */
  ide_clear_and_destroy_object (&priv->addins);

  /* Unload shortcut models */
  g_clear_object (&priv->shortcut_model_bubble);
  g_clear_object (&priv->shortcut_model_capture);
  g_clear_object (&priv->shortcuts);

  /* Remove the workspace from the workbench MRU/etc */
  group = gtk_window_get_group (GTK_WINDOW (self));
  if (IDE_IS_WORKBENCH (group))
    ide_workbench_remove_workspace (IDE_WORKBENCH (group), self);

  /* Chain up to ensure the GtkWindow cleans up any widgets or other
   * state attached to the workspace. We keep the context alive during
   * this process.
   */
  G_OBJECT_CLASS (ide_workspace_parent_class)->dispose (object);

  /* A reference is held during this so it is safe to run code after
   * chaining up to dispose. Force release teh context now.
   */
  g_clear_object (&priv->context);
}

static void
ide_workspace_finalize (GObject *object)
{
  IdeWorkspace *self = (IdeWorkspace *)object;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_object (&priv->context);
  g_clear_object (&priv->cancellable);
  g_clear_handle_id (&priv->queued_window_save, g_source_remove);

  G_OBJECT_CLASS (ide_workspace_parent_class)->finalize (object);
}

static void
ide_workspace_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeWorkspace *self = IDE_WORKSPACE (object);
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_workspace_get_context (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_workspace_get_id (self));
      break;

    case PROP_SEARCH_POPOVER:
      g_value_set_object (value, priv->search_popover);
      break;

    case PROP_TOOLBAR_STYLE:
      g_value_set_enum (value, ide_workspace_get_toolbar_style (self));
      break;

    case PROP_WORKBENCH:
      g_value_set_object (value, ide_workspace_get_workbench (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workspace_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeWorkspace *self = IDE_WORKSPACE (object);

  switch (prop_id)
    {
    case PROP_ID:
      ide_workspace_set_id (self, g_value_get_string (value));
      break;

    case PROP_TOOLBAR_STYLE:
      ide_workspace_set_toolbar_style (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workspace_class_init (IdeWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->constructed = ide_workspace_constructed;
  object_class->dispose = ide_workspace_dispose;
  object_class->finalize = ide_workspace_finalize;
  object_class->get_property = ide_workspace_get_property;
  object_class->set_property = ide_workspace_set_property;

  widget_class->realize = ide_workspace_realize;
  widget_class->size_allocate = ide_workspace_size_allocate;

  window_class->close_request = ide_workspace_close_request;

  klass->agree_to_close_async = ide_workspace_agree_to_close_async;
  klass->agree_to_close_finish = ide_workspace_agree_to_close_finish;
  klass->can_search = ide_workspace_real_can_search;
  klass->context_set = ide_workspace_real_context_set;
  klass->foreach_page = ide_workspace_real_foreach_page;
  klass->get_most_recent_frame = ide_workspace_real_get_most_recent_frame;
  klass->restore_size = ide_workspace_restore_size;
  klass->save_size = ide_workspace_save_size;
  klass->get_header_bar = ide_workspace_real_get_header_bar;

  /**
   * IdeWorkspace:context:
   *
   * The "context" property is the #IdeContext for the workspace. This is set
   * when the workspace joins a workbench.
   */
  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The IdeContext for the workspace, inherited from workbench",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeWorkspace:id:
   *
   * The "id" property is a unique identifier for the workspace
   * within the project.
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Identifier for the workspace window",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH_POPOVER] =
    g_param_spec_object ("search-popover", NULL, NULL,
                         IDE_TYPE_SEARCH_POPOVER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TOOLBAR_STYLE] =
    g_param_spec_enum ("toolbar-style", NULL, NULL,
                       ADW_TYPE_TOOLBAR_STYLE,
                       ADW_TOOLBAR_RAISED,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_WORKBENCH] =
    g_param_spec_object ("workbench", NULL, NULL,
                         IDE_TYPE_WORKBENCH,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  ide_action_mixin_init (&klass->action_mixin, object_class);
  ide_action_mixin_install_action (&klass->action_mixin, "close", NULL, ide_workspace_action_close);
  ide_action_mixin_install_action (&klass->action_mixin, "show-help-overlay", NULL, ide_workspace_action_help_overlay);
  ide_action_mixin_install_action (&klass->action_mixin, "focus-last-page", NULL, ide_workspace_action_focus_last_page);
}

static void
ide_workspace_init (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  g_autofree gchar *app_id = NULL;

#ifdef DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif

  priv->id = g_dbus_generate_guid ();
  priv->mru_link.data = self;

  /* Add org-gnome-Builder style CSS identifier */
  app_id = g_strdelimit (g_strdup (ide_get_application_id ()), ".", '-');
  gtk_widget_add_css_class (GTK_WIDGET (self), app_id);
  gtk_widget_add_css_class (GTK_WIDGET (self), "workspace");

  /* Setup container for children widgetry */
  priv->toolbar_view = ADW_TOOLBAR_VIEW (adw_toolbar_view_new ());
  adw_toolbar_view_set_top_bar_style (priv->toolbar_view, ADW_TOOLBAR_RAISED);
  adw_toolbar_view_set_bottom_bar_style (priv->toolbar_view, ADW_TOOLBAR_RAISED);

  priv->content_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  adw_toolbar_view_set_content (priv->toolbar_view, GTK_WIDGET (priv->content_box));

  adw_application_window_set_content (ADW_APPLICATION_WINDOW (self),
                                      GTK_WIDGET (priv->toolbar_view));

  /* Track focus change to propagate to addins */
  g_signal_connect (self,
                    "notify::focus-widget",
                    G_CALLBACK (ide_workspace_notify_focus_widget),
                    NULL);
}

GList *
_ide_workspace_get_mru_link (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_assert (IDE_IS_WORKSPACE (self));

  return &priv->mru_link;
}

/**
 * ide_workspace_get_context:
 *
 * Gets the #IdeContext for the #IdeWorkspace, which is set when the
 * workspace joins an #IdeWorkbench.
 *
 * Returns: (transfer none) (nullable): an #IdeContext or %NULL
 */
IdeContext *
ide_workspace_get_context (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  return priv->context;
}

void
_ide_workspace_set_context (IdeWorkspace *self,
                            IdeContext   *context)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (priv->context == NULL);

  if (g_set_object (&priv->context, context))
    {
      if (IDE_WORKSPACE_GET_CLASS (self)->context_set)
        IDE_WORKSPACE_GET_CLASS (self)->context_set (self, context);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
    }
}

/**
 * ide_workspace_get_cancellable:
 * @self: a #IdeWorkspace
 *
 * Gets a cancellable for a window. This is useful when you want operations
 * to be cancelled if a window is closed.
 *
 * Returns: (transfer none): a #GCancellable
 */
GCancellable *
ide_workspace_get_cancellable (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  if (priv->cancellable == NULL)
    priv->cancellable = g_cancellable_new ();

  return priv->cancellable;
}

/**
 * ide_workspace_foreach_page:
 * @self: a #IdeWorkspace
 * @callback: (scope call): a callback to execute for each view
 * @user_data: closure data for @callback
 *
 * Calls @callback for each #IdePage found within the workspace.
 */
void
ide_workspace_foreach_page (IdeWorkspace    *self,
                            IdePageCallback  callback,
                            gpointer         user_data)
{
  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (callback != NULL);

  if (IDE_WORKSPACE_GET_CLASS (self)->foreach_page)
    IDE_WORKSPACE_GET_CLASS (self)->foreach_page (self, callback, user_data);
}

/**
 * ide_workspace_get_header_bar:
 * @self: a #IdeWorkspace
 *
 * Gets the headerbar for the workspace, if it is an #IdeHeaderBar.
 * Also works around Gtk giving back a GtkStack for the header bar.
 *
 * Returns: (nullable) (transfer none): an #IdeHeaderBar or %NULL
 */
IdeHeaderBar *
ide_workspace_get_header_bar (IdeWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  return IDE_WORKSPACE_GET_CLASS (self)->get_header_bar (self);
}

/**
 * ide_workspace_get_most_recent_page:
 * @self: a #IdeWorkspace
 *
 * Gets the most recently focused #IdePage.
 *
 * Returns: (transfer none) (nullable): an #IdePage or %NULL
 */
IdePage *
ide_workspace_get_most_recent_page (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  if (priv->page_mru.head != NULL)
    return IDE_PAGE (priv->page_mru.head->data);

  return NULL;
}

/**
 * ide_workspace_get_most_recent_frame:
 * @self: a #IdeWorkspace
 *
 * Gets the most recently selected frame.
 *
 * Returns: (transfer none) (nullable): an #IdeFrame or %NULL
 */
IdeFrame *
ide_workspace_get_most_recent_frame (IdeWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  return IDE_WORKSPACE_GET_CLASS (self)->get_most_recent_frame (self);
}

void
_ide_workspace_add_page_mru (IdeWorkspace *self,
                             GList        *mru_link)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (mru_link != NULL);
  g_return_if_fail (mru_link->prev == NULL);
  g_return_if_fail (mru_link->next == NULL);
  g_return_if_fail (IDE_IS_PAGE (mru_link->data));

  g_debug ("Adding %s to page MRU",
           G_OBJECT_TYPE_NAME (mru_link->data));

  g_queue_push_head_link (&priv->page_mru, mru_link);
}

void
_ide_workspace_remove_page_mru (IdeWorkspace *self,
                                GList        *mru_link)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  IdePage *mru_page;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (mru_link != NULL);
  g_return_if_fail (IDE_IS_PAGE (mru_link->data));
  g_return_if_fail (g_queue_link_index (&priv->page_mru, mru_link) != -1);

  mru_page = mru_link->data;

  g_debug ("Removing %s from page MRU",
           G_OBJECT_TYPE_NAME (mru_page));

  g_queue_unlink (&priv->page_mru, mru_link);

  if ((gpointer)mru_page == priv->current_page_ptr)
    {
      g_clear_weak_pointer (&priv->current_page_ptr);
      ide_extension_set_adapter_foreach (priv->addins,
                                         ide_workspace_addin_page_changed_cb,
                                         NULL);
    }

  IDE_EXIT;
}

void
_ide_workspace_move_front_page_mru (IdeWorkspace *self,
                                    GList        *mru_link)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (mru_link != NULL);
  g_return_if_fail (IDE_IS_PAGE (mru_link->data));

  if (mru_link == priv->page_mru.head)
    return;

  /* Ignore unless the page is already in the MRU */
  if (g_queue_link_index (&priv->page_mru, mru_link) == -1)
    return;

  g_debug ("Moving %s to front of page MRU",
           G_OBJECT_TYPE_NAME (mru_link->data));

  g_queue_unlink (&priv->page_mru, mru_link);
  g_queue_push_head_link (&priv->page_mru, mru_link);
}

/**
 * ide_workspace_addin_find_by_module_name:
 * @workspace: an #IdeWorkspace
 * @module_name: the name of the addin module
 *
 * Finds the addin (if any) matching the plugin's @module_name.
 *
 * Returns: (transfer none) (nullable): an #IdeWorkspaceAddin or %NULL
 */
IdeWorkspaceAddin *
ide_workspace_addin_find_by_module_name (IdeWorkspace *workspace,
                                         const gchar  *module_name)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (workspace);
  PeasPluginInfo *plugin_info;
  GObject *ret = NULL;
  PeasEngine *engine;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_WORKSPACE (workspace), NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  if (priv->addins == NULL)
    return NULL;

  engine = peas_engine_get_default ();

  if ((plugin_info = peas_engine_get_plugin_info (engine, module_name)))
    ret = ide_extension_set_adapter_get_extension (priv->addins, plugin_info);

  return IDE_WORKSPACE_ADDIN (ret);
}

/**
 * ide_workspace_add_page:
 * @self: a #IdeWorkspace
 * @page: an #IdePage
 * @position: the position for the page
 *
 * Adds @page to @workspace.
 *
 * In future versions, @position may be updated to reflect the
 * position in which @page was added.
 */
void
ide_workspace_add_page (IdeWorkspace  *self,
                        IdePage       *page,
                        PanelPosition *position)
{
  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_PAGE (page));
  g_return_if_fail (position != NULL);

  if (IDE_WORKSPACE_GET_CLASS (self)->add_page)
    IDE_WORKSPACE_GET_CLASS (self)->add_page (self, page, position);
  else
    g_critical ("%s does not support adding pages",
                G_OBJECT_TYPE_NAME (self));
}

/**
 * ide_workspace_add_pane:
 * @self: a #IdeWorkspace
 * @pane: an #IdePane
 * @position: the position for the pane
 *
 * Adds @pane to @workspace.
 *
 * In future versions, @position may be updated to reflect the
 * position in which @pane was added.
 */
void
ide_workspace_add_pane (IdeWorkspace  *self,
                        IdePane       *pane,
                        PanelPosition *position)
{
  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_PANE (pane));
  g_return_if_fail (position != NULL);

  if (IDE_WORKSPACE_GET_CLASS (self)->add_pane)
    IDE_WORKSPACE_GET_CLASS (self)->add_pane (self, pane, position);
  else
    g_critical ("%s does not support adding panels",
                G_OBJECT_TYPE_NAME (self));
}

static void
ide_workspace_add_child (GtkBuildable *buildable,
                         GtkBuilder   *builder,
                         GObject      *object,
                         const char   *type)
{
  IdeWorkspace *self = (IdeWorkspace *)buildable;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (object));

  if (GTK_IS_WIDGET (object))
    {
      if (g_strcmp0 (type, "titlebar") == 0)
        {
          adw_toolbar_view_add_top_bar (priv->toolbar_view, GTK_WIDGET (object));
        }
      else
        {
          gtk_box_append (priv->content_box, GTK_WIDGET (object));
        }
    }
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = ide_workspace_add_child;
}

static void
add_to_frame_with_depth (PanelFrame  *frame,
                         PanelWidget *widget,
                         guint        depth,
                         gboolean     depth_set)
{
  PanelWidget *previous_page;
  guint n_pages;

  g_assert (PANEL_IS_FRAME (frame));
  g_assert (PANEL_IS_WIDGET (widget));

  previous_page = panel_frame_get_visible_child (frame);

  if (!depth_set || depth > G_MAXINT)
    depth = G_MAXINT;

  SET_PRIORITY (widget, depth);

  n_pages = panel_frame_get_n_pages (frame);

  for (guint i = 0; i < n_pages; i++)
    {
      PanelWidget *child = panel_frame_get_page (frame, i);

      if ((int)depth < GET_PRIORITY (child))
        {
          panel_frame_add_before (frame, widget, child);
          goto reset_page;
        }
    }

  panel_frame_add (frame, widget);

reset_page:
  if (previous_page != NULL)
    panel_frame_set_visible_child (frame, previous_page);
}

static gboolean
find_open_frame (IdeGrid *grid,
                 guint   *column,
                 guint   *row)
{
  guint n_columns;

  g_assert (IDE_IS_GRID (grid));
  g_assert (column != NULL);
  g_assert (row != NULL);

  n_columns = panel_grid_get_n_columns (PANEL_GRID (grid));

  for (guint c = 0; c < n_columns; c++)
    {
      PanelGridColumn *grid_column = panel_grid_get_column (PANEL_GRID (grid), c);
      guint n_rows = panel_grid_column_get_n_rows (grid_column);

      for (guint r = 0; r < n_rows; r++)
        {
          PanelFrame *frame = panel_grid_column_get_row (grid_column, r);

          if (panel_frame_get_empty (frame))
            {
              *column = c;
              *row = r;
              return TRUE;
            }
        }
    }

  return FALSE;
}

static void
find_most_recent_frame (IdeWorkspace *workspace,
                        IdeGrid      *grid,
                        guint        *column,
                        guint        *row)
{
  GtkWidget *grid_column;
  IdeFrame *frame;
  guint n_columns;

  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (IDE_IS_GRID (grid));
  g_assert (column != NULL);
  g_assert (row != NULL);

  *column = 0;
  *row = 0;

  if (!(frame = ide_workspace_get_most_recent_frame (workspace)) ||
      !(grid_column = gtk_widget_get_ancestor (GTK_WIDGET (frame), PANEL_TYPE_GRID_COLUMN)))
    return;

  n_columns = panel_grid_get_n_columns (PANEL_GRID (grid));

  for (guint c = 0; c < n_columns; c++)
    {
      if (grid_column == (GtkWidget *)panel_grid_get_column (PANEL_GRID (grid), c))
        {
          guint n_rows = panel_grid_column_get_n_rows (PANEL_GRID_COLUMN (grid_column));

          for (guint r = 0; r < n_rows; r++)
            {
              if ((PanelFrame *)frame == panel_grid_column_get_row (PANEL_GRID_COLUMN (grid_column), r))
                {
                  *column = c;
                  *row = r;
                  return;
                }
            }
        }
    }
}

static gboolean
dummy_cb (gpointer data)
{
  return G_SOURCE_REMOVE;
}

void
_ide_workspace_add_widget (IdeWorkspace     *self,
                           PanelWidget      *widget,
                           PanelPosition    *position,
                           IdeWorkspaceDock *dock)
{
  PanelFrame *frame;
  gboolean depth_set;
  guint depth;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (PANEL_IS_WIDGET (widget));
  g_return_if_fail (position != NULL);
  g_return_if_fail (dock != NULL);

  if (!(frame = _ide_workspace_find_frame (self, position, dock)))
    {
      /* Extreme failure case, try to be nice and wait until
       * end of the main loop to destroy
       */
      g_idle_add_full (G_PRIORITY_LOW,
                       dummy_cb,
                       g_object_ref_sink (widget),
                       g_object_unref);
      IDE_EXIT;
    }

  depth_set = ide_panel_position_get_depth (position, &depth);
  add_to_frame_with_depth (frame, widget, depth, depth_set);

  IDE_EXIT;
}

PanelFrame *
_ide_workspace_find_frame (IdeWorkspace     *self,
                           PanelPosition    *position,
                           IdeWorkspaceDock *dock)
{
  PanelArea area;
  PanelPaned *paned = NULL;
  PanelFrame *ret;
  GtkWidget *parent;
  guint column = 0;
  guint row = 0;
  guint nth = 0;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);
  g_return_val_if_fail (position != NULL, NULL);
  g_return_val_if_fail (dock != NULL, NULL);

  if (!ide_panel_position_get_area (position, &area))
    area = PANEL_AREA_CENTER;

  if (area == PANEL_AREA_CENTER)
    {
      gboolean has_column = ide_panel_position_get_column (position, &column);
      gboolean has_row = ide_panel_position_get_row (position, &row);

      /* If we are adding a page, and no row or column is set, then the next
       * best thing to do is to try to find an open frame. If we can't do that
       * then we'll try to find the most recent frame.
       */
      if (!has_column && !has_row)
        {
          if (!find_open_frame (dock->grid, &column, &row))
            find_most_recent_frame (self, dock->grid, &column, &row);
        }

      ret = panel_grid_column_get_row (panel_grid_get_column (PANEL_GRID (dock->grid), column), row);

      IDE_RETURN (ret);
    }

  switch (area)
    {
    case PANEL_AREA_START:
      paned = dock->start_area;
      ide_panel_position_get_row (position, &nth);
      break;

    case PANEL_AREA_END:
      paned = dock->end_area;
      ide_panel_position_get_row (position, &nth);
      break;

    case PANEL_AREA_BOTTOM:
      paned = dock->bottom_area;
      ide_panel_position_get_column (position, &nth);
      break;

    case PANEL_AREA_TOP:
      g_warning ("Top panel is not supported");
      return NULL;

    case PANEL_AREA_CENTER:
    default:
      return NULL;
    }

  while (!(parent = panel_paned_get_nth_child (paned, nth)))
    {
      parent = panel_frame_new ();

      if (area == PANEL_AREA_START ||
          area == PANEL_AREA_END)
        gtk_orientable_set_orientation (GTK_ORIENTABLE (parent), GTK_ORIENTATION_VERTICAL);
      else
        gtk_orientable_set_orientation (GTK_ORIENTABLE (parent), GTK_ORIENTATION_HORIZONTAL);

      panel_paned_append (paned, parent);
    }

  IDE_RETURN (PANEL_FRAME (parent));
}

/**
 * ide_workspace_get_frame_at_position:
 * @self: an #IdeWorkspace
 * @position: an #PanelPosition
 *
 * Attempts to locate the #PanelFrame at a given position.
 *
 * Returns: (transfer none) (nullable): a #PaneFrame or %NULL
 */
PanelFrame *
ide_workspace_get_frame_at_position (IdeWorkspace  *self,
                                     PanelPosition *position)
{
  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);
  g_return_val_if_fail (position != NULL, NULL);

  if (IDE_WORKSPACE_GET_CLASS (self)->get_frame_at_position)
    return IDE_WORKSPACE_GET_CLASS (self)->get_frame_at_position (self, position);

  return NULL;
}

gboolean
_ide_workspace_can_search (IdeWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_WORKSPACE (self), FALSE);

  return IDE_WORKSPACE_GET_CLASS (self)->can_search (self);
}

void
_ide_workspace_begin_global_search (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));

  if (priv->search_popover == NULL)
    {
      IdeWorkbench *workbench = ide_workspace_get_workbench (self);
      IdeSearchEngine *search_engine = ide_workbench_get_search_engine (workbench);

      priv->search_popover = g_object_ref_sink (IDE_SEARCH_POPOVER (ide_search_popover_new (search_engine)));

      /* Popovers don't appear (as of GTK 4.7) to capture/bubble from the GtkRoot
       * when running controllers. So we need to manually attach them for the popovers
       * that are important enough to care about.
       */
      ide_workspace_attach_shortcuts (self, GTK_WIDGET (priv->search_popover));
    }

  adw_dialog_present (ADW_DIALOG (priv->search_popover), GTK_WIDGET (self));
}

void
ide_workspace_add_overlay (IdeWorkspace *self,
                           GtkWidget    *overlay)
{
  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (GTK_IS_WIDGET (overlay));
  g_return_if_fail (gtk_widget_get_parent (overlay) == NULL);

  if (IDE_WORKSPACE_GET_CLASS (self)->add_overlay == NULL)
    g_critical ("Attempt to add overlay of type %s to workspace of type %s which does not support overlays",
                G_OBJECT_TYPE_NAME (overlay), G_OBJECT_TYPE_NAME (self));
  else
    IDE_WORKSPACE_GET_CLASS (self)->add_overlay (self, overlay);
}

void
ide_workspace_remove_overlay (IdeWorkspace *self,
                              GtkWidget    *overlay)
{
  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (GTK_IS_WIDGET (overlay));

  if (IDE_WORKSPACE_GET_CLASS (self)->remove_overlay == NULL)
    g_critical ("Attempt to remove overlay of type %s to workspace of type %s which does not support overlays",
                G_OBJECT_TYPE_NAME (overlay), G_OBJECT_TYPE_NAME (self));
  else
    IDE_WORKSPACE_GET_CLASS (self)->remove_overlay (self, overlay);
}

static gboolean
shortcut_phase_filter (gpointer item,
                       gpointer user_data)
{
  return ide_shortcut_is_phase (item, GPOINTER_TO_UINT (user_data));
}

void
_ide_workspace_set_shortcut_model (IdeWorkspace *self,
                                   GListModel   *model)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  static GtkCustomFilter *bubble_filter;
  static GtkCustomFilter *capture_filter;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (G_IS_LIST_MODEL (model));

  g_set_object (&priv->shortcuts, model);

  if (bubble_filter == NULL)
    bubble_filter = gtk_custom_filter_new (shortcut_phase_filter, GINT_TO_POINTER (GTK_PHASE_BUBBLE), NULL);

  if (capture_filter == NULL)
    capture_filter = gtk_custom_filter_new (shortcut_phase_filter, GINT_TO_POINTER (GTK_PHASE_CAPTURE), NULL);

  priv->shortcut_model_capture = gtk_filter_list_model_new (g_object_ref (model),
                                                            g_object_ref (GTK_FILTER (capture_filter)));
  priv->shortcut_model_bubble = gtk_filter_list_model_new (g_object_ref (model),
                                                           g_object_ref (GTK_FILTER (bubble_filter)));

  ide_workspace_attach_shortcuts (self, GTK_WIDGET (self));
}

void
ide_workspace_add_grid_column (IdeWorkspace *self,
                               guint         position)
{
  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_WORKSPACE_GET_CLASS (self)->add_grid_column);

  IDE_WORKSPACE_GET_CLASS (self)->add_grid_column (self, position);
}

/**
 * ide_workspace_class_install_action:
 * @workspace_class: an `IdeWorkspaceClass`
 * @action_name: a prefixed action name, such as "clipboard.paste"
 * @parameter_type: (nullable): the parameter type
 * @activate: (scope notified): callback to use when the action is activated
 *
 * This should be called at class initialization time to specify
 * actions to be added for all instances of this class.
 *
 * Actions installed by this function are stateless. The only state
 * they have is whether they are enabled or not.
 */
void
ide_workspace_class_install_action (IdeWorkspaceClass     *workspace_class,
                                    const char            *action_name,
                                    const char            *parameter_type,
                                    IdeActionActivateFunc  activate)
{
  workspace_class->action_mixin.object_class = G_OBJECT_CLASS (workspace_class);

  ide_action_mixin_install_action (&workspace_class->action_mixin, action_name, parameter_type, activate);
}

/**
 * ide_workspace_class_install_property_action:
 * @workspace_class: an `IdeWorkspaceClass`
 * @action_name: name of the action
 * @property_name: name of the property in instances of @widget_class
 *   or any parent class.
 *
 * Installs an action called @action_name on @widget_class and
 * binds its state to the value of the @property_name property.
 *
 * This function will perform a few santity checks on the property selected
 * via @property_name. Namely, the property must exist, must be readable,
 * writable and must not be construct-only. There are also restrictions
 * on the type of the given property, it must be boolean, int, unsigned int,
 * double or string. If any of these conditions are not met, a critical
 * warning will be printed and no action will be added.
 *
 * The state type of the action matches the property type.
 *
 * If the property is boolean, the action will have no parameter and
 * toggle the property value. Otherwise, the action will have a parameter
 * of the same type as the property.
 */
void
ide_workspace_class_install_property_action (IdeWorkspaceClass *workspace_class,
                                             const char        *action_name,
                                             const char        *property_name)
{
  workspace_class->action_mixin.object_class = G_OBJECT_CLASS (workspace_class);

  ide_action_mixin_install_property_action (&workspace_class->action_mixin, action_name, property_name);
}

void
ide_workspace_action_set_enabled (IdeWorkspace *self,
                                  const char   *action_name,
                                  gboolean      enabled)
{
  ide_action_mixin_set_enabled (self, action_name, enabled);
}

static void
_ide_workspace_agree_to_close_run_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  PanelChangesDialog *dialog = (PanelChangesDialog *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (PANEL_IS_CHANGES_DIALOG (dialog));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!panel_changes_dialog_run_finish (dialog, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
_ide_workspace_agree_to_close_page_cb (IdePage  *page,
                                       gpointer  user_data)
{
  PanelChangesDialog *dialog = user_data;
  PanelSaveDelegate *delegate;

  g_assert (IDE_IS_PAGE (page));
  g_assert (PANEL_IS_CHANGES_DIALOG (dialog));

  if ((delegate = panel_widget_get_save_delegate (PANEL_WIDGET (page))) &&
      panel_widget_get_modified (PANEL_WIDGET (page)))
    panel_changes_dialog_add_delegate (dialog, delegate);
}

void
_ide_workspace_agree_to_close_async (IdeWorkspace        *self,
                                     IdeGrid             *grid,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  PanelChangesDialog *dialog;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_GRID (grid));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, _ide_workspace_agree_to_close_async);

  dialog = PANEL_CHANGES_DIALOG (panel_changes_dialog_new ());

  ide_grid_foreach_page (grid, _ide_workspace_agree_to_close_page_cb, dialog);

  panel_changes_dialog_run_async (dialog,
                                  GTK_WIDGET (self),
                                  cancellable,
                                  _ide_workspace_agree_to_close_run_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
_ide_workspace_agree_to_close_finish (IdeWorkspace  *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

void
ide_workspace_inhibit_logout (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));

  priv->inhibit_logout_count++;

  if (priv->inhibit_logout_count == 1)
    {
      priv->inhibit_logout_cookie =
        gtk_application_inhibit (GTK_APPLICATION (IDE_APPLICATION_DEFAULT),
                                 GTK_WINDOW (self),
                                 GTK_APPLICATION_INHIBIT_LOGOUT,
                                 _("There are unsaved documents"));
    }
}

void
ide_workspace_uninhibit_logout (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));

  if (priv->inhibit_logout_count == 1)
    {
      gtk_application_uninhibit (GTK_APPLICATION (IDE_APPLICATION_DEFAULT),
                                 priv->inhibit_logout_cookie);
      priv->inhibit_logout_cookie = 0;
    }

  priv->inhibit_logout_count--;
}

void
ide_workspace_set_id (IdeWorkspace *self,
                      const char   *id)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));

  if (g_set_str (&priv->id, id))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
}

const char *
ide_workspace_get_id (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  return priv->id;
}

void
ide_workspace_set_toolbar_style (IdeWorkspace    *self,
                                 AdwToolbarStyle  style)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));

  adw_toolbar_view_set_top_bar_style (priv->toolbar_view, style);
  adw_toolbar_view_set_bottom_bar_style (priv->toolbar_view, style);
}

AdwToolbarStyle
ide_workspace_get_toolbar_style (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), ADW_TOOLBAR_FLAT);

  return adw_toolbar_view_get_top_bar_style (priv->toolbar_view);
}

void
_ide_workspace_class_bind_template_dock (GtkWidgetClass *widget_class,
                                         goffset         struct_offset)
{
  g_return_if_fail (IDE_IS_WORKSPACE_CLASS (widget_class));
  g_return_if_fail (struct_offset > 0);

  /* TODO: We should just add an IdeDock class w/ the widgetry all defined. */

#define BIND_CHILD(c, name) \
  gtk_widget_class_bind_template_child_full (c, #name, FALSE, \
                                             struct_offset + G_STRUCT_OFFSET (IdeWorkspaceDock, name))
  BIND_CHILD (widget_class, dock);
  BIND_CHILD (widget_class, grid);
  BIND_CHILD (widget_class, start_area);
  BIND_CHILD (widget_class, bottom_area);
  BIND_CHILD (widget_class, end_area);
#undef BIND_CHILD
}

IdeExtensionSetAdapter *
_ide_workspace_get_addins (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  return priv->addins;
}

gboolean
_ide_workspace_adopt_widget (IdeWorkspace *workspace,
                             PanelWidget  *widget,
                             PanelDock    *dock)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (PANEL_IS_WIDGET (widget));
  g_assert (PANEL_IS_DOCK (dock));

  if (ide_widget_get_context (GTK_WIDGET (workspace)) == ide_widget_get_context (GTK_WIDGET (widget)))
    IDE_RETURN (GDK_EVENT_PROPAGATE);

  IDE_RETURN (GDK_EVENT_STOP);
}

/**
 * ide_workspace_get_statusbar:
 * @self: a #IdeWorkspace
 *
 * Returns: (transfer none) (nullable): a #PanelStatusbar or %NULL
 */
PanelStatusbar *
ide_workspace_get_statusbar (IdeWorkspace *self)
{
  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  if (!IDE_WORKSPACE_GET_CLASS (self)->get_statusbar)
    return NULL;

  return IDE_WORKSPACE_GET_CLASS (self)->get_statusbar (self);
}
