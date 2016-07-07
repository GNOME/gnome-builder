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
#include <egg-signal-group.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-build-result.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-configuration-manager.h"
#include "projects/ide-project.h"
#include "util/ide-gtk.h"
#include "vcs/ide-vcs.h"
#include "workbench/ide-omni-bar.h"
#include "workbench/ide-omni-bar-row.h"

#define LOOPER_INTERVAL_SECONDS 5

struct _IdeOmniBar
{
  GtkBox          parent_instance;

  EggSignalGroup *build_result_signals;
  GSource        *looper_source;
  GtkGesture     *gesture;

  guint           seen_count;

  GtkLabel       *branch_label;
  GtkEventBox    *event_box;
  GtkLabel       *project_label;
  GtkLabel       *build_result_mode_label;
  GtkImage       *build_result_diagnostics_image;
  GtkButton      *build_button;
  GtkImage       *build_button_image;
  GtkLabel       *config_name_label;
  GtkStack       *message_stack;
  GtkPopover     *popover;
  GtkLabel       *popover_branch_label;
  GtkButton      *popover_build_cancel_button;
  GtkLabel       *popover_build_mode_label;
  GtkLabel       *popover_build_running_time_label;
  GtkListBox     *popover_configuration_list_box;
  GtkLabel       *popover_failed_label;
  GtkLabel       *popover_last_build_time_label;
  GtkButton      *popover_view_output_button;
  GtkLabel       *popover_project_label;
};

G_DEFINE_TYPE (IdeOmniBar, ide_omni_bar, GTK_TYPE_BOX)

static void
on_configure_row (IdeOmniBar    *self,
                  IdeOmniBarRow *row)
{
  IdeConfiguration *config;
  const gchar *id;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_OMNI_BAR_ROW (row));

  config = ide_omni_bar_row_get_item (row);
  id = ide_configuration_get_id (config);

  /*
   * TODO: This can be removed once GtkPopover can activate actions
   *       that are resolved via the GtkPopover:relative-to property,
   *       or it gets a proper parent that is not the toplevel.
   *
   *       https://bugzilla.gnome.org/show_bug.cgi?id=768023
   */

  ide_widget_action (GTK_WIDGET (self),
                     "build-tools",
                     "configure",
                     g_variant_new_string (id));

  gtk_widget_hide (GTK_WIDGET (self->popover));
}

static GtkWidget *
create_configuration_row (gpointer item,
                          gpointer user_data)
{
  IdeConfiguration *configuration = item;
  IdeOmniBar *self = user_data;
  GtkWidget *ret;

  g_assert (IDE_IS_CONFIGURATION (configuration));
  g_assert (IDE_IS_OMNI_BAR (self));

  ret = g_object_new (IDE_TYPE_OMNI_BAR_ROW,
                      "item", configuration,
                      "visible", TRUE,
                      NULL);

  g_signal_connect_object (ret,
                           "configure",
                           G_CALLBACK (on_configure_row),
                           self,
                           G_CONNECT_SWAPPED);

  return ret;
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
ide_omni_bar_select_current_config (GtkWidget *widget,
                                    gpointer   user_data)
{
  IdeConfiguration *current = user_data;
  IdeOmniBarRow *row = (IdeOmniBarRow *)widget;

  g_assert (IDE_IS_OMNI_BAR_ROW (row));
  g_assert (IDE_IS_CONFIGURATION (current));

  ide_omni_bar_row_set_active (row, (current == ide_omni_bar_row_get_item (row)));
}

static void
ide_omni_bar_current_changed (IdeOmniBar              *self,
                              GParamSpec              *pspec,
                              IdeConfigurationManager *config_manager)
{
  IdeConfiguration *current;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_CONFIGURATION_MANAGER (config_manager));

  current = ide_configuration_manager_get_current (config_manager);

  gtk_container_foreach (GTK_CONTAINER (self->popover_configuration_list_box),
                         ide_omni_bar_select_current_config,
                         current);
}

static void
ide_omni_bar_row_activated (IdeOmniBar    *self,
                            IdeOmniBarRow *row,
                            GtkListBox    *list_box)
{
  IdeConfiguration *config;
  IdeConfigurationManager *config_manager;
  IdeContext *context;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_OMNI_BAR_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  context = ide_widget_get_context (GTK_WIDGET (self));
  config_manager = ide_context_get_configuration_manager (context);
  config = ide_omni_bar_row_get_item (row);

  ide_configuration_manager_set_current (config_manager, config);
}

static gboolean
add_target_prefix_transform (GBinding     *binding,
                             const GValue *from_value,
                             GValue       *to_value,
                             gpointer      user_data)
{
  g_assert (G_IS_BINDING (binding));
  g_assert (from_value != NULL);
  g_assert (G_VALUE_HOLDS_STRING (from_value));
  g_assert (to_value != NULL);

  g_value_take_string (to_value,
                       g_strdup_printf ("%s: %s",
                                        /* Translators: "Target" is providing context to the selected build configuration */
                                        _("Target"),
                                        g_value_get_string (from_value)));

  return TRUE;
}

static void
ide_omni_bar_context_set (GtkWidget  *widget,
                          IdeContext *context)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;

  IDE_ENTRY;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  ide_omni_bar_update (self);

  if (context != NULL)
    {
      IdeConfigurationManager *configs;
      g_autofree gchar *path = NULL;
      g_autoptr(GFile) home = NULL;
      GFile *workdir;
      IdeVcs *vcs;

      configs = ide_context_get_configuration_manager (context);
      vcs = ide_context_get_vcs (context);
      workdir = ide_vcs_get_working_directory (vcs);
      home = g_file_new_for_path (g_get_home_dir ());

      if (g_file_has_prefix (workdir, home))
        path = g_file_get_relative_path (home, workdir);
      else if (g_file_is_native (workdir))
        path = g_file_get_path (workdir);
      else
        path = g_file_get_uri (workdir);

      gtk_label_set_label (self->popover_project_label, path);

      g_signal_connect_object (vcs,
                               "changed",
                               G_CALLBACK (ide_omni_bar_update),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_bind_property_full (configs, "current-display-name",
                                   self->config_name_label, "label",
                                   G_BINDING_SYNC_CREATE,
                                   add_target_prefix_transform,
                                   NULL, NULL, NULL);

      gtk_list_box_bind_model (self->popover_configuration_list_box,
                               G_LIST_MODEL (configs),
                               create_configuration_row,
                               self,
                               NULL);

      g_signal_connect_object (configs,
                               "notify::current",
                               G_CALLBACK (ide_omni_bar_current_changed),
                               self,
                               G_CONNECT_SWAPPED);

      ide_omni_bar_current_changed (self, NULL, configs);
    }

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
ide_omni_bar_build_result_notify_mode (IdeOmniBar     *self,
                                       GParamSpec     *pspec,
                                       IdeBuildResult *result)
{
  g_autofree gchar *mode = NULL;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_BUILD_RESULT (result));

  mode = ide_build_result_get_mode (result);

  gtk_label_set_label (self->build_result_mode_label, mode);

  if (ide_build_result_get_running (result))
    gtk_label_set_label (self->popover_build_mode_label, mode);
  else
    gtk_label_set_label (self->popover_build_mode_label, _("Last Build"));
}

static void
ide_omni_bar_build_result_notify_failed (IdeOmniBar     *self,
                                         GParamSpec     *pspec,
                                         IdeBuildResult *result)
{
  gboolean failed;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_BUILD_RESULT (result));

  failed = ide_build_result_get_failed (result);

  gtk_widget_set_visible (GTK_WIDGET (self->popover_failed_label), failed);
}

static void
ide_omni_bar_build_result_notify_running_time (IdeOmniBar     *self,
                                               GParamSpec     *pspec,
                                               IdeBuildResult *result)
{
  g_autofree gchar *text = NULL;
  GTimeSpan span;
  guint hours;
  guint minutes;
  guint seconds;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_BUILD_RESULT (result));

  span = ide_build_result_get_running_time (result);

  hours = span / G_TIME_SPAN_HOUR;
  minutes = (span % G_TIME_SPAN_HOUR) / G_TIME_SPAN_MINUTE;
  seconds = (span % G_TIME_SPAN_MINUTE) / G_TIME_SPAN_SECOND;

  text = g_strdup_printf ("%02u:%02u:%02u", hours, minutes, seconds);
  gtk_label_set_label (self->popover_build_running_time_label, text);
}

static void
ide_omni_bar_build_result_notify_running (IdeOmniBar     *self,
                                          GParamSpec     *pspec,
                                          IdeBuildResult *result)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_BUILD_RESULT (result));

  if (ide_build_result_get_running (result))
    {
      g_object_set (self->build_button_image,
                    "icon-name", "process-stop-symbolic",
                    NULL);
      g_object_set (self->build_button,
                    "action-name", "build-tools.cancel-build",
                    NULL);

      gtk_stack_set_visible_child (self->message_stack,
                                   GTK_WIDGET (self->build_result_mode_label));

      gtk_widget_hide (GTK_WIDGET (self->popover_last_build_time_label));

      gtk_widget_show (GTK_WIDGET (self->popover_build_cancel_button));
      gtk_widget_show (GTK_WIDGET (self->popover_build_mode_label));
      gtk_widget_show (GTK_WIDGET (self->popover_build_running_time_label));
    }
  else
    {
      g_object_set (self->build_button_image,
                    "icon-name", "system-run-symbolic",
                    NULL);
      g_object_set (self->build_button,
                    "action-name", "build-tools.build",
                    NULL);

      gtk_label_set_label (self->popover_build_mode_label, _("Last Build"));

      gtk_widget_hide (GTK_WIDGET (self->popover_build_cancel_button));
      gtk_widget_hide (GTK_WIDGET (self->popover_build_running_time_label));

      gtk_widget_show (GTK_WIDGET (self->popover_build_mode_label));
      gtk_widget_show (GTK_WIDGET (self->popover_last_build_time_label));
    }
}

static void
ide_omni_bar_build_result_diagnostic (IdeOmniBar     *self,
                                      IdeDiagnostic  *diagnostic,
                                      IdeBuildResult *result)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_BUILD_RESULT (result));

  gtk_widget_show (GTK_WIDGET (self->build_result_diagnostics_image));
}

static void
ide_omni_bar_next_message (IdeOmniBar *self)
{
  IdeBuildResult *build_result;
  const gchar *name;

  g_assert (IDE_IS_OMNI_BAR (self));

  build_result = ide_omni_bar_get_build_result (self);
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
      if (build_result != NULL && self->seen_count < 2)
        gtk_stack_set_visible_child_name (self->message_stack, "build");
    }
  else if (!ide_build_result_get_running (build_result))
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

  gtk_widget_show (GTK_WIDGET (self->popover));

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
ide_omni_bar_finalize (GObject *object)
{
  IdeOmniBar *self = (IdeOmniBar *)object;

  g_clear_object (&self->build_result_signals);

  G_OBJECT_CLASS (ide_omni_bar_parent_class)->finalize (object);
}

static void
ide_omni_bar_destroy (GtkWidget *widget)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;

  g_assert (IDE_IS_OMNI_BAR (self));

  g_clear_pointer (&self->looper_source, g_source_destroy);
  g_clear_object (&self->gesture);

  GTK_WIDGET_CLASS (ide_omni_bar_parent_class)->destroy (widget);
}

static void
ide_omni_bar_class_init (IdeOmniBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_omni_bar_constructed;
  object_class->finalize = ide_omni_bar_finalize;

  widget_class->destroy = ide_omni_bar_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-bar.ui");
  gtk_widget_class_set_css_name (widget_class, "omnibar");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, branch_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, build_button);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, build_button_image);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, build_result_diagnostics_image);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, build_result_mode_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, config_name_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, event_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, message_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_branch_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_build_cancel_button);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_build_mode_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_build_running_time_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_configuration_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_failed_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_last_build_time_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_project_label);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover_view_output_button);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, project_label);
}

static void
ide_omni_bar_init (IdeOmniBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

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

  self->build_result_signals = egg_signal_group_new (IDE_TYPE_BUILD_RESULT);

  egg_signal_group_connect_object (self->build_result_signals,
                                   "notify::failed",
                                   G_CALLBACK (ide_omni_bar_build_result_notify_failed),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->build_result_signals,
                                   "notify::mode",
                                   G_CALLBACK (ide_omni_bar_build_result_notify_mode),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->build_result_signals,
                                   "notify::running",
                                   G_CALLBACK (ide_omni_bar_build_result_notify_running),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->build_result_signals,
                                   "notify::running-time",
                                   G_CALLBACK (ide_omni_bar_build_result_notify_running_time),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->build_result_signals,
                                   "diagnostic",
                                   G_CALLBACK (ide_omni_bar_build_result_diagnostic),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_connect_object (self->popover_configuration_list_box,
                           "row-activated",
                           G_CALLBACK (ide_omni_bar_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  ide_widget_set_context_handler (self, ide_omni_bar_context_set);
}

GtkWidget *
ide_omni_bar_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_BAR, NULL);
}

/**
 * ide_omni_bar_get_build_result:
 *
 * Gets the current build result that is being visualized in the omni bar.
 *
 * Returns: (nullable) (transfer none): An #IdeBuildResult or %NULL.
 */
IdeBuildResult *
ide_omni_bar_get_build_result (IdeOmniBar *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_BAR (self), NULL);

  return egg_signal_group_get_target (self->build_result_signals);
}

void
ide_omni_bar_set_build_result (IdeOmniBar     *self,
                               IdeBuildResult *build_result)
{
  g_autoptr(GDateTime) now = NULL;
  g_autofree gchar *nowstr = NULL;
  gboolean failed = FALSE;

  g_return_if_fail (IDE_IS_OMNI_BAR (self));
  g_return_if_fail (!build_result || IDE_IS_BUILD_RESULT (build_result));

  gtk_widget_hide (GTK_WIDGET (self->build_result_diagnostics_image));
  egg_signal_group_set_target (self->build_result_signals, build_result);

  self->seen_count = 0;

  gtk_stack_set_visible_child_name (self->message_stack, "build");

  now = g_date_time_new_now_local ();
  nowstr = g_date_time_format (now, "%a %B %e, %X");
  gtk_label_set_label (self->popover_last_build_time_label, nowstr);

  gtk_widget_show (GTK_WIDGET (self->popover_view_output_button));

  if (build_result)
    failed = ide_build_result_get_failed (build_result);
  gtk_widget_set_visible (GTK_WIDGET (self->popover_failed_label), failed);
}
