/* ide-editor-view.c
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

#define G_LOG_DOMAIN "ide-editor-view"

#include <dazzle.h>
#include <libpeas/peas.h>

#include "editor/ide-editor-private.h"
#include "util/ide-gtk.h"

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_VIEW,
  N_PROPS
};

enum {
  DND_TARGET_URI_LIST = 100,
};

G_DEFINE_TYPE (IdeEditorView, ide_editor_view, IDE_TYPE_LAYOUT_VIEW)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_view_notify_child_revealed (IdeEditorView *self,
                                       GParamSpec    *pspec,
                                       GtkRevealer   *revealer)
{
  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (GTK_IS_REVEALER (revealer));

  if (gtk_revealer_get_child_revealed (revealer))
    gtk_widget_grab_focus (GTK_WIDGET (self->search_bar));
}

static void
ide_editor_view_drag_data_received (IdeEditorView    *self,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    guint             timestamp,
                                    IdeSourceView    *source_view)
{
  g_auto(GStrv) uri_list = NULL;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  switch (info)
    {
    case DND_TARGET_URI_LIST:
      uri_list = dzl_dnd_get_uri_list (selection_data);

      if (uri_list != NULL)
        {
          GVariantBuilder *builder;
          GVariant *variant;
          guint i;

          builder = g_variant_builder_new (G_VARIANT_TYPE_STRING_ARRAY);
          for (i = 0; uri_list [i]; i++)
            g_variant_builder_add (builder, "s", uri_list [i]);
          variant = g_variant_builder_end (builder);
          g_variant_builder_unref (builder);

          /*
           * request that we get focus first so the workbench will deliver the
           * document to us in the case it is not already open
           */
          gtk_widget_grab_focus (GTK_WIDGET (self));
          dzl_gtk_widget_action (GTK_WIDGET (self), "workbench", "open-uri-list", variant);
        }

      gtk_drag_finish (context, TRUE, FALSE, timestamp);
      break;

    default:
      break;
    }
}

static gboolean
ide_editor_view_focus_in_event (IdeEditorView *self,
                                GdkEventFocus *focus,
                                IdeSourceView *source_view)
{
  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  gtk_revealer_set_reveal_child (self->search_revealer, FALSE);

  if (self->buffer != NULL)
    ide_buffer_check_for_volume_change (self->buffer);

  return GDK_EVENT_PROPAGATE;
}

static void
ide_editor_view_buffer_modified_changed (IdeEditorView   *self,
                                         GtkSourceBuffer *buffer)
{
  gboolean modified;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  modified = gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer));

  ide_layout_view_set_modified (IDE_LAYOUT_VIEW (self), modified);
}

static void
ide_editor_view_buffer_notify_language_cb (IdeExtensionSetAdapter *set,
                                           PeasPluginInfo         *plugin_info,
                                           PeasExtension          *exten,
                                           gpointer                user_data)
{
  const gchar *language_id = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_VIEW_ADDIN (exten));

  ide_editor_view_addin_language_changed (IDE_EDITOR_VIEW_ADDIN (exten), language_id);
}

static void
ide_editor_view_buffer_notify_language (IdeEditorView   *self,
                                        GParamSpec      *pspec,
                                        GtkSourceBuffer *buffer)
{
  GtkSourceLanguage *language;
  const gchar *language_id = NULL;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->addins == NULL)
    return;

  if (NULL != (language = gtk_source_buffer_get_language (buffer)))
    language_id = gtk_source_language_get_id (language);

  ide_extension_set_adapter_set_value (self->addins, language_id);

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_editor_view_buffer_notify_language_cb,
                                     (gpointer)language_id);
}

static void
ide_editor_view_bind_signals (IdeEditorView   *self,
                              GtkSourceBuffer *buffer,
                              DzlSignalGroup  *buffer_signals)
{
  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  ide_editor_view_buffer_modified_changed (self, buffer);
  ide_editor_view_buffer_notify_language (self, NULL, buffer);
}

static void
ide_editor_view_set_buffer (IdeEditorView *self,
                            IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  if (g_set_object (&self->buffer, buffer))
    {
      dzl_signal_group_set_target (self->buffer_signals, buffer);
      dzl_binding_group_set_source (self->buffer_bindings, buffer);
      gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->source_view),
                                GTK_TEXT_BUFFER (buffer));
    }
}

static IdeLayoutView *
ide_editor_view_create_split_view (IdeLayoutView *view)
{
  IdeEditorView *self = (IdeEditorView *)view;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  return g_object_new (IDE_TYPE_EDITOR_VIEW,
                       "buffer", self->buffer,
                       "visible", TRUE,
                       NULL);
}

static void
ide_editor_view_addin_added (IdeExtensionSetAdapter *set,
                             PeasPluginInfo         *plugin_info,
                             PeasExtension          *exten,
                             gpointer                user_data)
{
  IdeEditorView *self = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_VIEW_ADDIN (exten));
  g_assert (IDE_IS_EDITOR_VIEW (self));

  ide_editor_view_addin_load (IDE_EDITOR_VIEW_ADDIN (exten), self);
}

static void
ide_editor_view_addin_removed (IdeExtensionSetAdapter *set,
                               PeasPluginInfo         *plugin_info,
                               PeasExtension          *exten,
                               gpointer                user_data)
{
  IdeEditorView *self = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_VIEW_ADDIN (exten));
  g_assert (IDE_IS_EDITOR_VIEW (self));

  ide_editor_view_addin_unload (IDE_EDITOR_VIEW_ADDIN (exten), self);
}

static void
ide_editor_view_hierarchy_changed (GtkWidget *widget,
                                   GtkWidget *old_toplevel)
{
  IdeEditorView *self = (IdeEditorView *)widget;
  IdeContext *context;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  /* Make sure we chain up if things change in the future */
  if (GTK_WIDGET_CLASS (ide_editor_view_parent_class)->hierarchy_changed)
    GTK_WIDGET_CLASS (ide_editor_view_parent_class)->hierarchy_changed (widget, old_toplevel);

  context = ide_widget_get_context (GTK_WIDGET (self));

  if (context != NULL && self->addins == NULL)
    {
      self->addins = ide_extension_set_adapter_new (context,
                                                    peas_engine_get_default (),
                                                    IDE_TYPE_EDITOR_VIEW_ADDIN,
                                                    "Editor-View-Languages",
                                                    ide_editor_view_get_language_id (self));

      g_signal_connect (self->addins,
                        "extension-added",
                        G_CALLBACK (ide_editor_view_addin_added),
                        self);

      g_signal_connect (self->addins,
                        "extension-removed",
                        G_CALLBACK (ide_editor_view_addin_removed),
                        self);

      ide_extension_set_adapter_foreach (self->addins,
                                         ide_editor_view_addin_added,
                                         self);
    }
}

static void
ide_editor_view_constructed (GObject *object)
{
  IdeEditorView *self = (IdeEditorView *)object;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  G_OBJECT_CLASS (ide_editor_view_parent_class)->constructed (object);

  _ide_editor_view_init_actions (self);
  _ide_editor_view_init_shortcuts (self);
  _ide_editor_view_init_settings (self);

  g_signal_connect_swapped (self->source_view,
                            "drag-data-received",
                            G_CALLBACK (ide_editor_view_drag_data_received),
                            self);

  g_signal_connect_swapped (self->source_view,
                            "focus-in-event",
                            G_CALLBACK (ide_editor_view_focus_in_event),
                            self);
}

static void
ide_editor_view_destroy (GtkWidget *widget)
{
  IdeEditorView *self = (IdeEditorView *)widget;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  g_clear_object (&self->addins);

  if (self->buffer_bindings != NULL)
    {
      dzl_binding_group_set_source (self->buffer_bindings, NULL);
      g_clear_object (&self->buffer_bindings);
    }

  if (self->buffer_signals != NULL)
    {
      dzl_signal_group_set_target (self->buffer_signals, NULL);
      g_clear_object (&self->buffer_signals);
    }

  g_clear_object (&self->buffer);

  GTK_WIDGET_CLASS (ide_editor_view_parent_class)->destroy (widget);
}

static void
ide_editor_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeEditorView *self = IDE_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_editor_view_get_buffer (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, ide_editor_view_get_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeEditorView *self = IDE_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_editor_view_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_view_class_init (IdeEditorViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeLayoutViewClass *layout_view_class = IDE_LAYOUT_VIEW_CLASS (klass);

  object_class->constructed = ide_editor_view_constructed;
  object_class->get_property = ide_editor_view_get_property;
  object_class->set_property = ide_editor_view_set_property;

  widget_class->destroy = ide_editor_view_destroy;
  widget_class->hierarchy_changed = ide_editor_view_hierarchy_changed;

  layout_view_class->create_split_view = ide_editor_view_create_split_view;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer for the view",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The view for editing the buffer",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, overlay);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, search_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, search_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, source_view);
  gtk_widget_class_bind_template_callback (widget_class, ide_editor_view_notify_child_revealed);

  g_type_ensure (IDE_TYPE_SOURCE_VIEW);
  g_type_ensure (IDE_TYPE_EDITOR_SEARCH_BAR);
}

static void
ide_editor_view_init (IdeEditorView *self)
{
  GtkTargetList *target_list;

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_layout_view_set_can_split (IDE_LAYOUT_VIEW (self), TRUE);
  ide_layout_view_set_menu_id (IDE_LAYOUT_VIEW (self), "ide-editor-view-document-menu");

  /*
   * Setup signals to monitor on the buffer.
   */
  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "modified-changed",
                                    G_CALLBACK (ide_editor_view_buffer_modified_changed),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::language",
                                    G_CALLBACK (ide_editor_view_buffer_notify_language),
                                    self);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (ide_editor_view_bind_signals),
                            self);

  /*
   * Setup bindings for the buffer.
   */
  self->buffer_bindings = dzl_binding_group_new ();
  dzl_binding_group_bind (self->buffer_bindings, "title", self, "title", 0);

  /*
   * Setup Drag and Drop support.
   */
  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (self->source_view));
  if (target_list != NULL)
    gtk_target_list_add_uri_targets (target_list, DND_TARGET_URI_LIST);
}

/**
 * ide_editor_view_get_buffer:
 * @self: a #IdeEditorView
 *
 * Gets the underlying buffer for the view.
 *
 * Returns: (transfer none): An #IdeBuffer
 *
 * Since: 3.26
 */
IdeBuffer *
ide_editor_view_get_buffer (IdeEditorView *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_VIEW (self), NULL);

  return self->buffer;
}

/**
 * ide_editor_view_get_view:
 * @self: a #IdeEditorView
 *
 * Gets the #IdeSourceView that is part of the #IdeEditorView.
 *
 * Returns: (transfer none): An #IdeSourceView
 *
 * Since: 3.26
 */
IdeSourceView *
ide_editor_view_get_view (IdeEditorView *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_VIEW (self), NULL);

  return self->source_view;
}

/**
 * ide_editor_view_get_language_id:
 * @self: a #IdeEditorView
 *
 * This is a helper to get the language-id of the underlying buffer.
 *
 * Returns: (nullable): the language-id as a string, or %NULL
 *
 * Since: 3.26
 */
const gchar *
ide_editor_view_get_language_id (IdeEditorView *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_VIEW (self), NULL);

  if (self->buffer != NULL)
    {
      GtkSourceLanguage *language;

      language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self->buffer));

      if (language != NULL)
        return gtk_source_language_get_id (language);
    }

  return NULL;
}
