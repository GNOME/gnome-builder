/* gbp-gobject-spec-editor.c
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

#define G_LOG_DOMAIN "gbp-gobject-spec-editor"

#include <egg-binding-group.h>
#include <egg-file-chooser-entry.h>
#include <egg-state-machine.h>

#include "gbp-gobject-spec-editor.h"

struct _GbpGobjectSpecEditor
{
  GtkBin               parent_instance;

  GbpGobjectSpec      *spec;
  EggBindingGroup     *spec_bindings;

  GtkSwitch           *derive_switch;
  GtkEntry            *class_entry;
  EggStateMachine     *language_state;
  EggFileChooserEntry *location_entry;
  GtkEntry            *name_entry;
  GtkEntry            *namespace_entry;
  GtkEntry            *parent_entry;
};

enum {
  PROP_0,
  PROP_SPEC,
  N_PROPS
};

G_DEFINE_TYPE (GbpGobjectSpecEditor, gbp_gobject_spec_editor, GTK_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

static void
gbp_gobject_spec_editor_finalize (GObject *object)
{
  GbpGobjectSpecEditor *self = (GbpGobjectSpecEditor *)object;

  g_clear_object (&self->spec_bindings);
  g_clear_object (&self->spec);

  G_OBJECT_CLASS (gbp_gobject_spec_editor_parent_class)->finalize (object);
}

static void
gbp_gobject_spec_editor_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbpGobjectSpecEditor *self = GBP_GOBJECT_SPEC_EDITOR (object);

  switch (prop_id)
    {
    case PROP_SPEC:
      g_value_set_object (value, gbp_gobject_spec_editor_get_spec (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_spec_editor_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbpGobjectSpecEditor *self = GBP_GOBJECT_SPEC_EDITOR (object);

  switch (prop_id)
    {
    case PROP_SPEC:
      gbp_gobject_spec_editor_set_spec (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gobject_spec_editor_class_init (GbpGobjectSpecEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_gobject_spec_editor_finalize;
  object_class->get_property = gbp_gobject_spec_editor_get_property;
  object_class->set_property = gbp_gobject_spec_editor_set_property;

  properties [PROP_SPEC] =
    g_param_spec_object ("spec",
                         "Spec",
                         "The gobject specification",
                         GBP_TYPE_GOBJECT_SPEC,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/gobject-templates/gbp-gobject-spec-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectSpecEditor, class_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectSpecEditor, derive_switch);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectSpecEditor, language_state);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectSpecEditor, location_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectSpecEditor, name_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectSpecEditor, namespace_entry);
  gtk_widget_class_bind_template_child (widget_class, GbpGobjectSpecEditor, parent_entry);
}

static void
gbp_gobject_spec_editor_init (GbpGobjectSpecEditor *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  g_autoptr(GAction) language_state_action = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  group = g_simple_action_group_new ();
  language_state_action = egg_state_machine_create_action (self->language_state, "language");
  g_action_map_add_action (G_ACTION_MAP (group), language_state_action);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "spec", G_ACTION_GROUP (group));

  self->spec_bindings = egg_binding_group_new ();

  egg_binding_group_bind (self->spec_bindings, "name",
                          self->name_entry, "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  egg_binding_group_bind (self->spec_bindings, "class-name",
                          self->class_entry, "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  egg_binding_group_bind (self->spec_bindings, "namespace",
                          self->namespace_entry, "text",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  egg_binding_group_bind (self->spec_bindings, "final",
                          self->derive_switch, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL | G_BINDING_INVERT_BOOLEAN);
}

/**
 * gbp_gobject_spec_editor_get_spec:
 *
 * Returns: (transfer none): A #GbpGobjectSpecEditor.
 */
GbpGobjectSpec *
gbp_gobject_spec_editor_get_spec (GbpGobjectSpecEditor *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC_EDITOR (self), NULL);

  return self->spec;
}

void
gbp_gobject_spec_editor_set_spec (GbpGobjectSpecEditor *self,
                                  GbpGobjectSpec       *spec)
{
  g_return_if_fail (GBP_IS_GOBJECT_SPEC_EDITOR (self));
  g_return_if_fail (!spec || GBP_IS_GOBJECT_SPEC (spec));

  if (g_set_object (&self->spec, spec))
    {
      egg_binding_group_set_source (self->spec_bindings, spec);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SPEC]);
    }
}

void
gbp_gobject_spec_editor_set_directory (GbpGobjectSpecEditor *self,
                                       GFile                *directory)
{
  g_return_if_fail (GBP_IS_GOBJECT_SPEC_EDITOR (self));
  g_return_if_fail (!directory || G_IS_FILE (directory));

  egg_file_chooser_entry_set_file (self->location_entry, directory);
}

/**
 * gbp_gobject_spec_editor_get_directory:
 *
 * Returns: (transfer full) (nullable): A #GFile or %NULL.
 */
GFile *
gbp_gobject_spec_editor_get_directory (GbpGobjectSpecEditor *self)
{
  g_return_val_if_fail (GBP_IS_GOBJECT_SPEC_EDITOR (self), NULL);

  return egg_file_chooser_entry_get_file (self->location_entry);
}
