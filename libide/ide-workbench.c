/* ide-workbench.c
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

#define G_LOG_DOMAIN "ide-workbench"

#include "ide-debug.h"
#include "ide-genesis-perspective.h"
#include "ide-greeter-perspective.h"
#include "ide-gtk.h"
#include "ide-layout.h"
#include "ide-layout-pane.h"
#include "ide-layout-view.h"
#include "ide-layout-stack.h"
#include "ide-macros.h"
#include "ide-preferences-perspective.h"
#include "ide-window-settings.h"
#include "ide-workbench.h"
#include "ide-workbench-addin.h"
#include "ide-workbench-header-bar.h"
#include "ide-workbench-private.h"

#define STABLIZE_DELAY_MSEC 50

G_DEFINE_TYPE (IdeWorkbench, ide_workbench, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_VISIBLE_PERSPECTIVE,
  PROP_VISIBLE_PERSPECTIVE_NAME,
  LAST_PROP
};

enum {
  ACTION,
  SET_PERSPECTIVE,
  UNLOAD,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static void
ide_workbench_notify_visible_child (IdeWorkbench *self,
                                    GParamSpec   *pspec,
                                    GtkStack     *stack)
{
  IdePerspective *perspective;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (GTK_IS_STACK (stack));

  perspective = IDE_PERSPECTIVE (gtk_stack_get_visible_child (stack));

  if (perspective != NULL)
    {
      GActionGroup *actions;
      gchar *id;

      id = ide_perspective_get_id (perspective);
      gtk_stack_set_visible_child_name (self->titlebar_stack, id);

      actions = ide_perspective_get_actions (perspective);
      gtk_widget_insert_action_group (GTK_WIDGET (self), "perspective", actions);

      g_clear_object (&actions);
      g_free (id);
    }
}

static gint
ide_workbench_compare_perspective (gconstpointer a,
                                   gconstpointer b)
{
  IdePerspective *perspective_a = (IdePerspective *)a;
  IdePerspective *perspective_b = (IdePerspective *)b;

  return (ide_perspective_get_priority (perspective_a) -
          ide_perspective_get_priority (perspective_b));
}

static void
ide_workbench_resort_perspectives (IdeWorkbench *self)
{
  GList *children;
  const GList *iter;
  gint i = 0;

  g_assert (IDE_IS_WORKBENCH (self));

  children = gtk_container_get_children (GTK_CONTAINER (self->perspectives_stack));
  children = g_list_sort (children, ide_workbench_compare_perspective);

  for (iter = children; iter; iter = iter->next, i++)
    {
      GtkWidget *child = iter->data;

      gtk_container_child_set (GTK_CONTAINER (self->perspectives_stack), child,
                               "position", i,
                               NULL);
    }

  g_list_free (children);
}

static void
ide_workbench_unload_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(IdeWorkbench) self = user_data;
  IdeContext *context = (IdeContext *)object;

  g_return_if_fail (IDE_IS_WORKBENCH (self));

  ide_context_unload_finish (context, result, NULL);

  gtk_widget_destroy (GTK_WIDGET (self));
}

static gboolean
ide_workbench_agree_to_shutdown (IdeWorkbench *self)
{
  GList *children;
  const GList *iter;
  gboolean ret = TRUE;

  g_assert (IDE_IS_WORKBENCH (self));

  children = gtk_container_get_children (GTK_CONTAINER (self->perspectives_stack));

  for (iter = children; iter; iter = iter->next)
    {
      IdePerspective *perspective = iter->data;

      if (!ide_perspective_agree_to_shutdown (perspective))
        {
          ret = FALSE;
          break;
        }
    }

  g_list_free (children);

  return ret;
}

void
ide_workbench_set_selection_owner (IdeWorkbench *self,
                                   GObject      *object)
{
  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (G_IS_OBJECT (object) || object == NULL);

  self->selection_owner = object;
}

GObject *
ide_workbench_get_selection_owner (IdeWorkbench *self)
{
  g_assert (IDE_IS_WORKBENCH (self));

  return self->selection_owner;
}

static gboolean
ide_workbench_delete_event (GtkWidget   *widget,
                            GdkEventAny *event)
{
  IdeWorkbench *self = (IdeWorkbench *)widget;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (event != NULL);

  if (self->unloading)
    {
      g_cancellable_cancel (self->cancellable);
      return GDK_EVENT_STOP;
    }

  if (!ide_workbench_agree_to_shutdown (self))
    return GDK_EVENT_STOP;

  self->unloading = TRUE;

  g_signal_emit (self, signals [UNLOAD], 0, self->context);

  if (self->context != NULL)
    {
      self->cancellable = g_cancellable_new ();
      ide_context_unload_async (self->context,
                                self->cancellable,
                                ide_workbench_unload_cb,
                                g_object_ref (self));
      return GDK_EVENT_STOP;
    }

  g_clear_object (&self->addins);

  return GDK_EVENT_PROPAGATE;
}

static void
ide_workbench_constructed (GObject *object)
{
  IdeWorkbench *self = (IdeWorkbench *)object;

  G_OBJECT_CLASS (ide_workbench_parent_class)->constructed (object);

  ide_workbench_actions_init (self);
}

static void
ide_workbench_finalize (GObject *object)
{
  IdeWorkbench *self = (IdeWorkbench *)object;

  ide_clear_weak_pointer (&self->perspective);
  g_clear_object (&self->context);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ide_workbench_parent_class)->finalize (object);
}

static void
ide_workbench_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeWorkbench *self = IDE_WORKBENCH (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_workbench_get_context (self));
      break;

    case PROP_VISIBLE_PERSPECTIVE:
      g_value_set_object (value, ide_workbench_get_visible_perspective (self));
      break;

    case PROP_VISIBLE_PERSPECTIVE_NAME:
      g_value_set_string (value, ide_workbench_get_visible_perspective_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workbench_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeWorkbench *self = IDE_WORKBENCH (object);

  switch (prop_id)
    {
    case PROP_VISIBLE_PERSPECTIVE:
      ide_workbench_set_visible_perspective (self, g_value_get_object (value));
      break;

    case PROP_VISIBLE_PERSPECTIVE_NAME:
      ide_workbench_set_visible_perspective_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_workbench_class_init (IdeWorkbenchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_workbench_constructed;
  object_class->finalize = ide_workbench_finalize;
  object_class->get_property = ide_workbench_get_property;
  object_class->set_property = ide_workbench_set_property;

  widget_class->delete_event = ide_workbench_delete_event;

  /**
   * IdeWorkbench:context:
   *
   * The #IdeWorkbench:context property contains the #IdeContext for the loaded
   * project. Loading a project consists of creating an #IdeContext, so there
   * is a 1:1 mapping between "loaded project" and an #IdeContext.
   *
   * The #IdeContext contains many of the important components of a project.
   * For example, it contains the #IdeVcs representing the active version
   * control system and an #IdeBuildSystem representing the current build
   * system.
   *
   * The creation of #IdeWorkbenchAddin addins are deferred until this property
   * has been set.
   */
  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "Context",
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeWorkbench:visible-perspective:
   *
   * This property contains the #IdePerspective that is currently selected.
   * Connect to the "notify::visible-perspective" signal to be notified when
   * the perspective has been changed.
   */
  properties [PROP_VISIBLE_PERSPECTIVE] =
    g_param_spec_object ("visible-perspective",
                         "visible-Perspective",
                         "visible-Perspective",
                         IDE_TYPE_PERSPECTIVE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeWorkbench:visible-perspective-name:
   *
   * This property is just like #IdeWorkbench:visible-perspective except that
   * it contains the name of the perspective as a string.
   */
  properties [PROP_VISIBLE_PERSPECTIVE_NAME] =
    g_param_spec_string ("visible-perspective-name",
                         "visible-Perspective-name",
                         "visible-Perspective-name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [ACTION] =
    g_signal_new_class_handler ("action",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_widget_action_with_string),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                3,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING);

  /**
   * IdeWorkbench::set-perspective:
   * @self: An #IdeWorkbench
   * @name: the name of the perspective
   *
   * This signal is meant for keybindings to change the current perspective.
   */
  signals [SET_PERSPECTIVE] =
    g_signal_new_class_handler ("set-perspective",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_workbench_set_visible_perspective_name),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 1, G_TYPE_STRING);

  signals [UNLOAD] =
    g_signal_new ("unload",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, IDE_TYPE_CONTEXT);

  gtk_widget_class_set_css_name (widget_class, "workbench");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-workbench.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, header_size_group);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, perspectives_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, perspectives_stack_switcher);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, titlebar_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, top_stack);
}

static void
ide_workbench_init (IdeWorkbench *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  ide_workbench_add_perspective (self,
                                 g_object_new (IDE_TYPE_GREETER_PERSPECTIVE,
                                               "visible", TRUE,
                                               NULL));
  ide_workbench_add_perspective (self,
                                 g_object_new (IDE_TYPE_GENESIS_PERSPECTIVE,
                                               "visible", TRUE,
                                               NULL));
  ide_workbench_add_perspective (self,
                                 g_object_new (IDE_TYPE_PREFERENCES_PERSPECTIVE,
                                               "visible", TRUE,
                                               NULL));

  ide_window_settings_register (GTK_WINDOW (self));

  g_signal_connect_object (self->perspectives_stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_workbench_notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  ide_workbench_set_visible_perspective_name (self, "greeter");
}

static void
ide_workbench_views_foreach_cb (GtkWidget *widget,
                                gpointer   user_data)
{
  struct {
    GtkCallback callback;
    gpointer    user_data;
  } *closure = user_data;

  g_assert (IDE_IS_PERSPECTIVE (widget));
  g_assert (closure != NULL);
  g_assert (closure->callback != NULL);

  ide_perspective_views_foreach (IDE_PERSPECTIVE (widget), closure->callback, closure->user_data);
}

/**
 * ide_workbench_views_foreach:
 * @self: An #IdeWorkbench.
 * @callback: (scope call): The callback to execute
 * @user_data: user data for @callback.
 *
 * Executes @callback for every #IdeLayoutView across all perspectives.
 */
void
ide_workbench_views_foreach (IdeWorkbench *self,
                             GtkCallback   callback,
                             gpointer      user_data)
{
  struct {
    GtkCallback callback;
    gpointer    user_data;
  } closure = { callback, user_data };

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (callback != NULL);

  gtk_container_foreach (GTK_CONTAINER (self->perspectives_stack),
                         ide_workbench_views_foreach_cb,
                         &closure);
}

static void
ide_workbench_addin_added (PeasExtensionSet *set,
                           PeasPluginInfo   *plugin_info,
                           PeasExtension    *extension,
                           gpointer          user_data)
{
  IdeWorkbench *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKBENCH_ADDIN (extension));
  g_assert (IDE_IS_WORKBENCH (self));

  IDE_TRACE_MSG ("Loading workbench addin for %s",
                 peas_plugin_info_get_module_name (plugin_info));

  ide_workbench_addin_load (IDE_WORKBENCH_ADDIN (extension), self);
}

static void
ide_workbench_addin_removed (PeasExtensionSet *set,
                             PeasPluginInfo   *plugin_info,
                             PeasExtension    *extension,
                             gpointer          user_data)
{
  IdeWorkbench *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKBENCH_ADDIN (extension));
  g_assert (IDE_IS_WORKBENCH (self));

  ide_workbench_addin_unload (IDE_WORKBENCH_ADDIN (extension), self);
}

/**
 * ide_workbench_get_context:
 * @self: An #IdeWorkbench.
 *
 * Gets the context associated with the workbench, or %NULL.
 *
 * Returns: (transfer none) (nullable): An #IdeContext or %NULL.
 */
IdeContext *
ide_workbench_get_context (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  return self->context;
}

static gboolean
restore_in_timeout (gpointer data)
{
  g_autoptr(IdeContext) context = data;

  g_assert (IDE_IS_CONTEXT (context));

  ide_context_restore_async (context, NULL, NULL, NULL);

  return G_SOURCE_REMOVE;
}

static gboolean
stablize_cb (gpointer data)
{
  g_autoptr(IdeWorkbench) self = data;

  g_assert (IDE_IS_WORKBENCH (self));

  ide_workbench_set_visible_perspective_name (self, "editor");

  return G_SOURCE_REMOVE;
}

void
ide_workbench_set_context (IdeWorkbench *self,
                           IdeContext   *context)
{
  guint duration;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (self->context == NULL);

  g_set_object (&self->context, context);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_WORKBENCH_ADDIN,
                                         NULL);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_workbench_addin_added),
                    self);

  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_workbench_addin_removed),
                    self);

  peas_extension_set_foreach (self->addins, ide_workbench_addin_added, self);

  /*
   * Creating all the addins above is a bit intenstive, so give ourselves
   * just a bit of time to stablize allocations and sizing before
   * transitioning to the editor.
   */
  g_timeout_add (STABLIZE_DELAY_MSEC, stablize_cb, g_object_ref (self));

  /*
   * When restoring, previous buffers may get loaded. This causes new
   * widgets to be created and added to the workspace. Doing so during
   * the stack transition results in non-smooth transitions. So instead,
   * we will delay until the transition has completed.
   */
  duration = gtk_stack_get_transition_duration (self->top_stack);
  g_timeout_add (STABLIZE_DELAY_MSEC + duration, restore_in_timeout, g_object_ref (context));
}

void
ide_workbench_add_perspective (IdeWorkbench   *self,
                               IdePerspective *perspective)
{
  g_autofree gchar *icon_name = NULL;
  g_autofree gchar *id = NULL;
  g_autofree gchar *title = NULL;
  GtkStack *stack;
  GtkWidget *titlebar;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_PERSPECTIVE (perspective));

  id = ide_perspective_get_id (perspective);
  title = ide_perspective_get_title (perspective);
  icon_name = ide_perspective_get_icon_name (perspective);

  if (ide_perspective_is_early (perspective))
    stack = self->top_stack;
  else
    stack = self->perspectives_stack;

  gtk_widget_set_hexpand (GTK_WIDGET (perspective), TRUE);

  gtk_container_add_with_properties (GTK_CONTAINER (stack),
                                     GTK_WIDGET (perspective),
                                     "icon-name", icon_name,
                                     "name", id,
                                     "needs-attention", FALSE,
                                     "title", title,
                                     NULL);

  titlebar = ide_perspective_get_titlebar (perspective);
  if (titlebar == NULL)
    titlebar = g_object_new (IDE_TYPE_WORKBENCH_HEADER_BAR,
                             "visible", TRUE,
                             NULL);

  gtk_container_add_with_properties (GTK_CONTAINER (self->titlebar_stack), titlebar,
                                     "name", id,
                                     NULL);

  ide_workbench_resort_perspectives (self);
}

void
ide_workbench_remove_perspective (IdeWorkbench   *self,
                                  IdePerspective *perspective)
{
  const gchar *id;
  GtkWidget *titlebar;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_PERSPECTIVE (perspective));
  g_assert (gtk_widget_get_parent (GTK_WIDGET (perspective)) ==
            GTK_WIDGET (self->perspectives_stack));

  id = ide_perspective_get_id (perspective);
  titlebar = gtk_stack_get_child_by_name (self->titlebar_stack, id);

  gtk_container_remove (GTK_CONTAINER (self->titlebar_stack), titlebar);
  gtk_container_remove (GTK_CONTAINER (self->perspectives_stack), GTK_WIDGET (perspective));
}

/**
 * ide_workbench_get_perspective_by_name:
 *
 * Gets the perspective by it's registered name as defined in
 * ide_perspective_get_id().
 *
 * Returns: (nullable) (transfer none): An #IdePerspective or %NULL.
 */
IdePerspective *
ide_workbench_get_perspective_by_name (IdeWorkbench *self,
                                       const gchar  *name)
{
  GtkWidget *ret;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  ret = gtk_stack_get_child_by_name (self->perspectives_stack, name);
  if (ret == NULL)
    ret = gtk_stack_get_child_by_name (self->top_stack, name);

  return IDE_PERSPECTIVE (ret);
}

/**
 * ide_workbench_get_visible_perspective:
 * @self: An #IdeWorkbench.
 *
 * Gets the current perspective.
 *
 * Returns: (transfer none): An #IdePerspective.
 */
IdePerspective *
ide_workbench_get_visible_perspective (IdeWorkbench *self)
{
  GtkWidget *visible_child;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  visible_child = gtk_stack_get_visible_child (self->top_stack);

  if (IDE_IS_PERSPECTIVE (visible_child))
    return IDE_PERSPECTIVE (visible_child);

  return IDE_PERSPECTIVE (gtk_stack_get_visible_child (self->perspectives_stack));
}

void
ide_workbench_set_visible_perspective (IdeWorkbench   *self,
                                       IdePerspective *perspective)
{
  GActionGroup *actions;
  GtkStack *stack;
  gchar *id;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_PERSPECTIVE (perspective));

  stack = GTK_STACK (gtk_widget_get_ancestor (GTK_WIDGET (perspective), GTK_TYPE_STACK));

  id = ide_perspective_get_id (perspective);

  if (!ide_str_equal0 (gtk_stack_get_visible_child_name (stack), id))
    {
      gtk_stack_set_visible_child_name (stack, id);
      gtk_stack_set_visible_child_name (self->titlebar_stack, id);
    }

  g_free (id);

  actions = ide_perspective_get_actions (perspective);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "perspective", actions);

  if ((stack == self->perspectives_stack) &&
      !ide_str_equal0 (gtk_stack_get_visible_child_name (self->top_stack), "perspectives"))
    gtk_stack_set_visible_child_name (self->top_stack, "perspectives");
}

const gchar *
ide_workbench_get_visible_perspective_name (IdeWorkbench *self)
{
  IdePerspective *perspective;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  perspective = ide_workbench_get_visible_perspective (self);

  if (perspective != NULL)
    {
      GtkWidget *parent;

      /*
       * Normally we would call ide_perspective_get_id(), but we want to be
       * able to return a const gchar*. So instead we just use the registered
       * name in the stack, which is the same thing.
       */
      parent = gtk_widget_get_parent (GTK_WIDGET (perspective));
      return gtk_stack_get_visible_child_name (GTK_STACK (parent));
    }

  return NULL;
}

void
ide_workbench_set_visible_perspective_name (IdeWorkbench *self,
                                            const gchar  *name)
{
  IdePerspective *perspective;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (name != NULL);

  perspective = ide_workbench_get_perspective_by_name (self, name);
  if (perspective != NULL)
    ide_workbench_set_visible_perspective (self, perspective);
}

static void
ide_workbench_show_parents (GtkWidget *widget)
{
  GtkWidget *parent;

  g_assert (GTK_IS_WIDGET (widget));

  parent = gtk_widget_get_parent (widget);

  if (IDE_IS_LAYOUT_PANE (widget))
    pnl_dock_revealer_set_reveal_child (PNL_DOCK_REVEALER (widget), TRUE);

  if (IDE_IS_PERSPECTIVE (widget))
    ide_workbench_set_visible_perspective (ide_widget_get_workbench (widget),
                                           IDE_PERSPECTIVE (widget));

  if (GTK_IS_STACK (parent))
    gtk_stack_set_visible_child (GTK_STACK (parent), widget);

  if (parent != NULL)
    ide_workbench_show_parents (parent);
}

void
ide_workbench_focus (IdeWorkbench *self,
                     GtkWidget    *widget)
{
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  ide_workbench_show_parents (widget);
  gtk_widget_grab_focus (widget);
}
