/* ide-debugger-perspective.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-perspective"

#include <glib/gi18n.h>

#include "ide-debugger-perspective.h"

struct _IdeDebuggerPerspective
{
  IdeLayout parent_instance;
};

enum {
  PROP_0,
  N_PROPS
};

static gchar *
ide_debugger_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Debugger"));
}

static gchar *
ide_debugger_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("debugger");
}

static gchar *
ide_debugger_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("builder-debugger-symbolic");
}

static gchar *
ide_debugger_perspective_get_accelerator (IdePerspective *perspective)
{
  return g_strdup ("<Alt>2");
}

static void
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_accelerator = ide_debugger_perspective_get_accelerator;
  iface->get_icon_name = ide_debugger_perspective_get_icon_name;
  iface->get_id = ide_debugger_perspective_get_id;
  iface->get_title = ide_debugger_perspective_get_title;
}

G_DEFINE_TYPE_EXTENDED (IdeDebuggerPerspective, ide_debugger_perspective, IDE_TYPE_LAYOUT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_perspective_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_debugger_perspective_parent_class)->finalize (object);
}

static void
ide_debugger_perspective_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeDebuggerPerspective *self = IDE_DEBUGGER_PERSPECTIVE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_perspective_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeDebuggerPerspective *self = IDE_DEBUGGER_PERSPECTIVE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_perspective_class_init (IdeDebuggerPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_debugger_perspective_finalize;
  object_class->get_property = ide_debugger_perspective_get_property;
  object_class->set_property = ide_debugger_perspective_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-debugger-perspective.ui");
}

static void
ide_debugger_perspective_init (IdeDebuggerPerspective *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
