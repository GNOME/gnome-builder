/* ide-editor-perspective.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-perspective"

#include <glib/gi18n.h>

#include "ide-editor-perspective.h"
#include "ide-workbench-header-bar.h"

struct _IdeEditorPerspective
{
  IdeLayout              parent_instance;

  IdeWorkbenchHeaderBar *titlebar;
};

static void ide_perspective_iface_init (IdePerspectiveInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeEditorPerspective, ide_editor_perspective, IDE_TYPE_LAYOUT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, ide_perspective_iface_init))

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_editor_perspective_restore_panel_state (IdeEditorPerspective *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkWidget *pane;
  gboolean reveal;
  guint position;

  g_assert (IDE_IS_EDITOR_PERSPECTIVE (self));

  settings = g_settings_new ("org.gnome.builder.workbench");

  pane = ide_layout_get_left_pane (IDE_LAYOUT (self));
  reveal = g_settings_get_boolean (settings, "left-visible");
  position = g_settings_get_int (settings, "left-position");
  gtk_container_child_set (GTK_CONTAINER (self), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);

  pane = ide_layout_get_right_pane (IDE_LAYOUT (self));
  reveal = g_settings_get_boolean (settings, "right-visible");
  position = g_settings_get_int (settings, "right-position");
  gtk_container_child_set (GTK_CONTAINER (self), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);

  pane = ide_layout_get_bottom_pane (IDE_LAYOUT (self));
  reveal = g_settings_get_boolean (settings, "bottom-visible");
  position = g_settings_get_int (settings, "bottom-position");
  gtk_container_child_set (GTK_CONTAINER (self), pane,
                           "position", position,
                           "reveal", reveal,
                           NULL);
}

static void
ide_editor_perspective_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_editor_perspective_parent_class)->finalize (object);
}

static void
ide_editor_perspective_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_perspective_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_perspective_class_init (IdeEditorPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_editor_perspective_finalize;
  object_class->get_property = ide_editor_perspective_get_property;
  object_class->set_property = ide_editor_perspective_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-perspective.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPerspective, titlebar);
}

static void
ide_editor_perspective_init (IdeEditorPerspective *self)
{
  GActionGroup *actions;

  gtk_widget_init_template (GTK_WIDGET (self));

  actions = gtk_widget_get_action_group (GTK_WIDGET (self), "panels");
  gtk_widget_insert_action_group (GTK_WIDGET (self->titlebar), "panels", actions);

  ide_editor_perspective_restore_panel_state (self);
}

static gchar *
ide_editor_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Editor"));
}

static GtkWidget *
ide_editor_perspective_get_titlebar (IdePerspective *perspective)
{
  return GTK_WIDGET (IDE_EDITOR_PERSPECTIVE (perspective)->titlebar);
}

static gchar *
ide_editor_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("text-editor-symbolic");
}

static gchar *
ide_editor_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("editor");
}

static void
ide_perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_id = ide_editor_perspective_get_id;
  iface->get_title = ide_editor_perspective_get_title;
  iface->get_titlebar = ide_editor_perspective_get_titlebar;
  iface->get_icon_name = ide_editor_perspective_get_icon_name;
}
