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
#include <sysprof-ui.h>

#include "gbp-sysprof-surface.h"

struct _GbpSysprofSurface
{
  IdeSurface       parent_instance;
  SysprofNotebook *notebook;
};

G_DEFINE_TYPE (GbpSysprofSurface, gbp_sysprof_surface, IDE_TYPE_SURFACE)

static void
gbp_sysprof_surface_class_init (GbpSysprofSurfaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/sysprof/gbp-sysprof-surface.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofSurface, notebook);

  g_type_ensure (SYSPROF_TYPE_NOTEBOOK);
}

static void
gbp_sysprof_surface_init (GbpSysprofSurface *self)
{
  DzlShortcutController *controller;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_name (GTK_WIDGET (self), "profiler");
  ide_surface_set_icon_name (IDE_SURFACE (self), "org.gnome.Sysprof-symbolic");
  ide_surface_set_title (IDE_SURFACE (self), _("Profiler"));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));
  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.sysprof.focus",
                                              "<alt>2",
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              "win.surface('profiler')");
}

void
gbp_sysprof_surface_open (GbpSysprofSurface *self,
                          GFile             *file)
{
  g_assert (GBP_IS_SYSPROF_SURFACE (self));
  g_assert (G_IS_FILE (file));

  sysprof_notebook_open (self->notebook, file);
}

void
gbp_sysprof_surface_add_profiler (GbpSysprofSurface *self,
                                  SysprofProfiler   *profiler)
{
  g_return_if_fail (GBP_IS_SYSPROF_SURFACE (self));
  g_return_if_fail (SYSPROF_IS_PROFILER (profiler));

  sysprof_notebook_add_profiler (self->notebook, profiler);
}
