/* gbp-gobject-property-editor.c
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

#define G_LOG_DOMAIN "gbp-gobject-property-editor"

#include <egg-binding-group.h>
#include <egg-state-machine.h>

#include "gbp-gobject-property.h"
#include "gbp-gobject-property-editor.h"

struct _GbpGobjectPropertyEditor
{
  GtkBin              parent_instance;

  GbpGobjectProperty *property;
  EggBindingGroup    *property_bindings;

  GtkEntry           *default_entry;
  GtkEntry           *max_entry;
  GtkEntry           *min_entry;
  GtkEntry           *name_entry;
  GtkEntry           *ctype_entry;
  GtkComboBoxText    *kind_combobox;
  EggStateMachine    *kind_state;
  GtkSwitch          *readable_switch;
  GtkSwitch          *writable_switch;
  GtkSwitch          *construct_only_switch;
};

G_DEFINE_TYPE (GbpGobjectPropertyEditor, gbp_gobject_property_editor, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_PROPERTY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

void
gbp_gobject_property_editor_set_property (GbpGobjectPropertyEditor *self,
                                          GbpGobjectProperty       *property)
{
  g_return_if_fail (GBP_IS_GOBJECT_PROPERTY_EDITOR (self));
  g_return_if_fail (!property || GBP_IS_GOBJECT_PROPERTY (property));

  if (g_set_object (&self->property, property))
    {
      egg_binding_group_set_source (self->property_bindings, property);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROPERTY]);
    }
}

static void
gbp_gobject_property_editor_grab_focus (GtkWidget *widget)
{
  GbpGobjectPropertyEditor *self = (GbpGobjectPropertyEditor *)widget;

  g_assert (GBP_IS_GOBJECT_PROPERTY_EDITOR (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->name_entry));
}

static void
gbp_gobject_property_editor_finalize (GObject *object)
{
  GbpGobjectPropertyEditor *self = (GbpGobjectPropertyEditor *)object;

  g_clear_object (&self->property);
  g_clear_object (&self->property_bindings);

  G_OBJECT_CLASS (gbp_gobject_property_editor_parent_class)->finalize (object);
}

static void
gbp_gobject_property_editor_do_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  GbpGobjectPropertyEditor *self = GBP_GOBJECT_PROPERTY_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PROPERTY:
      g_value_set_object (value, self->property);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_property_editor_do_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  GbpGobjectPropertyEditor *self = GBP_GOBJECT_PROPERTY_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PROPERTY:
      gbp_gobject_property_editor_set_property (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_property_editor_class_init (GbpGobjectPropertyEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_gobject_property_editor_finalize;
  object_class->get_property = gbp_gobject_property_editor_do_get_property;
  object_class->set_property = gbp_gobject_property_editor_do_set_property;

  widget_class->grab_focus = gbp_gobject_property_editor_grab_focus;

  properties [PROP_PROPERTY] =
    g_param_spec_object ("property",
                         "Property",
                         "Property",
                         GBP_TYPE_GOBJECT_PROPERTY,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/gobject-templates/gbp-gobject-property-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, construct_only_switch);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, ctype_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, default_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, kind_combobox);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, kind_state);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, max_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, min_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, name_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, readable_switch);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectPropertyEditor, writable_switch);
}

static gboolean
string_to_text (GBinding     *binding,
                const GValue *from_value,
                GValue       *to_value,
                gpointer      user_data)
{
  const gchar *str;

  str = g_value_get_string (from_value);

  if (str == NULL)
    g_value_set_static_string (to_value, "");
  else
    g_value_set_string (to_value, str);

  return TRUE;
}

static gboolean
kind_to_string (GBinding     *binding,
                const GValue *from_value,
                GValue       *to_value,
                gpointer      user_data)
{
  const GEnumValue *value;
  GEnumClass *klass;

  /* klass should always be available here */
  klass = g_type_class_peek (GBP_TYPE_GOBJECT_PROPERTY_KIND);
  g_assert (klass != NULL);

  value = g_enum_get_value (klass, g_value_get_enum (from_value));

  if (value == NULL)
    return FALSE;

  g_value_set_string (to_value, value->value_nick);

  return TRUE;
}

static gboolean
string_to_kind (GBinding     *binding,
                const GValue *from_value,
                GValue       *to_value,
                gpointer      user_data)
{
  const GEnumValue *value;
  GEnumClass *klass;

  /* klass should always be available here */
  klass = g_type_class_peek (GBP_TYPE_GOBJECT_PROPERTY_KIND);
  g_assert (klass != NULL);

  value = g_enum_get_value_by_nick (klass, g_value_get_string (from_value));

  if (value == NULL)
    return FALSE;

  g_value_set_enum (to_value, value->value);

  return TRUE;
}

static void
gbp_gobject_property_editor_init (GbpGobjectPropertyEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->property_bindings = egg_binding_group_new ();

  egg_binding_group_bind_full (self->property_bindings, "name",
                               self->name_entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               string_to_text, NULL, NULL, NULL);

  egg_binding_group_bind_full (self->property_bindings, "kind",
                               self->kind_combobox, "active-id",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               kind_to_string, string_to_kind, NULL, NULL);

  egg_binding_group_bind_full (self->property_bindings, "kind",
                               self->kind_state, "state",
                               G_BINDING_SYNC_CREATE,
                               kind_to_string, string_to_kind, NULL, NULL);

  egg_binding_group_bind_full (self->property_bindings, "ctype",
                               self->ctype_entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               string_to_text, NULL, NULL, NULL);

  egg_binding_group_bind_full (self->property_bindings, "minimum",
                               self->min_entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               string_to_text, NULL, NULL, NULL);

  egg_binding_group_bind_full (self->property_bindings, "maximum",
                               self->max_entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               string_to_text, NULL, NULL, NULL);

  egg_binding_group_bind_full (self->property_bindings, "default",
                               self->default_entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               string_to_text, NULL, NULL, NULL);

  egg_binding_group_bind (self->property_bindings, "construct-only",
                          self->construct_only_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  egg_binding_group_bind (self->property_bindings, "readable",
                          self->readable_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  egg_binding_group_bind (self->property_bindings, "writable",
                          self->writable_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}
