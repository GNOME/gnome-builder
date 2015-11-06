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

#include "ide-macros.h"
#include "ide-window-settings.h"
#include "ide-workbench.h"
#include "ide-workbench-addin.h"
#include "ide-workbench-header-bar.h"
#include "ide-workbench-private.h"

G_DEFINE_TYPE (IdeWorkbench, ide_workbench, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_PERSPECTIVE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

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

static void
ide_workbench_finalize (GObject *object)
{
  IdeWorkbench *self = (IdeWorkbench *)object;

  ide_clear_weak_pointer (&self->perspective);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (ide_workbench_parent_class)->finalize (object);
}

static void
ide_workbench_constructed (GObject *object)
{
  G_OBJECT_CLASS (ide_workbench_parent_class)->constructed (object);
}

static void
ide_workbench_destroy (GtkWidget *widget)
{
  IdeWorkbench *self = (IdeWorkbench *)widget;

  g_assert (IDE_IS_WORKBENCH (self));

  GTK_WIDGET_CLASS (ide_workbench_parent_class)->destroy (widget);
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
    case PROP_PERSPECTIVE:
      g_value_set_object (value, ide_workbench_get_perspective (self));
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
    case PROP_PERSPECTIVE:
      ide_workbench_set_perspective (self, g_value_get_object (value));
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

  widget_class->destroy = ide_workbench_destroy;

  properties [PROP_PERSPECTIVE] =
    g_param_spec_object ("perspective",
                         "Perspective",
                         "Perspective",
                         IDE_TYPE_PERSPECTIVE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_css_name (widget_class, "workbench");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-workbench.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, perspectives_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, perspectives_stack_switcher);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, titlebar_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeWorkbench, top_stack);
}

static void
ide_workbench_init_greeter (IdeWorkbench *self)
{
  g_assert (IDE_IS_WORKBENCH (self));

  self->greeter_perspective = g_object_new (IDE_TYPE_GREETER_PERSPECTIVE,
                                            "visible", TRUE,
                                            NULL);
  ide_workbench_add_perspective (self, IDE_PERSPECTIVE (self->greeter_perspective));
  gtk_container_child_set (GTK_CONTAINER (self->titlebar_stack),
                           ide_perspective_get_titlebar (IDE_PERSPECTIVE (self->greeter_perspective)),
                           "position", 0,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (self->top_stack),
                           GTK_WIDGET (self->greeter_perspective),
                           "position", 0,
                           NULL);
  ide_workbench_set_perspective (self, IDE_PERSPECTIVE (self->greeter_perspective));
}

static void
ide_workbench_init_editor (IdeWorkbench *self)
{
  g_assert (IDE_IS_WORKBENCH (self));

  self->editor_perspective = g_object_new (IDE_TYPE_EDITOR_PERSPECTIVE,
                                           "visible", TRUE,
                                           NULL);
  ide_workbench_add_perspective (self, IDE_PERSPECTIVE (self->editor_perspective));
}

static void
ide_workbench_init_preferences (IdeWorkbench *self)
{
  g_assert (IDE_IS_WORKBENCH (self));

  self->preferences_perspective = g_object_new (IDE_TYPE_PREFERENCES_PERSPECTIVE,
                                                "visible", TRUE,
                                                NULL);
  ide_workbench_add_perspective (self, IDE_PERSPECTIVE (self->preferences_perspective));
}

static void
ide_workbench_init (IdeWorkbench *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  ide_workbench_init_greeter (self);
  ide_workbench_init_editor (self);
  ide_workbench_init_preferences (self);
  ide_window_settings_register (GTK_WINDOW (self));

  g_signal_connect_object (self->perspectives_stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_workbench_notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_stack_set_visible_child (self->top_stack, GTK_WIDGET (self->greeter_perspective));
}

static void
ide_workbench_views_foreach_cb (GtkWidget *widget,
                                gpointer   user_data)
{
  IdeWorkbenchForeach *foreach_data = user_data;

  g_assert (foreach_data);
  g_assert (foreach_data->callback);

  foreach_data->callback (widget, foreach_data->user_data);
}

/**
 * ide_workbench_views_foreach:
 * @self: An #IdeWorkbench.
 * @callback: (scope call): The callback to execute
 * @user_data: user data for @callback.
 *
 * Executes @callback for every #IdeView across all perspectives.
 */
void
ide_workbench_views_foreach (IdeWorkbench *self,
                             GtkCallback   callback,
                             gpointer      user_data)
{
  IdeWorkbenchForeach foreach = { callback, user_data };

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (callback != NULL);

  gtk_container_foreach (GTK_CONTAINER (self->perspectives_stack),
                         ide_workbench_views_foreach_cb,
                         &foreach);
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

void
_ide_workbench_set_context (IdeWorkbench *self,
                            IdeContext   *context)
{
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

  ide_workbench_set_perspective (self, IDE_PERSPECTIVE (self->editor_perspective));

  gtk_stack_set_visible_child_name (self->top_stack, "perspectives");
}

void
ide_workbench_add_perspective (IdeWorkbench   *self,
                               IdePerspective *perspective)
{
  const gchar *id;
  const gchar *title;
  const gchar *icon_name;
  GtkStack *stack;
  GtkWidget *titlebar;

  g_assert (IDE_IS_WORKBENCH (self));
  g_assert (IDE_IS_PERSPECTIVE (perspective));

  id = ide_perspective_get_id (perspective);
  title = ide_perspective_get_title (perspective);
  icon_name = ide_perspective_get_icon_name (perspective);

  if (IDE_IS_GREETER_PERSPECTIVE (perspective))
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
 * ide_workbench_get_perspective:
 * @self: An #IdeWorkbench.
 *
 * Gets the current perspective.
 *
 * Returns: (transfer none): An #IdePerspective.
 */
IdePerspective *
ide_workbench_get_perspective (IdeWorkbench *self)
{
  GtkWidget *visible_child;

  g_return_val_if_fail (IDE_IS_WORKBENCH (self), NULL);

  visible_child = gtk_stack_get_visible_child (self->perspectives_stack);

  return IDE_PERSPECTIVE (visible_child);
}

void
ide_workbench_set_perspective (IdeWorkbench   *self,
                               IdePerspective *perspective)
{
  GActionGroup *actions;
  GtkStack *stack;
  gchar *id;

  g_return_if_fail (IDE_IS_WORKBENCH (self));
  g_return_if_fail (IDE_IS_PERSPECTIVE (perspective));

  /*
   * NOTE:
   *
   * The greeter perspective is special cased. We want to use the same window for the greeter
   * and the workbench, so it has a toplevel stack with slightly different semantics than the
   * other perspectives. We don't show the perspective sidebar, and we use a slide-right-left
   * animation.
   *
   * Once we leave the greeter, we do not come back to it. We only use crossfade animations from
   * then on.
   */

  if (IDE_IS_GREETER_PERSPECTIVE (perspective))
    stack = self->top_stack;
  else
    stack = self->perspectives_stack;

  id = ide_perspective_get_id (perspective);
  gtk_stack_set_visible_child_name (stack, id);
  gtk_stack_set_visible_child_name (self->titlebar_stack, id);

  actions = ide_perspective_get_actions (perspective);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "perspective", actions);

  if (!IDE_IS_GREETER_PERSPECTIVE (perspective))
    gtk_stack_set_transition_type (self->titlebar_stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);

  g_free (id);
}
