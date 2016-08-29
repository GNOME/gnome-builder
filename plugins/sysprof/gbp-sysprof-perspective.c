/* gbp-sysprof-perspective.c
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

#define G_LOG_DOMAIN "gbp-sysprof-perspective"

#include <glib/gi18n.h>
#include <sysprof.h>
#include <sysprof-ui.h>

#include "gbp-sysprof-perspective.h"

struct _GbpSysprofPerspective
{
  GtkBin           parent_instance;

  SpCallgraphView *callgraph_view;
};

static void perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpSysprofPerspective, gbp_sysprof_perspective, GTK_TYPE_BIN, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

static void
gbp_sysprof_perspective_class_init (GbpSysprofPerspectiveClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/sysprof-plugin/gbp-sysprof-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpSysprofPerspective, callgraph_view);

  g_type_ensure (SP_TYPE_CALLGRAPH_VIEW);
}

static void
gbp_sysprof_perspective_init (GbpSysprofPerspective *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static gchar *
gbp_sysprof_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("utilities-system-monitor-symbolic");
}

static gchar *
gbp_sysprof_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Profiler"));
}

static gchar *
gbp_sysprof_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("profiler");
}

static gint
gbp_sysprof_perspective_get_priority (IdePerspective *perspective)
{
  return 70000;
}

static gchar *
gbp_sysprof_perspective_get_accelerator (IdePerspective *perspective)
{
  return g_strdup ("<Alt>3");
}

static void
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_icon_name = gbp_sysprof_perspective_get_icon_name;
  iface->get_title = gbp_sysprof_perspective_get_title;
  iface->get_id = gbp_sysprof_perspective_get_id;
  iface->get_priority = gbp_sysprof_perspective_get_priority;
  iface->get_accelerator = gbp_sysprof_perspective_get_accelerator;
}

static void
generate_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  SpCallgraphProfile *profile = (SpCallgraphProfile *)object;
  g_autoptr(GbpSysprofPerspective) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (SP_IS_CALLGRAPH_PROFILE (profile));
  g_assert (GBP_IS_SYSPROF_PERSPECTIVE (self));

  if (!sp_profile_generate_finish (SP_PROFILE (profile), result, &error))
    {
      g_warning ("Failed to generate profile: %s", error->message);
      return;
    }

  sp_callgraph_view_set_profile (self->callgraph_view, profile);
}

void
gbp_sysprof_perspective_set_reader (GbpSysprofPerspective *self,
                                    SpCaptureReader       *reader)
{
  g_autoptr(SpProfile) profile = NULL;

  g_assert (GBP_IS_SYSPROF_PERSPECTIVE (self));

  if (reader == NULL)
    {
      sp_callgraph_view_set_profile (self->callgraph_view, NULL);
      return;
    }

  profile = sp_callgraph_profile_new ();

  sp_profile_set_reader (profile, reader);

  sp_profile_generate (profile, NULL, generate_cb, g_object_ref (self));
}
