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

#include <glib/gi18n.h>

#include "ide-debug.h"
#include "ide-macros.h"

#include "application/ide-application.h"
#include "application/ide-application-actions.h"
#include "editor/ide-editor-perspective.h"
#include "greeter/ide-greeter-perspective.h"
#include "preferences/ide-preferences-perspective.h"
#include "util/ide-gtk.h"
#include "util/ide-window-settings.h"
#include "workbench/ide-layout-pane.h"
#include "workbench/ide-layout-stack.h"
#include "workbench/ide-layout-view.h"
#include "workbench/ide-layout.h"
#include "workbench/ide-workbench-addin.h"
#include "workbench/ide-workbench-header-bar.h"
#include "workbench/ide-workbench-private.h"
#include "workbench/ide-workbench.h"

#define STABLIZE_DELAY_MSEC 50

G_DEFINE_TYPE (IdeWorkbench, ide_workbench, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_DISABLE_GREETER,
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
  GActionGroup *actions = NULL;
  IdePerspective *perspective;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (GTK_IS_STACK (stack));

  perspective = IDE_PERSPECTIVE (gtk_stack_get_visible_child (stack));
  if (perspective != NULL)
    actions = ide_perspective_get_actions (perspective);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "perspective", actions);

  g_clear_object (&actions);
}

static gint
ide_workbench_compare_perspective (gconstpointer a,
                                   gconstpointer b,
                                   gpointer      data_unused)
{
  IdePerspective *perspective_a = (IdePerspective *)a;
  IdePerspective *perspective_b = (IdePerspective *)b;

  return (ide_perspective_get_priority (perspective_a) -
          ide_perspective_get_priority (perspective_b));
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

  gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (self), FALSE);

  ide_workbench_add_perspective (self,
                                 g_object_new (IDE_TYPE_PREFERENCES_PERSPECTIVE,
                                               "visible", TRUE,
                                               NULL));

  if (self->disable_greeter == FALSE)
    {
      ide_workbench_add_perspective (self,
                                     g_object_new (IDE_TYPE_GREETER_PERSPECTIVE,
                                                   "visible", TRUE,
                                                   NULL));

      ide_workbench_set_visible_perspective_name (self, "greeter");
    }

  ide_workbench_actions_init (self);
}

static void
ide_workbench_finalize (GObject *object)
{
  IdeWorkbench *self = (IdeWorkbench *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->perspectives);

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

    case PROP_DISABLE_GREETER:
      g_value_set_boolean (value, self->disable_greeter);
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
    case PROP_DISABLE_GREETER:
      self->disable_greeter = g_value_get_boolean (value);
      break;

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
   * IdeWorkbench:disable-greeter:
   *
   * This property is used internally by Builder to avoid creating the
   * greeter when opening a new workspace that is only for loading a
   * project.
   *
   * This should not be used by application plugins.
   */
  properties [PROP_DISABLE_GREETER] =
    g_param_spec_boolean ("disable-greeter",
                          "Disable Greeter",
                          "If the greeter should be disabled when creating the workbench",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-workbench.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, header_size_group);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, header_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, message_box);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, perspective_menu_button);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, perspectives_stack);
}

static void
ide_workbench_init (IdeWorkbench *self)
{
  g_autoptr(GtkWindowGroup) window_group = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->perspectives = g_list_store_new (IDE_TYPE_PERSPECTIVE);

  ide_window_settings_register (GTK_WINDOW (self));

  g_signal_connect_object (self->perspectives_stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_workbench_notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  window_group = gtk_window_group_new ();
  gtk_window_group_add_window (window_group, GTK_WINDOW (self));
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

static gboolean
transform_title (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      user_data)
{
  const gchar *name = g_value_get_string (from_value);

  if (name != NULL)
    g_value_take_string (to_value, g_strdup_printf (_("%s — Builder"), name));
  else
    g_value_set_static_string (to_value, _("Builder"));

  return TRUE;
}

void
ide_workbench_set_context (IdeWorkbench *self,
                           IdeContext   *context)
{
  g_autoptr(GSettings) settings = NULL;
  IdeBuildManager *build_manager;
  IdeDebugManager *debug_manager;
  IdeRunManager *run_manager;
  IdeProject *project;
  guint delay_msec;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (self->context == NULL);

  settings = g_settings_new ("org.gnome.builder");

  g_set_object (&self->context, context);

  project = ide_context_get_project (context);
  g_object_bind_property_full (project, "name",
                               self, "title",
                               G_BINDING_SYNC_CREATE,
                               transform_title, NULL, NULL, NULL);

  build_manager = ide_context_get_build_manager (context);
  run_manager = ide_context_get_run_manager (context);
  debug_manager = ide_context_get_debug_manager (context);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "build-manager", G_ACTION_GROUP (build_manager));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "debug-manager", G_ACTION_GROUP (debug_manager));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "run-manager", G_ACTION_GROUP (run_manager));

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

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);

  /*
   * Creating all the addins above is a bit intensive, so give ourselves
   * just a bit of time to stablize allocations and sizing before
   * transitioning to the editor.
   */
  delay_msec = self->disable_greeter ? 0 : STABLIZE_DELAY_MSEC;
  g_timeout_add (delay_msec, stablize_cb, g_object_ref (self));

  /*
   * When restoring, previous buffers may get loaded. This causes new
   * widgets to be created and added to the workspace. Doing so during
   * the stack transition results in non-smooth transitions. So instead,
   * we will delay until the transition has completed.
   */
  if (g_settings_get_boolean (settings, "restore-previous-files"))
    {
      guint duration = 0;

      if (!self->disable_greeter)
        duration = gtk_stack_get_transition_duration (self->perspectives_stack);
      g_timeout_add (delay_msec + duration, restore_in_timeout, g_object_ref (context));
    }

  IDE_EXIT;
}

void
ide_workbench_add_perspective (IdeWorkbench   *self,
                               IdePerspective *perspective)
{
  g_autofree gchar *accel= NULL;
  g_autofree gchar *icon_name = NULL;
  g_autofree gchar *id = NULL;
  g_autofree gchar *title = NULL;
  GtkWidget *titlebar;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_PERSPECTIVE (perspective));

  id = ide_perspective_get_id (perspective);
  title = ide_perspective_get_title (perspective);
  icon_name = ide_perspective_get_icon_name (perspective);
  titlebar = ide_perspective_get_titlebar (perspective);

  gtk_container_add_with_properties (GTK_CONTAINER (self->perspectives_stack),
                                     GTK_WIDGET (perspective),
                                     "icon-name", icon_name,
                                     "name", id,
                                     "needs-attention", FALSE,
                                     "title", title,
                                     NULL);

  if (titlebar != NULL)
    gtk_container_add_with_properties (GTK_CONTAINER (self->header_stack), titlebar,
                                       "name", id,
                                       NULL);

  if (!IDE_IS_GREETER_PERSPECTIVE (perspective))
    {
      guint position = 0;

      gtk_container_child_get (GTK_CONTAINER (self->perspectives_stack),
                               GTK_WIDGET (perspective),
                               "position", &position,
                               NULL);

      g_list_store_append (self->perspectives, perspective);
      g_list_store_sort (self->perspectives,
                         ide_workbench_compare_perspective,
                         NULL);
    }

  accel = ide_perspective_get_accelerator (perspective);

  if (accel != NULL)
    {
      const gchar *accel_map[] = { accel, NULL };
      g_autofree gchar *action_name = NULL;

      action_name = g_strdup_printf ("win.perspective('%s')", id);
      gtk_application_set_accels_for_action (GTK_APPLICATION (IDE_APPLICATION_DEFAULT),
                                             action_name, accel_map);

    }
}

void
ide_workbench_remove_perspective (IdeWorkbench   *self,
                                  IdePerspective *perspective)
{
  guint n_items;
  guint i;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_PERSPECTIVE (perspective));
  g_assert (gtk_widget_get_parent (GTK_WIDGET (perspective)) ==
            GTK_WIDGET (self->perspectives_stack));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->perspectives));

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(IdePerspective) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (self->perspectives), i);

      if (item == perspective)
        {
          g_list_store_remove (self->perspectives, i);
          break;
        }
    }

  gtk_container_remove (GTK_CONTAINER (self->perspectives_stack),
                        GTK_WIDGET (perspective));
}

/**
 * ide_workbench_get_perspective_by_name:
 *
 * Gets the perspective by its registered name as defined in
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
  GtkWidget *ret;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  ret = gtk_stack_get_visible_child (self->perspectives_stack);

  return IDE_PERSPECTIVE (ret);
}

static void
ide_workbench_notify_perspective_set (PeasExtensionSet *set,
                                      PeasPluginInfo   *plugin_info,
                                      PeasExtension    *exten,
                                      gpointer          user_data)
{
  IdeWorkbenchAddin *addin = (IdeWorkbenchAddin *)exten;
  IdePerspective *perspective = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_PERSPECTIVE (perspective));

  ide_workbench_addin_perspective_set (addin, perspective);
}

static void
do_remove_early_perspectives (GtkWidget *widget,
                              gpointer   user_data)
{
  if (IDE_IS_GREETER_PERSPECTIVE (widget))
    gtk_widget_destroy (widget);
}

static void
remove_early_perspectives (IdeWorkbench *self)
{
  g_assert (IDE_IS_WORKBENCH (self));

  if (self->early_perspectives_removed)
    return;

  gtk_container_foreach (GTK_CONTAINER (self->perspectives_stack),
                         do_remove_early_perspectives,
                         NULL);

  self->early_perspectives_removed = TRUE;
}

void
ide_workbench_set_visible_perspective (IdeWorkbench   *self,
                                       IdePerspective *perspective)
{
  g_autofree gchar *id = NULL;
  GActionGroup *actions = NULL;
  const gchar *current_id;
  GtkWidget *titlebar;
  guint restore_duration = 0;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_PERSPECTIVE (perspective));

  /*
   * If we can detect that this is the first transition to the editor,
   * and that our window is not yet displayed, we can avoid the transition
   * duration altogether. This is handy when first opening a window with
   * a project loaded, as used by self->disable_greeter.
   */
  if (self->disable_greeter &&
      IDE_IS_EDITOR_PERSPECTIVE (perspective) &&
      !self->did_initial_editor_transition)
    {
      self->did_initial_editor_transition = TRUE;
      restore_duration = gtk_stack_get_transition_duration (self->perspectives_stack);
      gtk_stack_set_transition_duration (self->perspectives_stack, 0);
    }

  current_id = gtk_stack_get_visible_child_name (self->perspectives_stack);
  id = ide_perspective_get_id (perspective);

  if (!ide_str_equal0 (current_id, id))
    gtk_stack_set_visible_child_name (self->perspectives_stack, id);

  titlebar = gtk_stack_get_child_by_name (self->header_stack, id);

  if (titlebar != NULL)
    gtk_stack_set_visible_child (self->header_stack, titlebar);
  else
    gtk_stack_set_visible_child (self->header_stack, GTK_WIDGET (self->header_bar));

  actions = ide_perspective_get_actions (perspective);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "perspective", actions);

  /*
   * If we are transitioning to the editor the first time, we can
   * remove the early perspectives (greeter, etc).
   */
  if (IDE_IS_EDITOR_PERSPECTIVE (perspective))
    remove_early_perspectives (self);

  gtk_widget_set_visible (GTK_WIDGET (self->perspective_menu_button),
                          !ide_perspective_is_early (perspective));

  if (self->addins != NULL)
    peas_extension_set_foreach (self->addins,
                                ide_workbench_notify_perspective_set,
                                perspective);

  g_clear_object (&actions);

  if (restore_duration != 0)
    gtk_stack_set_transition_duration (self->perspectives_stack, restore_duration);

  /* Notify the application to possibly update actions such
   * as the preferences state.
   */
  ide_application_actions_update (IDE_APPLICATION_DEFAULT);
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

/**
 * ide_workbench_get_headerbar:
 *
 * Helper that is equivalent to calling gtk_window_get_titlebar() and casting
 * to an #IdeWorkbenchHeaderBar. This is convenience for plugins.
 *
 * Returns: (transfer none): An #IdeWorkbenchHeaderBar.
 */
IdeWorkbenchHeaderBar *
ide_workbench_get_headerbar (IdeWorkbench *self)
{
  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  return self->header_bar;
}

static void
ide_workbench_message_response (IdeWorkbench        *self,
                                gint                 response_id,
                                IdeWorkbenchMessage *message)
{
  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_WORKBENCH_MESSAGE (message));

  if (response_id == GTK_RESPONSE_CLOSE)
    gtk_widget_hide (GTK_WIDGET (message));
}

void
ide_workbench_push_message (IdeWorkbench        *self,
                            IdeWorkbenchMessage *message)
{
  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_WORKBENCH_MESSAGE (message));

  g_signal_connect_object (message,
                           "response",
                           G_CALLBACK (ide_workbench_message_response),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_container_add (GTK_CONTAINER (self->message_box), GTK_WIDGET (message));
}

static void
locate_widget_for_message_id (GtkWidget *widget,
                              gpointer user_data)
{
  struct {
    const gchar *id;
    GtkWidget   *widget;
  } *lookup = user_data;

  if (IDE_IS_WORKBENCH_MESSAGE (widget))
    {
      const gchar *id;

      id = ide_workbench_message_get_id (IDE_WORKBENCH_MESSAGE (widget));

      if (g_strcmp0 (id, lookup->id) == 0)
        lookup->widget = widget;
    }
}

gboolean
ide_workbench_pop_message (IdeWorkbench *self,
                           const gchar  *message_id)
{
  struct {
    const gchar *id;
    GtkWidget   *widget;
  } lookup = { message_id };

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), FALSE);
  g_return_val_if_fail (message_id != NULL, FALSE);

  gtk_container_foreach (GTK_CONTAINER (self->message_box),
                         locate_widget_for_message_id,
                         &lookup);

  if (lookup.widget)
    {
      gtk_widget_destroy (lookup.widget);
      return TRUE;
    }

  return FALSE;
}
