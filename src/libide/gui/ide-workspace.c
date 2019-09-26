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
#include "ide-workspace.h"
#include "ide-workspace-addin.h"

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

  /* We use an overlay as our top-most child so that plugins can potentially
   * render any widget a layer above the UI.
   */
  GtkOverlay *overlay;

  /* All workspaces are comprised of a series of "surfaces". However there may
   * only ever be a single surface in a workspace (such as the editor workspace
   * which is dedicated for editing).
   */
  GtkStack *surfaces;

  /* The event box ensures that we can have events that will be used by the
   * fullscreen overlay so that it gets delivery of crossing events.
   */
  GtkEventBox *event_box;

  /* A MRU that is updated as pages are focused. It allows us to move through
   * the pages in the order they've been most-recently focused.
   */
  GQueue page_mru;
} IdeWorkspacePrivate;

typedef struct
{
  GtkCallback callback;
  gpointer    user_data;
} ForeachPage;

enum {
  SURFACE_SET,
  N_SIGNALS
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_VISIBLE_SURFACE,
  N_PROPS
};

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeWorkspace, ide_workspace, DZL_TYPE_APPLICATION_WINDOW,
                                  G_ADD_PRIVATE (IdeWorkspace)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

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

  ide_workspace_addin_surface_set (addin, NULL);
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
ide_workspace_addin_surface_set_cb (IdeExtensionSetAdapter *set,
                                    PeasPluginInfo         *plugin_info,
                                    PeasExtension          *exten,
                                    gpointer                user_data)
{
  IdeWorkspaceAddin *addin = (IdeWorkspaceAddin *)exten;
  IdeSurface *surface = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (!surface || IDE_IS_SURFACE (surface));

  ide_workspace_addin_surface_set (addin, surface);
}

static void
ide_workspace_real_surface_set (IdeWorkspace *self,
                                IdeSurface   *surface)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (!surface || IDE_IS_SURFACE (surface));

  if (priv->addins != NULL)
    ide_extension_set_adapter_foreach (priv->addins,
                                       ide_workspace_addin_surface_set_cb,
                                       surface);
}

/**
 * ide_workspace_foreach_surface:
 * @self: a #IdeWorkspace
 * @callback: (scope call): a #GtkCallback to execute for every surface
 * @user_data: user data for @callback
 *
 * Calls callback for every #IdeSurface based #GtkWidget that is registered
 * in the workspace.
 *
 * Since: 3.32
 */
void
ide_workspace_foreach_surface (IdeWorkspace *self,
                               GtkCallback   callback,
                               gpointer      user_data)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (callback != NULL);

  gtk_container_foreach (GTK_CONTAINER (priv->surfaces), callback, user_data);
}

static void
ide_workspace_agree_to_shutdown_cb (GtkWidget *widget,
                                    gpointer   user_data)
{
  gboolean *blocked = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SURFACE (widget));
  g_assert (blocked != NULL);

  *blocked |= !ide_surface_agree_to_shutdown (IDE_SURFACE (widget));
}

static void
ide_workspace_addin_can_close_cb (IdeExtensionSetAdapter *adapter,
                                  PeasPluginInfo         *plugin_info,
                                  PeasExtension          *exten,
                                  gpointer                user_data)
{
  IdeWorkspaceAddin *addin = (IdeWorkspaceAddin *)exten;
  gboolean *blocked = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE_ADDIN (addin));
  g_assert (blocked != NULL);

  *blocked |= !ide_workspace_addin_can_close (addin);
}

static gboolean
ide_workspace_agree_to_shutdown (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  gboolean blocked = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKSPACE (self));

  ide_workspace_foreach_surface (self, ide_workspace_agree_to_shutdown_cb, &blocked);

  if (!blocked)
    ide_extension_set_adapter_foreach (priv->addins,
                                       ide_workspace_addin_can_close_cb,
                                       &blocked);

  return !blocked;
}

static gboolean
ide_workspace_delete_event (GtkWidget   *widget,
                            GdkEventAny *any)
{
  IdeWorkspace *self = (IdeWorkspace *)widget;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  IdeWorkbench *workbench;

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (any != NULL);

  /* TODO:
   *
   * If there are any active transfers, we want to ask the user if they
   * are sure they want to exit and risk losing them. We can allow them
   * to be completed in the background.
   *
   * Note that we only want to do this on the final workspace window.
   */

  if (!ide_workspace_agree_to_shutdown (self))
    return GDK_EVENT_STOP;

  g_cancellable_cancel (priv->cancellable);

  workbench = ide_widget_get_workbench (widget);

  if (ide_workbench_has_project (workbench) &&
      _ide_workbench_is_last_workspace (workbench, self))
    {
      gtk_widget_hide (GTK_WIDGET (self));
      ide_workbench_unload_async (workbench, NULL, NULL, NULL);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_workspace_notify_surface_cb (IdeWorkspace *self,
                                 GParamSpec   *pspec,
                                 GtkStack     *surfaces)
{
  GtkWidget *visible_child;
  IdeHeaderBar *header_bar;

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (GTK_IS_STACK (surfaces));

  visible_child = gtk_stack_get_visible_child (surfaces);
  if (!IDE_IS_SURFACE (visible_child))
    visible_child = NULL;

  if (visible_child != NULL)
    gtk_widget_grab_focus (visible_child);

  if ((header_bar = ide_workspace_get_header_bar (self)))
    {
      if (visible_child != NULL)
        dzl_gtk_widget_mux_action_groups (GTK_WIDGET (header_bar), visible_child, MUX_ACTIONS_KEY);
      else
        dzl_gtk_widget_mux_action_groups (GTK_WIDGET (header_bar), NULL, MUX_ACTIONS_KEY);
    }

  g_signal_emit (self, signals [SURFACE_SET], 0, visible_child);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VISIBLE_SURFACE]);
}

static void
ide_workspace_destroy (GtkWidget *widget)
{
  IdeWorkspace *self = (IdeWorkspace *)widget;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkWindowGroup *group;

  g_assert (IDE_IS_WORKSPACE (self));

  ide_clear_and_destroy_object (&priv->addins);

  group = gtk_window_get_group (GTK_WINDOW (self));
  if (IDE_IS_WORKBENCH (group))
    ide_workbench_remove_workspace (IDE_WORKBENCH (group), self);

  GTK_WIDGET_CLASS (ide_workspace_parent_class)->destroy (widget);
}

/**
 * ide_workspace_class_set_kind:
 * @klass: a #IdeWorkspaceClass
 *
 * Sets the shorthand name for the kind of workspace. This is used to limit
 * what #IdeWorkspaceAddin may load within the workspace.
 *
 * Since: 3.32
 */
void
ide_workspace_class_set_kind (IdeWorkspaceClass *klass,
                              const gchar       *kind)
{
  g_return_if_fail (IDE_IS_WORKSPACE_CLASS (klass));

  klass->kind = g_intern_string (kind);
}


static void
ide_workspace_foreach_page_cb (GtkWidget *widget,
                               gpointer   user_data)
{
  ForeachPage *state = user_data;

  if (IDE_IS_SURFACE (widget))
    ide_surface_foreach_page (IDE_SURFACE (widget), state->callback, state->user_data);
}

static void
ide_workspace_real_foreach_page (IdeWorkspace *self,
                                 GtkCallback   callback,
                                 gpointer      user_data)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  ForeachPage state = { callback, user_data };

  g_assert (IDE_IS_WORKSPACE (self));
  g_assert (callback != NULL);

  gtk_container_foreach (GTK_CONTAINER (priv->surfaces),
                         ide_workspace_foreach_page_cb,
                         &state);
}

static void
ide_workspace_set_surface_fullscreen_cb (GtkWidget *widget,
                                         gpointer   user_data)
{
  g_assert (GTK_IS_WIDGET (widget));

  if (IDE_IS_SURFACE (widget))
    _ide_surface_set_fullscreen (IDE_SURFACE (widget), !!user_data);
}

static void
ide_workspace_real_set_fullscreen (DzlApplicationWindow *window,
                                   gboolean              fullscreen)
{
  IdeWorkspace *self = (IdeWorkspace *)window;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkWidget *titlebar;

  g_assert (IDE_IS_WORKSPACE (self));

  DZL_APPLICATION_WINDOW_CLASS (ide_workspace_parent_class)->set_fullscreen (window, fullscreen);

  titlebar = dzl_application_window_get_titlebar (window);
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (titlebar), !fullscreen);

  gtk_container_foreach (GTK_CONTAINER (priv->surfaces),
                         ide_workspace_set_surface_fullscreen_cb,
                         GUINT_TO_POINTER (fullscreen));
}

static void
ide_workspace_grab_focus (GtkWidget *widget)
{
  IdeWorkspace *self = (IdeWorkspace *)widget;
  IdeSurface *surface;

  g_assert (IDE_IS_WORKSPACE (self));

  if ((surface = ide_workspace_get_visible_surface (self)))
    gtk_widget_grab_focus (GTK_WIDGET (surface));
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

    case PROP_VISIBLE_SURFACE:
      g_value_set_object (value, ide_workspace_get_visible_surface (self));
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
    case PROP_VISIBLE_SURFACE:
      ide_workspace_set_visible_surface (self, g_value_get_object (value));
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
  DzlApplicationWindowClass *window_class = DZL_APPLICATION_WINDOW_CLASS (klass);

  object_class->finalize = ide_workspace_finalize;
  object_class->get_property = ide_workspace_get_property;
  object_class->set_property = ide_workspace_set_property;

  widget_class->destroy = ide_workspace_destroy;
  widget_class->delete_event = ide_workspace_delete_event;
  widget_class->grab_focus = ide_workspace_grab_focus;

  window_class->set_fullscreen = ide_workspace_real_set_fullscreen;

  klass->foreach_page = ide_workspace_real_foreach_page;
  klass->context_set = ide_workspace_real_context_set;
  klass->surface_set = ide_workspace_real_surface_set;

  /**
   * IdeWorkspace:context:
   *
   * The "context" property is the #IdeContext for the workspace. This is set
   * when the workspace joins a workbench.
   *
   * Since: 3.32
   */
  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The IdeContext for the workspace, inherited from workbench",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeWorkspace:visible-surface:
   *
   * The "visible-surface" property contains the currently foremost surface
   * in the workspaces stack of surfaces. Usually, this is the editor surface,
   * but may be other surfaces such as build preferences, profiler, etc.
   *
   * Since: 3.32
   */
  properties [PROP_VISIBLE_SURFACE] =
    g_param_spec_object ("visible-surface",
                         "Visible Surface",
                         "The currently visible surface",
                         IDE_TYPE_SURFACE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeWorkspace::surface-set:
   * @self: an #IdeWorkspace
   * @surface: (nullable): an #IdeSurface
   *
   * The "surface-set" signal is emitted when the current surface changes
   * within the workspace.
   *
   * Since: 3.32
   */
  signals [SURFACE_SET] =
    g_signal_new ("surface-set",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeWorkspaceClass, surface_set),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, IDE_TYPE_SURFACE);
  g_signal_set_va_marshaller (signals [SURFACE_SET],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-workspace.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkspace, event_box);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkspace, overlay);
  gtk_widget_class_bind_template_child_private (widget_class, IdeWorkspace, surfaces);
}

static void
ide_workspace_init (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  g_autofree gchar *app_id = NULL;

  priv->mru_link.data = self;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (priv->surfaces,
                           "notify::visible-child",
                           G_CALLBACK (ide_workspace_notify_surface_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Add org-gnome-Builder style CSS identifier */
  app_id = g_strdelimit (g_strdup (ide_get_application_id ()), ".", '-');
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self), app_id);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self), "workspace");

  /* Add events for motion controller of fullscreen titlebar */
  gtk_widget_add_events (GTK_WIDGET (priv->event_box),
                         (GDK_POINTER_MOTION_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK));

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
 *
 * Since: 3.32
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
 *
 * Since: 3.32
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
 *
 * Since: 3.32
 */
void
ide_workspace_foreach_page (IdeWorkspace *self,
                            GtkCallback   callback,
                            gpointer      user_data)
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
 *
 * Since: 3.32
 */
IdeHeaderBar *
ide_workspace_get_header_bar (IdeWorkspace *self)
{
  GtkWidget *titlebar;

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  if ((titlebar = gtk_window_get_titlebar (GTK_WINDOW (self))))
    {
      if (GTK_IS_STACK (titlebar))
        titlebar = gtk_stack_get_visible_child (GTK_STACK (titlebar));

      if (IDE_IS_HEADER_BAR (titlebar))
        return IDE_HEADER_BAR (titlebar);
    }

  return NULL;
}

/**
 * ide_workspace_add_surface:
 * @self: a #IdeWorkspace
 *
 * Adds a new #IdeSurface to the workspace.
 *
 * Since: 3.32
 */
void
ide_workspace_add_surface (IdeWorkspace *self,
                           IdeSurface   *surface)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  g_autofree gchar *title = NULL;

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_SURFACE (surface));

  if (DZL_IS_DOCK_ITEM (surface))
    title = dzl_dock_item_get_title (DZL_DOCK_ITEM (surface));

  gtk_container_add_with_properties (GTK_CONTAINER (priv->surfaces), GTK_WIDGET (surface),
                                     "name", gtk_widget_get_name (GTK_WIDGET (surface)),
                                     "title", title,
                                     NULL);
}

/**
 * ide_workspace_set_visible_surface_name:
 * @self: a #IdeWorkspace
 * @visible_surface_name: the name of the #IdeSurface
 *
 * Sets the visible surface based on the name of the surface.  The name of the
 * surface comes from gtk_widget_get_name(), which should be set when creating
 * the surface using gtk_widget_set_name().
 *
 * Since: 3.32
 */
void
ide_workspace_set_visible_surface_name (IdeWorkspace *self,
                                        const gchar  *visible_surface_name)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (visible_surface_name != NULL);

  gtk_stack_set_visible_child_name (priv->surfaces, visible_surface_name);
}

/**
 * ide_workspace_get_visible_surface:
 * @self: a #IdeWorkspace
 *
 * Gets the currently visible #IdeSurface, or %NULL
 *
 * Returns: (transfer none) (nullable): an #IdeSurface or %NULL
 *
 * Since: 3.32
 */
IdeSurface *
ide_workspace_get_visible_surface (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkWidget *child;

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  child = gtk_stack_get_visible_child (priv->surfaces);
  if (!IDE_IS_SURFACE (child))
    child = NULL;

  return IDE_SURFACE (child);
}

/**
 * ide_workspace_set_visible_surface:
 * @self: a #IdeWorkspace
 * @surface: an #IdeSurface
 *
 * Sets the #IdeWorkspace:visible-surface property which is the currently
 * visible #IdeSurface in the workspace.
 *
 * Since: 3.32
 */
void
ide_workspace_set_visible_surface (IdeWorkspace *self,
                                   IdeSurface   *surface)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_if_fail (IDE_IS_WORKSPACE (self));
  g_return_if_fail (IDE_IS_SURFACE (surface));

  gtk_stack_set_visible_child (priv->surfaces, GTK_WIDGET (surface));
}

/**
 * ide_workspace_get_surface_by_name:
 * @self: a #IdeWorkspace
 * @name: the name of the surface
 *
 * Locates an #IdeSurface that has been added to the workspace by the name
 * that was registered for the widget using gtk_widget_set_name().
 *
 * Returns: (transfer none) (nullable): an #IdeSurface or %NULL
 *
 * Since: 3.32
 */
IdeSurface *
ide_workspace_get_surface_by_name (IdeWorkspace *self,
                                   const gchar  *name)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);
  GtkWidget *child;

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  child = gtk_stack_get_child_by_name (priv->surfaces, name);

  return IDE_IS_SURFACE (child) ? IDE_SURFACE (child) : NULL;
}

static GObject *
ide_workspace_get_internal_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  const gchar  *child_name)
{
  IdeWorkspace *self = (IdeWorkspace *)buildable;
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_assert (GTK_IS_BUILDABLE (buildable));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (child_name != NULL);

  if (ide_str_equal0 (child_name, "surfaces"))
    return G_OBJECT (priv->surfaces);

  return NULL;
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = ide_workspace_get_internal_child;
}

/**
 * ide_workspace_get_overlay:
 * @self: a #IdeWorkspace
 *
 * Gets a #GtkOverlay that contains all of the primary contents of the window
 * (everything except the headerbar). This can be used by plugins to draw
 * above the workspace contents.
 *
 * Returns: (transfer none): a #GtkOverlay
 *
 * Since: 3.32
 */
GtkOverlay *
ide_workspace_get_overlay (IdeWorkspace *self)
{
  IdeWorkspacePrivate *priv = ide_workspace_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_WORKSPACE (self), NULL);

  return priv->overlay;
}

/**
 * ide_workspace_get_most_recent_page:
 * @self: a #IdeWorkspace
 *
 * Gets the most recently focused #IdePage.
 *
 * Returns: (transfer none) (nullable): an #IdePage or %NULL
 *
 * Since: 3.32
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
