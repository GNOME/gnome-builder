/* ide-omni-bar.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-omni-bar"

#include <glib/gi18n.h>
#include <dazzle.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-system.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-configuration-manager.h"
#include "projects/ide-project.h"
#include "runtimes/ide-runtime.h"
#include "util/ide-gtk.h"
#include "vcs/ide-vcs.h"
#include "workbench/ide-omni-bar.h"

#define LOOPER_INTERVAL_SECONDS 5
#define SETTLE_MESSAGE_COUNT 2

struct _IdeOmniBar
{
  GtkBox parent_instance;

  /*
   * This source is used to loop through the various messages that are
   * available. It runs on a regular interval (LOOPER_INTERVAL_SECONDS).
   * It isn't very smart, it doesn't even reset when the messages are
   * changed.
   */
  GSource *looper_source;

  /*
   * This gesture is used to track "clicks" inside the omnibar. Upon
   * click, the popover is displayed (or hidden) as necessary.
   */
  GtkGesture *gesture;

  /*
   * This manages the bindings we need for the IdeBuildManager instance.
   * This includes various label text and state tracking to determine
   * what actions we can apply and when.
   */
  DzlBindingGroup *build_manager_bindings;

  /*
   * This manages the signals we need for the IdeBuildManager instance.
   * This includes tracking build start/failure/finished.
   */
  DzlSignalGroup *build_manager_signals;

  /*
   * This manages the bindings we need for the IdeConfigurationManager
   * such as the current configuration name.
   */
  DzlBindingGroup *config_manager_bindings;

  /*
   * This manages the signals we need from the IdeConfigurationManager
   * such as when the current configuration has been changed.
   */
  DzlSignalGroup *config_manager_signals;

  /*
   * This manages the bindings we need for the IdeProject which binds
   * some information to the popover.
   */
  DzlBindingGroup *project_bindings;

  /*
   * This manages the bindings we need for the IdeVcs such as the
   * current branch name.
   */
  DzlBindingGroup *vcs_bindings;

  /*
   * This tracks the number of times we have shown the current build
   * message while looping between the various messages. After our
   * SETTLE_MESSAGE_COUNT has been reached, we stop flapping between
   * messages.
   */
  guint seen_count;

  /*
   * Just tracks if we have already done a build so we can change
   * how we display user messages.
   */
  guint did_build : 1;

  /*
   * The following are template children from the GtkBuilder template.
   */
  GtkLabel             *branch_label;
  GtkEventBox          *event_box;
  GtkLabel             *project_label;
  GtkBox               *branch_box;
  GtkImage             *build_result_diagnostics_image;
  GtkButton            *build_button;
  GtkShortcutsShortcut *build_button_shortcut;
  GtkButton            *cancel_button;
  GtkLabel             *config_name_label;
  GtkStack             *message_stack;
  GtkListBox           *operations_list;
  GtkPopover           *popover;
  GtkLabel             *popover_branch_label;
  GtkLabel             *popover_config_label;
  GtkLabel             *popover_build_result_label;
  GtkRevealer          *popover_details_revealer;
  GtkLabel             *popover_errors_label;
  GtkLabel             *popover_last_build_time_label;
  GtkLabel             *popover_runtime_label;
  GtkLabel             *popover_project_label;
  GtkLabel             *popover_warnings_label;
};

G_DEFINE_TYPE (IdeOmniBar, ide_omni_bar, GTK_TYPE_BOX)

static gboolean
date_time_to_label (GBinding     *binding,
                    const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  GDateTime *dt;

  g_assert (G_IS_BINDING (binding));
  g_assert (from_value != NULL);
  g_assert (G_VALUE_HOLDS (from_value, G_TYPE_DATE_TIME));
  g_assert (to_value != NULL);
  g_assert (G_VALUE_HOLDS (to_value, G_TYPE_STRING));

  if (NULL != (dt = g_value_get_boxed (from_value)))
    g_value_take_string (to_value, g_date_time_format (dt, "%X"));

  return TRUE;
}

static void
ide_omni_bar_update (IdeOmniBar *self)
{
  g_autofree gchar *branch_name = NULL;
  const gchar *project_name = NULL;
  IdeContext *context;

  g_assert (IDE_IS_OMNI_BAR (self));

  context = ide_widget_get_context (GTK_WIDGET (self));

  if (IDE_IS_CONTEXT (context))
    {
      IdeProject *project;
      IdeVcs *vcs;

      project = ide_context_get_project (context);
      project_name = ide_project_get_name (project);

      vcs = ide_context_get_vcs (context);
      branch_name = ide_vcs_get_branch_name (vcs);
    }

  gtk_label_set_label (self->project_label, project_name);
  gtk_label_set_label (self->branch_label, branch_name);
  gtk_label_set_label (self->popover_branch_label, branch_name);
}

static void
ide_omni_bar__config_manager__notify_current (IdeOmniBar              *self,
                                              GParamSpec              *pspec,
                                              IdeConfigurationManager *config_manager)
{
  IdeConfiguration *current;
  IdeRuntime *runtime;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (config_manager));

  current = ide_configuration_manager_get_current (config_manager);
  runtime = ide_configuration_get_runtime (current);

  if (runtime != NULL)
    gtk_label_set_label (self->popover_runtime_label, ide_runtime_get_display_name (runtime));
  else
    gtk_label_set_label (self->popover_runtime_label, "");

  gtk_label_set_label (self->popover_config_label,
                       ide_configuration_get_display_name (current));
}

static void
ide_omni_bar_context_set (GtkWidget  *widget,
                          IdeContext *context)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;
  IdeConfigurationManager *config_manager = NULL;
  IdeBuildManager *build_manager = NULL;
  IdeProject *project = NULL;
  IdeVcs *vcs = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  ide_omni_bar_update (self);

  if (context != NULL)
    {
      vcs = ide_context_get_vcs (context);
      build_manager = ide_context_get_build_manager (context);
      config_manager = ide_context_get_configuration_manager (context);
      project = ide_context_get_project (context);
    }

  dzl_binding_group_set_source (self->build_manager_bindings, build_manager);
  dzl_signal_group_set_target (self->build_manager_signals, build_manager);
  dzl_binding_group_set_source (self->config_manager_bindings, config_manager);
  dzl_signal_group_set_target (self->config_manager_signals, config_manager);
  dzl_binding_group_set_source (self->project_bindings, project);
  dzl_binding_group_set_source (self->vcs_bindings, vcs);

  if (config_manager != NULL)
    ide_omni_bar__config_manager__notify_current (self, NULL, config_manager);

  IDE_EXIT;
}

static gboolean
event_box_enter_notify (IdeOmniBar  *self,
                        GdkEvent    *event,
                        GtkEventBox *event_box)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_EVENT_BOX (event_box));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);

  gtk_style_context_set_state (style_context, state_flags | GTK_STATE_FLAG_PRELIGHT);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
event_box_leave_notify (IdeOmniBar  *self,
                        GdkEvent    *event,
                        GtkEventBox *event_box)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_EVENT_BOX (event_box));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);

  gtk_style_context_set_state (style_context, state_flags & ~GTK_STATE_FLAG_PRELIGHT);

  return GDK_EVENT_PROPAGATE;
}

static void
ide_omni_bar_next_message (IdeOmniBar *self)
{
  IdeBuildManager *build_manager;
  const gchar *name;
  IdeContext *context;

  g_assert (IDE_IS_OMNI_BAR (self));

  if (NULL == (context = ide_widget_get_context (GTK_WIDGET (self))))
    return;

  build_manager = ide_context_get_build_manager (context);

  name = gtk_stack_get_visible_child_name (self->message_stack);

  /*
   * TODO: This isn't the cleanest way to do this.
   *       We need to come up with a strategy for moving between these
   *       in a way that has a "check" function to determine if we can
   *       toggle to the next child.
   */

  if (g_strcmp0 (name, "config") == 0)
    {
      /* Only rotate to build result if we have one and we haven't
       * flapped too many times.
       */
      if (self->did_build && self->seen_count < 2)
        gtk_stack_set_visible_child_name (self->message_stack, "build");
    }
  else if (!ide_build_manager_get_busy (build_manager))
    {
      self->seen_count++;
      gtk_stack_set_visible_child_name (self->message_stack, "config");
    }
}

static gboolean
ide_omni_bar_looper_cb (gpointer user_data)
{
  IdeOmniBar *self = user_data;

  g_assert (IDE_IS_OMNI_BAR (self));

  ide_omni_bar_next_message (self);

  return G_SOURCE_CONTINUE;
}

static void
ide_omni_bar_constructed (GObject *object)
{
  IdeOmniBar *self = (IdeOmniBar *)object;

  g_assert (IDE_IS_OMNI_BAR (self));

  G_OBJECT_CLASS (ide_omni_bar_parent_class)->constructed (object);

  /*
   * Start our looper, to loop through available messages.
   * We will release this in destroy.
   */
  self->looper_source = g_timeout_source_new_seconds (LOOPER_INTERVAL_SECONDS);
  g_source_set_callback (self->looper_source, ide_omni_bar_looper_cb, self, NULL);
  g_source_set_name (self->looper_source, "[ide] omnibar message looper");
  g_source_attach (self->looper_source, NULL);
}

static void
multipress_pressed_cb (GtkGestureMultiPress *gesture,
                       guint                 n_press,
                       gdouble               x,
                       gdouble               y,
                       IdeOmniBar           *self)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));

  gtk_popover_popup (self->popover);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);
  gtk_style_context_set_state (style_context, state_flags | GTK_STATE_FLAG_ACTIVE);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
ide_omni_bar_popover_closed (IdeOmniBar *self,
                             GtkPopover *popover)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_POPOVER (popover));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);
  gtk_style_context_set_state (style_context, state_flags & ~GTK_STATE_FLAG_ACTIVE);
}

static void
ide_omni_bar__build_manager__build_started (IdeOmniBar       *self,
                                            IdeBuildPipeline *build_pipeline,
                                            IdeBuildManager  *build_manager)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_BUILD_PIPELINE (build_pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  self->did_build = TRUE;
  self->seen_count = 0;

  gtk_revealer_set_reveal_child (self->popover_details_revealer, TRUE);

  gtk_label_set_label (self->popover_build_result_label, _("Building"));
  gtk_label_set_attributes (self->popover_build_result_label, NULL);
  dzl_gtk_widget_remove_style_class (GTK_WIDGET (self->popover_build_result_label), "error");
  dzl_gtk_widget_remove_style_class (GTK_WIDGET (self->popover_build_result_label), "success");

  dzl_gtk_widget_add_style_class (GTK_WIDGET (self), "building");
}

static void
ide_omni_bar__build_manager__build_failed (IdeOmniBar       *self,
                                           IdeBuildPipeline *build_pipeline,
                                           IdeBuildManager  *build_manager)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_BUILD_PIPELINE (build_pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  gtk_label_set_label (self->popover_build_result_label, _("Failed"));
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->popover_build_result_label), "error");

  dzl_gtk_widget_remove_style_class (GTK_WIDGET (self), "building");
}

static void
ide_omni_bar__build_manager__build_finished (IdeOmniBar       *self,
                                             IdeBuildPipeline *build_pipeline,
                                             IdeBuildManager  *build_manager)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_BUILD_PIPELINE (build_pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  gtk_label_set_label (self->popover_build_result_label, _("Success"));
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->popover_build_result_label), "success");

  dzl_gtk_widget_remove_style_class (GTK_WIDGET (self), "building");
}

static void
ide_omni_bar__build_button__query_tooltip (IdeOmniBar *self,
                                           gint        x,
                                           gint        y,
                                           gboolean    keyboard,
                                           GtkTooltip *tooltip,
                                           GtkButton  *button)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_TOOLTIP (tooltip));
  g_assert (GTK_IS_BUTTON (button));

  gtk_tooltip_set_custom (tooltip, GTK_WIDGET (self->build_button_shortcut));
}

static void
ide_omni_bar_destroy (GtkWidget *widget)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;

  g_assert (IDE_IS_OMNI_BAR (self));

  g_clear_pointer (&self->looper_source, g_source_destroy);
  g_clear_object (&self->gesture);

  g_clear_object (&self->build_manager_bindings);
  g_clear_object (&self->build_manager_signals);
  g_clear_object (&self->config_manager_bindings);
  g_clear_object (&self->config_manager_signals);
  g_clear_object (&self->project_bindings);
  g_clear_object (&self->vcs_bindings);

  GTK_WIDGET_CLASS (ide_omni_bar_parent_class)->destroy (widget);
}

static void
ide_omni_bar_class_init (IdeOmniBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_omni_bar_constructed;

  widget_class->destroy = ide_omni_bar_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-bar.ui");
  gtk_widget_class_set_css_name (widget_class, "omnibar");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, branch_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, branch_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, build_button);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, build_button_shortcut);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, build_result_diagnostics_image);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, config_name_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, event_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, message_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, operations_list);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_branch_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_build_result_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_config_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_details_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_errors_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_last_build_time_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_project_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_runtime_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_warnings_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, project_label);
}

static void
ide_omni_bar_init (IdeOmniBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_container_set_reallocate_redraws (GTK_CONTAINER (self), TRUE);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  gtk_widget_set_direction (GTK_WIDGET (self->branch_box), GTK_TEXT_DIR_LTR);

  g_signal_connect_object (self->build_button,
                           "query-tooltip",
                           G_CALLBACK (ide_omni_bar__build_button__query_tooltip),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * IdeBuildManager bindings and signals.
   */

  self->build_manager_bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->build_manager_bindings,
                          "busy",
                          self->cancel_button,
                          "visible",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->build_manager_bindings,
                          "busy",
                          self->build_button,
                          "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  dzl_binding_group_bind (self->build_manager_bindings,
                          "has-diagnostics",
                          self->build_result_diagnostics_image,
                          "visible",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->build_manager_bindings, "error-count",
                          self->popover_errors_label, "label",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->build_manager_bindings, "warning-count",
                          self->popover_warnings_label, "label",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind_full (self->build_manager_bindings,
                               "last-build-time",
                               self->popover_last_build_time_label,
                               "label",
                               G_BINDING_SYNC_CREATE,
                               date_time_to_label,
                               NULL,
                               NULL,
                               NULL);

  self->build_manager_signals = dzl_signal_group_new (IDE_TYPE_BUILD_MANAGER);

  dzl_signal_group_connect_object (self->build_manager_signals,
                                   "build-started",
                                   G_CALLBACK (ide_omni_bar__build_manager__build_started),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->build_manager_signals,
                                   "build-failed",
                                   G_CALLBACK (ide_omni_bar__build_manager__build_failed),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->build_manager_signals,
                                   "build-finished",
                                   G_CALLBACK (ide_omni_bar__build_manager__build_finished),
                                   self,
                                   G_CONNECT_SWAPPED);

  /*
   * Project bindings.
   */

  self->project_bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->project_bindings, "name",
                          self->popover_project_label, "label",
                          G_BINDING_SYNC_CREATE);

  /*
   * IdeVcs bindings and signals.
   */

  self->vcs_bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->vcs_bindings,
                          "branch-name",
                          self->branch_label,
                          "label",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (self->vcs_bindings,
                          "branch-name",
                          self->popover_branch_label,
                          "label",
                          G_BINDING_SYNC_CREATE);

  /*
   * IdeConfigurationManager bindings and signals.
   */

  self->config_manager_bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->config_manager_bindings,
                          "current-display-name",
                          self->config_name_label,
                          "label",
                          G_BINDING_SYNC_CREATE);

  self->config_manager_signals = dzl_signal_group_new (IDE_TYPE_CONFIGURATION_MANAGER);

  dzl_signal_group_connect_object (self->config_manager_signals,
                                   "notify::current",
                                   G_CALLBACK (ide_omni_bar__config_manager__notify_current),
                                   self,
                                   G_CONNECT_SWAPPED);

  /*
   * Enable various events for state tracking.
   */

  gtk_widget_add_events (GTK_WIDGET (self->event_box), GDK_BUTTON_PRESS_MASK);

  g_signal_connect_object (self->event_box,
                           "enter-notify-event",
                           G_CALLBACK (event_box_enter_notify),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->event_box,
                           "leave-notify-event",
                           G_CALLBACK (event_box_leave_notify),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->popover,
                           "closed",
                           G_CALLBACK (ide_omni_bar_popover_closed),
                           self,
                           G_CONNECT_SWAPPED);

  self->gesture = gtk_gesture_multi_press_new (GTK_WIDGET (self->event_box));
  g_signal_connect (self->gesture, "pressed", G_CALLBACK (multipress_pressed_cb), self);

  /*
   * Register to be notified of IdeWorkbench:context set.
   */
  ide_widget_set_context_handler (self, ide_omni_bar_context_set);
}

GtkWidget *
ide_omni_bar_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_BAR, NULL);
}
