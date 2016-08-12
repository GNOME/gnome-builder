/* gbp-gobject-dialog.c
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

#define G_LOG_DOMAIN "gbp-gobject-dialog"

#include <glib/gi18n.h>
#include <ide.h>

#include "gbp-gobject-dialog.h"
#include "gbp-gobject-property.h"
#include "gbp-gobject-property-editor.h"
#include "gbp-gobject-signal.h"
#include "gbp-gobject-signal-editor.h"
#include "gbp-gobject-spec.h"
#include "gbp-gobject-spec-editor.h"

struct _GbpGobjectDialog
{
  GtkAssistant              parent_instance;

  GbpGobjectSpec           *spec;
  GSimpleActionGroup       *actions;

  GbpGobjectSpecEditor     *editor;
  GtkWidget                *editor_page;
  GbpGobjectPropertyEditor *property_editor;
  GtkListBox               *properties_list_box;
  GtkStack                 *property_stack;
};

G_DEFINE_TYPE (GbpGobjectDialog, gbp_gobject_dialog, GTK_TYPE_ASSISTANT)

enum {
  PROP_0,
  PROP_DIRECTORY,
  PROP_SPEC,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GtkWidget *
gbp_gobject_dialog_new (void)
{
  return g_object_new (GBP_TYPE_GOBJECT_DIALOG, NULL);
}

static void
spec_notify_ready (GbpGobjectDialog *self,
                   GParamSpec       *pspec,
                   GbpGobjectSpec   *spec)
{
  gboolean complete;

  g_assert (GBP_IS_GOBJECT_DIALOG (self));
  g_assert (pspec != NULL);
  g_assert (GBP_IS_GOBJECT_SPEC (spec));

  complete = gbp_gobject_spec_get_ready (spec);

  gtk_container_child_set (GTK_CONTAINER (self), self->editor_page,
                           "complete", complete,
                           NULL);
}

static gboolean
name_to_label (GBinding     *binding,
               const GValue *from_value,
               GValue       *to_value,
               gpointer      user_data)
{
  const gchar *str;

  g_assert (G_IS_BINDING (binding));
  g_assert (from_value != NULL);

  str = g_value_get_string (from_value);

  if (str == NULL)
    g_value_set_static_string (to_value, _("New Property"));
  else
    g_value_set_string (to_value, str);

  return TRUE;
}

static GtkWidget *
create_property_row (gpointer item,
                     gpointer user_data)
{
  GbpGobjectDialog *self = user_data;
  GbpGobjectProperty *property = item;
  GtkLabel *label;
  GtkListBoxRow *row;

  g_assert (GBP_IS_GOBJECT_DIALOG (self));
  g_assert (GBP_IS_GOBJECT_PROPERTY (property));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "margin", 6,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);

  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (label));

  g_object_bind_property_full (property, "name", label, "label", G_BINDING_SYNC_CREATE,
                               name_to_label, NULL, NULL, NULL);

  g_object_set_data_full (G_OBJECT (row),
                          "GBP_GOBJECT_PROPERTY",
                          g_object_ref (property),
                          g_object_unref);

  return GTK_WIDGET (row);
}

GbpGobjectSpec *
gbp_gobject_dialog_get_spec (GbpGobjectDialog *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_DIALOG (self), NULL);

  return self->spec;
}

void
gbp_gobject_dialog_set_spec (GbpGobjectDialog *self,
                             GbpGobjectSpec   *spec)
{
  g_return_if_fail (GBP_IS_GOBJECT_DIALOG (self));
  g_return_if_fail (GBP_IS_GOBJECT_SPEC (spec));

  if (g_set_object (&self->spec, spec))
    {
      GListModel *props;

      g_signal_connect_object (spec,
                               "notify::ready",
                               G_CALLBACK (spec_notify_ready),
                               self,
                               G_CONNECT_SWAPPED);

      gbp_gobject_spec_editor_set_spec (self->editor, spec);

      if (spec != NULL)
        {
          props = gbp_gobject_spec_get_properties (spec);
          gtk_list_box_bind_model (self->properties_list_box,
                                   props,
                                   create_property_row,
                                   self,
                                   NULL);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SPEC]);
    }
}

/**
 * gbp_gobject_dialog_get_directory:
 *
 * Returns: (nullable) (transfer full): A #GFile or %NULL.
 */
GFile *
gbp_gobject_dialog_get_directory (GbpGobjectDialog *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_DIALOG (self), NULL);

  return gbp_gobject_spec_editor_get_directory (self->editor);
}

void
gbp_gobject_dialog_set_directory (GbpGobjectDialog *self,
                                  GFile            *directory)
{
  g_return_if_fail (GBP_IS_GOBJECT_DIALOG (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  gbp_gobject_spec_editor_set_directory (self->editor, directory);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
}

static void
gbp_gobject_dialog_property_selected (GbpGobjectDialog *self,
                                      GtkListBoxRow    *row,
                                      GtkListBox       *list_box)
{
  GAction *action;

  g_assert (GBP_IS_GOBJECT_DIALOG (self));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), "remove-property");

  g_assert (G_IS_SIMPLE_ACTION (action));

  if (row != NULL)
    {
      GbpGobjectProperty *prop;

      prop = g_object_get_data (G_OBJECT (row), "GBP_GOBJECT_PROPERTY");
      g_assert (!prop || GBP_IS_GOBJECT_PROPERTY (prop));
      gbp_gobject_property_editor_set_property (self->property_editor, prop);
      gtk_stack_set_visible_child_name (self->property_stack, "property");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
    }
  else
    {
      gbp_gobject_property_editor_set_property (self->property_editor, NULL);
      gtk_stack_set_visible_child_name (self->property_stack, "empty");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
    }
}

static void
gbp_gobject_dialog_constructed (GObject *object)
{
  GbpGobjectDialog *self = (GbpGobjectDialog *)object;

  g_assert (GBP_IS_GOBJECT_DIALOG (self));

  G_OBJECT_CLASS (gbp_gobject_dialog_parent_class)->constructed (object);

  if (self->spec == NULL)
    {
      g_autoptr(GbpGobjectSpec) spec = gbp_gobject_spec_new ();

      gbp_gobject_dialog_set_spec (self, spec);
    }
}

static void
gbp_gobject_dialog_destroy (GtkWidget *widget)
{
  GbpGobjectDialog *self = (GbpGobjectDialog *)widget;

  if (self->properties_list_box != NULL)
    g_signal_handlers_disconnect_by_func (self->properties_list_box,
                                          G_CALLBACK (gbp_gobject_dialog_property_selected),
                                          self);

  GTK_WIDGET_CLASS (gbp_gobject_dialog_parent_class)->destroy (widget);
}

static void
gbp_gobject_dialog_finalize (GObject *object)
{
  GbpGobjectDialog *self = (GbpGobjectDialog *)object;

  g_clear_object (&self->spec);
  g_clear_object (&self->actions);

  G_OBJECT_CLASS (gbp_gobject_dialog_parent_class)->finalize (object);
}

static void
gbp_gobject_dialog_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpGobjectDialog *self = GBP_GOBJECT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, gbp_gobject_dialog_get_directory (self));
      break;

    case PROP_SPEC:
      g_value_set_object (value, gbp_gobject_dialog_get_spec (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbpGobjectDialog *self = GBP_GOBJECT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      gbp_gobject_dialog_set_directory (self, g_value_get_object (value));
      break;

    case PROP_SPEC:
      gbp_gobject_dialog_set_spec (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_dialog_class_init (GbpGobjectDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_gobject_dialog_constructed;
  object_class->finalize = gbp_gobject_dialog_finalize;
  object_class->get_property = gbp_gobject_dialog_get_property;
  object_class->set_property = gbp_gobject_dialog_set_property;

  widget_class->destroy = gbp_gobject_dialog_destroy;

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "Directory",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SPEC] =
    g_param_spec_object ("spec",
                         "Spec",
                         "The GObject Specification",
                         GBP_TYPE_GOBJECT_SPEC,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/gobject-templates/gbp-gobject-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectDialog, editor);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectDialog, editor_page);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectDialog, properties_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectDialog, property_editor);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectDialog, property_stack);

  g_type_ensure (GBP_TYPE_GOBJECT_PROPERTY);
  g_type_ensure (GBP_TYPE_GOBJECT_PROPERTY_EDITOR);
  g_type_ensure (GBP_TYPE_GOBJECT_SIGNAL);
  g_type_ensure (GBP_TYPE_GOBJECT_SIGNAL_EDITOR);
  g_type_ensure (GBP_TYPE_GOBJECT_SPEC);
  g_type_ensure (GBP_TYPE_GOBJECT_SPEC_EDITOR);
}

static void
force_sidebar_hidden (GtkWidget  *widget,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (pspec != NULL);

  if (gtk_widget_get_visible (widget))
    gtk_widget_hide (widget);
}

static void
add_property (GSimpleAction *action,
              GVariant      *variant,
              gpointer       user_data)
{
  GbpGobjectDialog *self = user_data;
  g_autoptr(GbpGobjectProperty) property = NULL;
  GListModel *model;
  guint n_props;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (variant == NULL);
  g_assert (GBP_IS_GOBJECT_DIALOG (self));

  if (self->spec == NULL)
    return;

  property = gbp_gobject_property_new ();

  gbp_gobject_spec_add_property (self->spec, property);

  model = gbp_gobject_spec_get_properties (self->spec);
  n_props = g_list_model_get_n_items (model);

  if (n_props > 0)
    {
      GtkListBoxRow *row;

      row = gtk_list_box_get_row_at_index (self->properties_list_box, n_props - 1);

      if (row != NULL)
        {
          gtk_list_box_select_row (self->properties_list_box, row);
          gtk_widget_grab_focus (GTK_WIDGET (self->property_editor));
        }
    }
}

static void
remove_property (GSimpleAction *action,
                 GVariant      *variant,
                 gpointer       user_data)
{
  GbpGobjectDialog *self = user_data;
  GtkListBoxRow *row;

  g_assert (GBP_IS_GOBJECT_DIALOG (self));

  row = gtk_list_box_get_selected_row (self->properties_list_box);

  if (row != NULL)
    {
      GbpGobjectProperty *prop;

      prop = g_object_get_data (G_OBJECT (row), "GBP_GOBJECT_PROPERTY");
      gbp_gobject_spec_remove_property (self->spec, prop);
    }
}

static void
gbp_gobject_dialog_init (GbpGobjectDialog *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  GActionEntry entries[] = {
    { "add-property", add_property },
    { "remove-property", remove_property },
  };
  GtkWidget *child1;
  GtkWidget *child2;
  GAction *action;
  GList *list;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->properties_list_box,
                           "row-selected",
                           G_CALLBACK (gbp_gobject_dialog_property_selected),
                           self,
                           G_CONNECT_SWAPPED);

  /* Hide the sidebar widget */
  child1 = gtk_bin_get_child (GTK_BIN (self));
  list = gtk_container_get_children (GTK_CONTAINER (child1));
  child2 = list->data;
  g_signal_connect_after (child2,
                          "notify::visible",
                          G_CALLBACK (force_sidebar_hidden),
                          NULL);
  gtk_widget_hide (child2);
  g_list_free (list);

  /* Add actions needed by UI */
  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "dialog", G_ACTION_GROUP (group));
  self->actions = g_steal_pointer (&group);

  /* Disable remove-property by default */
  action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), "remove-property");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}
