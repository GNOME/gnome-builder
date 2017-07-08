/* ide-editor-properties.c
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

#define G_LOG_DOMAIN "ide-editor-properties"

#include <dazzle.h>

#include "editor/ide-editor-properties.h"

/**
 * SECTION:ide-editor-properties
 * @title: IdeEditorProperties
 * @short_description: property editor for IdeEditorView
 *
 * This widget is a property editor to tweak settings of an #IdeEditorView.
 * It should be used in a transient panel when the user needs to tweak the
 * settings of a view.
 *
 * Since: 3.26
 */

struct _IdeEditorProperties
{
  GtkBin    parent_instance;

  GtkCheckButton *show_line_numbers;
  GtkCheckButton *show_right_margin;
  GtkCheckButton *highlight_current_line;
  GtkCheckButton *insert_trailing_newline;
  GtkCheckButton *overwrite_braces;
  GtkCheckButton *auto_indent;
  GtkCheckButton *smart_backspace;
};

enum {
  PROP_0,
  PROP_VIEW,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorProperties, ide_editor_properties, GTK_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_properties_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeEditorProperties *self = IDE_EDITOR_PROPERTIES (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      ide_editor_properties_set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_properties_class_init (IdeEditorPropertiesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ide_editor_properties_set_property;

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The editor view to modify",
                         IDE_TYPE_EDITOR_VIEW,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/ide-editor-properties.ui");

  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, show_line_numbers);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, show_right_margin);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, highlight_current_line);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, insert_trailing_newline);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, overwrite_braces);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, auto_indent);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorProperties, smart_backspace);

  gtk_widget_class_set_css_name (widget_class, "ideeditorproperties");
}

static void
ide_editor_properties_init (IdeEditorProperties *self)
{
  GtkTextDirection dir;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Swap direction so check is at opposite end of checkbutton */
  dir = gtk_widget_get_direction (GTK_WIDGET (self));
  dir = (dir != GTK_TEXT_DIR_RTL) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;
  gtk_widget_set_direction (GTK_WIDGET (self->show_line_numbers), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->show_right_margin), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->highlight_current_line), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->insert_trailing_newline), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->overwrite_braces), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->auto_indent), dir);
  gtk_widget_set_direction (GTK_WIDGET (self->smart_backspace), dir);
}

/**
 * ide_editor_properties_new:
 *
 * Creates a new #IdeEditorProperties.
 *
 * Returns: (transfer full): A #IdeEditorProperties
 *
 * Since: 3.26
 */
GtkWidget *
ide_editor_properties_new (void)
{
  return g_object_new (IDE_TYPE_EDITOR_PROPERTIES, NULL);
}

/**
 * ide_editor_properties_set_view:
 * @self: an #IdeEditorProperties
 * @view: (nullable): an #IdeEditorView
 *
 * Sets the view to be edited by the property editor.
 *
 * Since: 3.26
 */
void
ide_editor_properties_set_view (IdeEditorProperties *self,
                                IdeEditorView       *view)
{
  g_return_if_fail (IDE_IS_EDITOR_PROPERTIES (self));
  g_return_if_fail (!view || IDE_IS_EDITOR_VIEW (view));

  dzl_gtk_widget_mux_action_groups (GTK_WIDGET (self),
                                    view ? GTK_WIDGET (view) : NULL,
                                    "IDE_EDITOR_PROPERTY_ACTIONS");
}
