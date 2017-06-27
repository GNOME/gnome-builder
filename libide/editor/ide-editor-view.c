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

#include "ide-editor-private.h"
#include "ide-editor-view.h"
#include "ide-editor-view-addin.h"

struct _IdeEditorView
{
  IdeLayoutView      parent_instance;

  PeasExtensionSet  *addins;

  IdeBuffer         *buffer;
  DzlBindingGroup   *buffer_bindings;
  DzlSignalGroup    *buffer_signals;

  GtkOverlay        *overlay;
  IdeSourceView     *source_view;
  GtkScrolledWindow *scroller;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorView, ide_editor_view, IDE_TYPE_LAYOUT_VIEW)

static GParamSpec *properties [N_PROPS];

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
ide_editor_view_buffer_notify_language_cb (PeasExtensionSet *set,
                                           PeasPluginInfo   *plugin_info,
                                           PeasExtension    *exten,
                                           gpointer          user_data)
{
  const gchar *language_id = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
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

  peas_extension_set_foreach (self->addins,
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
    dzl_signal_group_set_target (self->buffer_signals, buffer);
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
ide_editor_view_addin_added (PeasExtensionSet *set,
                             PeasPluginInfo   *plugin_info,
                             PeasExtension    *exten,
                             gpointer          user_data)
{
  IdeEditorView *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_VIEW_ADDIN (exten));
  g_assert (IDE_IS_EDITOR_VIEW (self));

  ide_editor_view_addin_load (IDE_EDITOR_VIEW_ADDIN (exten), self);
}

static void
ide_editor_view_addin_removed (PeasExtensionSet *set,
                               PeasPluginInfo   *plugin_info,
                               PeasExtension    *exten,
                               gpointer          user_data)
{
  IdeEditorView *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
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

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  GTK_WIDGET_CLASS (ide_editor_view_parent_class)->hierarchy_changed (widget, old_toplevel);

  if (self->addins == NULL)
    {
      self->addins = peas_extension_set_new (NULL, IDE_TYPE_EDITOR_VIEW_ADDIN, NULL);

      g_signal_connect (self->addins,
                        "extension-added",
                        G_CALLBACK (ide_editor_view_addin_added),
                        self);

      g_signal_connect (self->addins,
                        "extension-removed",
                        G_CALLBACK (ide_editor_view_addin_removed),
                        self);

      peas_extension_set_foreach (self->addins,
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

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, overlay);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, source_view);
}

static void
ide_editor_view_init (IdeEditorView *self)
{
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
 * ide_editor_view_get_source_view:
 * @self: a #IdeEditorView
 *
 * Gets the #IdeSourceView that is part of the #IdeEditorView.
 *
 * Returns: (transfer none): An #IdeSourceView
 *
 * Since: 3.26
 */
IdeSourceView *
ide_editor_view_get_source_view (IdeEditorView *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_VIEW (self), NULL);

  return self->source_view;
}
