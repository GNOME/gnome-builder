/* ide-environment-editor.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "ide-environment-editor.h"
#include "ide-environment-editor-row.h"

struct _IdeEnvironmentEditor
{
  GtkListBox      parent_instance;
  IdeEnvironment *environment;
  GtkWidget      *dummy_row;

  IdeEnvironmentVariable *dummy;
};

G_DEFINE_TYPE (IdeEnvironmentEditor, ide_environment_editor, GTK_TYPE_LIST_BOX)

enum {
  PROP_0,
  PROP_ENVIRONMENT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_environment_editor_delete_row (IdeEnvironmentEditor    *self,
                                   IdeEnvironmentEditorRow *row)
{
  IdeEnvironmentVariable *variable;

  g_assert (IDE_IS_ENVIRONMENT_EDITOR (self));
  g_assert (IDE_IS_ENVIRONMENT_EDITOR_ROW (row));

  variable = ide_environment_editor_row_get_variable (row);
  ide_environment_remove (self->environment, variable);
}

static GtkWidget *
ide_environment_editor_create_dummy_row (IdeEnvironmentEditor *self)
{
  GtkWidget *row;
  GtkWidget *label;

  g_assert (IDE_IS_ENVIRONMENT_EDITOR (self));

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("New variable…"),
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "child", label,
                      "visible", TRUE,
                      NULL);

  return row;
}

static GtkWidget *
ide_environment_editor_create_row (gpointer item,
                                   gpointer user_data)
{
  IdeEnvironmentVariable *variable = item;
  IdeEnvironmentEditor *self = user_data;
  IdeEnvironmentEditorRow *row;

  g_assert (IDE_IS_ENVIRONMENT_EDITOR (self));
  g_assert (IDE_IS_ENVIRONMENT_VARIABLE (variable));

  row = g_object_new (IDE_TYPE_ENVIRONMENT_EDITOR_ROW,
                      "variable", variable,
                      "visible", TRUE,
                      NULL);

  g_signal_connect_object (row,
                           "delete",
                           G_CALLBACK (ide_environment_editor_delete_row),
                           self,
                           G_CONNECT_SWAPPED);

  return GTK_WIDGET (row);
}

static void
ide_environment_editor_disconnect (IdeEnvironmentEditor *self)
{
  g_assert (IDE_IS_ENVIRONMENT_EDITOR (self));
  g_assert (IDE_IS_ENVIRONMENT (self->environment));

  gtk_list_box_bind_model (GTK_LIST_BOX (self), NULL, NULL, NULL, NULL);

  g_clear_object (&self->dummy);
}

static void
ide_environment_editor_connect (IdeEnvironmentEditor *self)
{
  g_assert (IDE_IS_ENVIRONMENT_EDITOR (self));
  g_assert (IDE_IS_ENVIRONMENT (self->environment));

  gtk_list_box_bind_model (GTK_LIST_BOX (self),
                           G_LIST_MODEL (self->environment),
                           ide_environment_editor_create_row, self, NULL);

  self->dummy_row = ide_environment_editor_create_dummy_row (self);
  gtk_container_add (GTK_CONTAINER (self), self->dummy_row);
}

static void
find_row_cb (GtkWidget *widget,
             gpointer   data)
{
  struct {
    IdeEnvironmentVariable  *variable;
    IdeEnvironmentEditorRow *row;
  } *lookup = data;

  g_assert (lookup != NULL);
  g_assert (GTK_IS_LIST_BOX_ROW (widget));

  if (IDE_IS_ENVIRONMENT_EDITOR_ROW (widget))
    {
      IdeEnvironmentVariable *variable;

      variable = ide_environment_editor_row_get_variable (IDE_ENVIRONMENT_EDITOR_ROW (widget));

      if (variable == lookup->variable)
        lookup->row = IDE_ENVIRONMENT_EDITOR_ROW (widget);
    }
}

static IdeEnvironmentEditorRow *
find_row (IdeEnvironmentEditor   *self,
          IdeEnvironmentVariable *variable)
{
  struct {
    IdeEnvironmentVariable  *variable;
    IdeEnvironmentEditorRow *row;
  } lookup = { variable, NULL };

  g_assert (IDE_IS_ENVIRONMENT_EDITOR (self));
  g_assert (IDE_IS_ENVIRONMENT_VARIABLE (variable));

  gtk_container_foreach (GTK_CONTAINER (self), find_row_cb, &lookup);

  return lookup.row;
}

static void
ide_environment_editor_row_activated (GtkListBox    *list_box,
                                      GtkListBoxRow *row)
{
  IdeEnvironmentEditor *self = (IdeEnvironmentEditor *)list_box;

  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  if (self->environment == NULL)
    return;

  if (self->dummy_row == GTK_WIDGET (row))
    {
      g_autoptr(IdeEnvironmentVariable) variable = NULL;

      variable = ide_environment_variable_new (NULL, NULL);
      ide_environment_append (self->environment, variable);
      ide_environment_editor_row_start_editing (find_row (self, variable));
    }
}

static void
ide_environment_editor_destroy (GtkWidget *widget)
{
  IdeEnvironmentEditor *self = (IdeEnvironmentEditor *)widget;

  GTK_WIDGET_CLASS (ide_environment_editor_parent_class)->destroy (widget);

  g_clear_object (&self->environment);
}

static void
ide_environment_editor_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeEnvironmentEditor *self = IDE_ENVIRONMENT_EDITOR(object);

  switch (prop_id)
    {
    case PROP_ENVIRONMENT:
      g_value_set_object (value, ide_environment_editor_get_environment (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_environment_editor_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeEnvironmentEditor *self = IDE_ENVIRONMENT_EDITOR(object);

  switch (prop_id)
    {
    case PROP_ENVIRONMENT:
      ide_environment_editor_set_environment (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_environment_editor_class_init (IdeEnvironmentEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkListBoxClass *list_box_class = GTK_LIST_BOX_CLASS (klass);

  object_class->get_property = ide_environment_editor_get_property;
  object_class->set_property = ide_environment_editor_set_property;

  widget_class->destroy = ide_environment_editor_destroy;

  list_box_class->row_activated = ide_environment_editor_row_activated;

  properties [PROP_ENVIRONMENT] =
    g_param_spec_object ("environment",
                         "Environment",
                         "Environment",
                         IDE_TYPE_ENVIRONMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_environment_editor_init (IdeEnvironmentEditor *self)
{
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self), GTK_SELECTION_NONE);
}

GtkWidget *
ide_environment_editor_new (void)
{
  return g_object_new (IDE_TYPE_ENVIRONMENT_EDITOR, NULL);
}

void
ide_environment_editor_set_environment (IdeEnvironmentEditor *self,
                                        IdeEnvironment       *environment)
{
  g_return_if_fail (IDE_IS_ENVIRONMENT_EDITOR (self));
  g_return_if_fail (IDE_IS_ENVIRONMENT (environment));

  if (self->environment != environment)
    {
      if (self->environment != NULL)
        {
          ide_environment_editor_disconnect (self);
          g_clear_object (&self->environment);
        }

      if (environment != NULL)
        {
          self->environment = g_object_ref (environment);
          ide_environment_editor_connect (self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENVIRONMENT]);
    }
}

/**
 * ide_environment_editor_get_environment:
 *
 * Returns: (nullable) (transfer none): An #IdeEnvironment or %NULL.
 */
IdeEnvironment *
ide_environment_editor_get_environment (IdeEnvironmentEditor *self)
{
  g_return_val_if_fail (IDE_IS_ENVIRONMENT_EDITOR (self), NULL);

  return self->environment;
}
