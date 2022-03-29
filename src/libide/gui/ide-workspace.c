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
#include "ide-gui-private.h"
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

  guint in_key_press : 1;
} IdeWorkspacePrivate;

typedef struct
{
  IdePageCallback callback;
  gpointer        user_data;
} ForeachPage;

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_VISIBLE_SURFACE,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (IdeWorkspace, ide_workspace, ADW_TYPE_APPLICATION_WINDOW)

static GParamSpec *properties [N_PROPS];

static void
ide_workspace_addin_added_cb (IdeExtensionSetAdapter *set,
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

  g_debug ("Loading workspace addin from module %s",
           peas_plugin_info_get_module_name (plugin_info));

  ide_workspace_addin_load (addin, self);
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

  ide_workspace_addin_unload (addin, self);
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
ide_workspace_dispose (GObject *object)
{
  IdeWorkspace *self = (IdeWorkspace *)object;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkWindowGroup *group;

  g_assert (IDE_IS_WORKSPACE (self));

  ide_clear_and_destroy_object (&priv->addins);

  group = gtk_window_get_group (GTK_WINDOW (self));
  if (IDE_IS_WORKBENCH (group))
    ide_workbench_remove_workspace (IDE_WORKBENCH (group), self);

  G_OBJECT_CLASS (ide_workspace_parent_class)->dispose (object);
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

static void
ide_workspace_finalize (GObject *object)
{
  IdeWorkspace *self = (IdeWorkspace *)object;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_clear_object (&priv->context);
  g_clear_object (&priv->cancellable);

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
ide_workspace_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workspace_class_init (IdeWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->dispose = ide_workspace_dispose;
  object_class->finalize = ide_workspace_finalize;
  object_class->get_property = ide_workspace_get_property;
  object_class->set_property = ide_workspace_set_property;

  window_class->close_request = ide_workspace_close_request;

  klass->foreach_page = ide_workspace_real_foreach_page;
  klass->context_set = ide_workspace_real_context_set;
  klass->agree_to_close_async = ide_workspace_agree_to_close_async;
  klass->agree_to_close_finish = ide_workspace_agree_to_close_finish;

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
}

static void
ide_workspace_init (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  g_autofree gchar *app_id = NULL;

  priv->mru_link.data = self;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Add org-gnome-Builder style CSS identifier */
  app_id = g_strdelimit (g_strdup (ide_get_application_id ()), ".", '-');
  gtk_widget_add_css_class (GTK_WIDGET (self), app_id);
  gtk_widget_add_css_class (GTK_WIDGET (self), "workspace");

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
