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

#include <libide-plugins.h>

#include "ide-gui-global.h"
#include "ide-workspace-addin.h"
#include "ide-workspace-private.h"
#include "ide-workbench-private.h"

#define MUX_ACTIONS_KEY "IDE_WORKSPACE_MUX_ACTIONS"

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

  /* A MRU that is updated as pages are focused. It allows us to move through
   * the pages in the order they've been most-recently focused.
   */
  GQueue page_mru;

  /* Queued source to save window size/etc */
  guint queued_window_save;

  /* Vertical box for children */
  GtkBox *box;

  /* Raw pointer to the last IdePage that was focused. This is never
   * dereferenced and only used to compare to determine if we've changed focus
   * into a new IdePage that must be propagated to the addins.
   */
  gpointer current_page_ptr;
} IdeWorkspacePrivate;

typedef struct
{
  IdePageCallback callback;
  gpointer        user_data;
} ForeachPage;

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeWorkspace, ide_workspace, ADW_TYPE_APPLICATION_WINDOW,
                                  G_ADD_PRIVATE (IdeWorkspace)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GParamSpec *properties [N_PROPS];
static GSettings *settings;

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
                              PeasExtension          *exten,
                              gpointer                user_data)
{
  IdeWorkspaceAddin *addin = (IdeWorkspaceAddin *)exten;
  IdeWorkspace *self = user_data;
  IdePage *page;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (IDE_IS_WORKSPACE (self));

  g_debug ("Loading workspace addin from module %s",
           peas_plugin_info_get_module_name (plugin_info));

  ide_workspace_addin_load (addin, self);

  if ((page = ide_workspace_get_focus_page (self)))
    ide_workspace_addin_page_changed (addin, page);
}

static void
ide_workspace_addin_removed_cb (IdeExtensionSetAdapter *set,
                                PeasPluginInfo         *plugin_info,
                                PeasExtension          *exten,
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

  ide_workspace_addin_page_changed (addin, NULL);
  ide_workspace_addin_unload (addin, self);
}

static void
ide_workspace_addin_page_changed_cb (IdeExtensionSetAdapter *set,
                                     PeasPluginInfo         *plugin_info,
                                     PeasExtension          *exten,
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

  if ((gpointer)focus != priv->current_page_ptr)
    {
      priv->current_page_ptr = focus;

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

static void
ide_workspace_restore_size (IdeWorkspace *workspace,
                            int           width,
                            int           height)
{
  g_assert (IDE_IS_WORKSPACE (workspace));

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

  if (maximized)
    gtk_window_maximize (GTK_WINDOW (self));
}

static void
ide_workspace_dispose (GObject *object)
{
  IdeWorkspace *self = (IdeWorkspace *)object;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkWindowGroup *group;

  g_assert (IDE_IS_WORKSPACE (self));

  /* Unload addins immediately */
  ide_clear_and_destroy_object (&priv->addins);

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

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_workspace_get_context (self));
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

  object_class->dispose = ide_workspace_dispose;
  object_class->finalize = ide_workspace_finalize;
  object_class->get_property = ide_workspace_get_property;

  widget_class->realize = ide_workspace_realize;
  widget_class->size_allocate = ide_workspace_size_allocate;

  window_class->close_request = ide_workspace_close_request;

  klass->foreach_page = ide_workspace_real_foreach_page;
  klass->context_set = ide_workspace_real_context_set;
  klass->agree_to_close_async = ide_workspace_agree_to_close_async;
  klass->agree_to_close_finish = ide_workspace_agree_to_close_finish;
  klass->restore_size = ide_workspace_restore_size;
  klass->save_size = ide_workspace_save_size;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_comma, GDK_CONTROL_MASK, "app.preferences", NULL);
}

static void
ide_workspace_init (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  g_autofree gchar *app_id = NULL;

#ifdef DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif

  priv->mru_link.data = self;

  /* Add org-gnome-Builder style CSS identifier */
  app_id = g_strdelimit (g_strdup (ide_get_application_id ()), ".", '-');
  gtk_widget_add_css_class (GTK_WIDGET (self), app_id);
  gtk_widget_add_css_class (GTK_WIDGET (self), "workspace");

  /* Setup container for children widgetry */
  priv->box = g_object_new (GTK_TYPE_BOX,
                            "orientation", GTK_ORIENTATION_VERTICAL,
                            NULL);
  adw_application_window_set_content (ADW_APPLICATION_WINDOW (self),
                                      GTK_WIDGET (priv->box));

  /* Track focus change to propagate to addins */
  g_signal_connect (self,
                    "notify::focus-widget",
                    G_CALLBACK (ide_workspace_notify_focus_widget),
                    NULL);

  /* Initialize GActions for workspace */
  _ide_workspace_init_actions (self);
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
  GtkWidget *ret = NULL;

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  ret = gtk_window_get_titlebar (GTK_WINDOW (self));

  if (IDE_IS_HEADER_BAR (ret))
    return IDE_HEADER_BAR (ret);

  return NULL;
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

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (mru_link != NULL);
  g_return_if_fail (IDE_IS_PAGE (mru_link->data));

  g_debug ("Removing %s from page MRU",
           G_OBJECT_TYPE_NAME (mru_link->data));

  g_queue_unlink (&priv->page_mru, mru_link);
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
  PeasExtension *ret = NULL;
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

void
ide_workspace_add_page (IdeWorkspace     *self,
                        IdePage          *page,
                        IdePanelPosition *position)
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

void
ide_workspace_add_pane (IdeWorkspace     *self,
                        IdePane          *pane,
                        IdePanelPosition *position)
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
        gtk_box_prepend (priv->box, GTK_WIDGET (object));
      else
        gtk_box_append (priv->box, GTK_WIDGET (object));
    }
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = ide_workspace_add_child;
}
