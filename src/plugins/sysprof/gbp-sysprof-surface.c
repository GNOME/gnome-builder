/* gbp-sysprof-surface.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-sysprof-surface"

#include <glib/gi18n.h>
#include <sysprof.h>
#include <sysprof-ui.h>

#include "gbp-sysprof-surface.h"

struct _GbpSysprofSurface
{
  IdeSurface            parent_instance;

  SpCaptureReader      *reader;

  GtkStack             *stack;
  SpCallgraphView      *callgraph_view;
  GtkLabel             *info_bar_label;
  GtkButton            *info_bar_close;
  GtkRevealer          *info_bar_revealer;
  SpVisualizerView     *visualizers;
  SpRecordingStateView *recording_view;
  SpZoomManager        *zoom_manager;
};

static void gbp_sysprof_surface_reload (GbpSysprofSurface *self);

G_DEFINE_TYPE (GbpSysprofSurface, gbp_sysprof_surface, IDE_TYPE_SURFACE)

static void
hide_info_bar (GbpSysprofSurface *self,
               GtkButton             *button)
{
  g_assert (GBP_IS_SYSPROF_SURFACE (self));

  gtk_revealer_set_reveal_child (self->info_bar_revealer, FALSE);
}

static void
gbp_sysprof_surface_selection_changed (GbpSysprofSurface *self,
                                           SpSelection           *selection)
{
  g_assert (GBP_IS_SYSPROF_SURFACE (self));
  g_assert (SP_IS_SELECTION (selection));

  gbp_sysprof_surface_reload (self);
}

static void
gbp_sysprof_surface_finalize (GObject *object)
{
  GbpSysprofSurface *self = (GbpSysprofSurface *)object;

  g_clear_pointer (&self->reader, sp_capture_reader_unref);

  G_OBJECT_CLASS (gbp_sysprof_surface_parent_class)->finalize (object);
}

static void
gbp_sysprof_surface_class_init (GbpSysprofSurfaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_sysprof_surface_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/sysprof/gbp-sysprof-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, callgraph_view);
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, info_bar_label);
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, info_bar_close);
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, info_bar_revealer);
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, stack);
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, recording_view);
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, visualizers);
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, zoom_manager);

  g_type_ensure (SP_TYPE_CALLGRAPH_VIEW);
  g_type_ensure (SP_TYPE_CPU_VISUALIZER_ROW);
  g_type_ensure (SP_TYPE_EMPTY_STATE_VIEW);
  g_type_ensure (SP_TYPE_FAILED_STATE_VIEW);
  g_type_ensure (SP_TYPE_RECORDING_STATE_VIEW);
  g_type_ensure (SP_TYPE_VISUALIZER_VIEW);
}

static void
gbp_sysprof_surface_init (GbpSysprofSurface *self)
{
  DzlShortcutController *controller;
  SpSelection *selection;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_name (GTK_WIDGET (self), "profiler");
  ide_surface_set_icon_name (IDE_SURFACE (self), "utilities-system-monitor-symbolic");
  ide_surface_set_title (IDE_SURFACE (self), _("Profiler"));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));
  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.sysprof.focus",
                                              "<alt>2",
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              "win.surface('profiler')");

  g_signal_connect_object (self->info_bar_close,
                           "clicked",
                           G_CALLBACK (hide_info_bar),
                           self,
                           G_CONNECT_SWAPPED);

  selection = sp_visualizer_view_get_selection (self->visualizers);

  g_signal_connect_object (selection,
                           "changed",
                           G_CALLBACK (gbp_sysprof_surface_selection_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
generate_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  SpCallgraphProfile *profile = (SpCallgraphProfile *)object;
  g_autoptr(GbpSysprofSurface) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (SP_IS_CALLGRAPH_PROFILE (profile));
  g_assert (GBP_IS_SYSPROF_SURFACE (self));

  if (!sp_profile_generate_finish (SP_PROFILE (profile), result, &error))
    {
      g_warning ("Failed to generate profile: %s", error->message);
      return;
    }

  sp_callgraph_view_set_profile (self->callgraph_view, profile);
}

static void
gbp_sysprof_surface_reload (GbpSysprofSurface *self)
{
  SpSelection *selection;
  g_autoptr(SpProfile) profile = NULL;

  g_assert (GBP_IS_SYSPROF_SURFACE (self));

  if (self->reader == NULL)
    return;

  /* If we failed, ignore the (probably mostly empty) reader */
  if (g_strcmp0 (gtk_stack_get_visible_child_name (self->stack), "failed") == 0)
    return;

  selection = sp_visualizer_view_get_selection (self->visualizers);
  profile = sp_callgraph_profile_new_with_selection (selection);

  sp_profile_set_reader (profile, self->reader);
  sp_profile_generate (profile, NULL, generate_cb, g_object_ref (self));

  sp_visualizer_view_set_reader (self->visualizers, self->reader);

  gtk_stack_set_visible_child_name (self->stack, "results");
}

SpCaptureReader *
gbp_sysprof_surface_get_reader (GbpSysprofSurface *self)
{
  g_return_val_if_fail (GBP_IS_SYSPROF_SURFACE (self), NULL);

  return sp_visualizer_view_get_reader (self->visualizers);
}

void
gbp_sysprof_surface_set_reader (GbpSysprofSurface *self,
                                    SpCaptureReader       *reader)
{
  g_assert (GBP_IS_SYSPROF_SURFACE (self));

  if (reader != self->reader)
    {
      SpSelection *selection;

      if (self->reader != NULL)
        {
          g_clear_pointer (&self->reader, sp_capture_reader_unref);
          sp_callgraph_view_set_profile (self->callgraph_view, NULL);
          sp_visualizer_view_set_reader (self->visualizers, NULL);
          gtk_stack_set_visible_child_name (self->stack, "empty");
        }

      selection = sp_visualizer_view_get_selection (self->visualizers);
      sp_selection_unselect_all (selection);

      if (reader != NULL)
        {
          self->reader = sp_capture_reader_ref (reader);
          gbp_sysprof_surface_reload (self);
        }
    }
}

static void
gbp_sysprof_surface_profiler_failed (GbpSysprofSurface *self,
                                         const GError          *error,
                                         SpProfiler            *profiler)
{
  IDE_ENTRY;

  g_assert (GBP_IS_SYSPROF_SURFACE (self));
  g_assert (error != NULL);
  g_assert (SP_IS_PROFILER (profiler));

  gtk_stack_set_visible_child_name (self->stack, "failed");

  gtk_label_set_label (self->info_bar_label, error->message);
  gtk_revealer_set_reveal_child (self->info_bar_revealer, TRUE);

  IDE_EXIT;
}

void
gbp_sysprof_surface_set_profiler (GbpSysprofSurface *self,
                                      SpProfiler            *profiler)
{
  g_return_if_fail (GBP_IS_SYSPROF_SURFACE (self));
  g_return_if_fail (!profiler || SP_IS_PROFILER (profiler));

  sp_recording_state_view_set_profiler (self->recording_view, profiler);

  if (profiler != NULL)
    {
      gtk_stack_set_visible_child_name (self->stack, "recording");

      g_signal_connect_object (profiler,
                               "failed",
                               G_CALLBACK (gbp_sysprof_surface_profiler_failed),
                               self,
                               G_CONNECT_SWAPPED);
    }
  else
    {
      gtk_stack_set_visible_child_name (self->stack, "empty");
    }
}

SpZoomManager *
gbp_sysprof_surface_get_zoom_manager (GbpSysprofSurface *self)
{
  g_return_val_if_fail (GBP_IS_SYSPROF_SURFACE (self), NULL);

  return self->zoom_manager;
}
