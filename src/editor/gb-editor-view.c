/* gb-editor-view.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "editor-view"

#include <glib/gi18n.h>

#include "gb-animation.h"
#include "gb-editor-frame.h"
#include "gb-editor-frame-private.h"
#include "gb-editor-tweak-widget.h"
#include "gb-editor-view.h"
#include "gb-glib.h"
#include "gb-html-document.h"
#include "gb-log.h"
#include "gb-widget.h"

struct _GbEditorViewPrivate
{
  /* References owned by view */
  GbEditorDocument *document;

  /* Weak references */
  GbAnimation     *progress_anim;

  /* References owned by GtkWidget template */
  GtkPaned        *paned;
  GtkToggleButton *split_button;
  GbEditorFrame   *frame;
  GtkProgressBar  *progress_bar;
  GtkLabel        *error_label;
  GtkButton       *error_close_button;
  GtkRevealer     *error_revealer;
  GtkLabel        *modified_label;
  GtkButton       *modified_reload_button;
  GtkButton       *modified_cancel_button;
  GtkRevealer     *modified_revealer;
  GtkMenuButton   *tweak_button;

  guint            use_spaces : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorView, gb_editor_view, GB_TYPE_DOCUMENT_VIEW)

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_SPLIT_ENABLED,
  PROP_USE_SPACES,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GtkWidget *
gb_editor_view_new (GbEditorDocument *document)
{
  return g_object_new (GB_TYPE_EDITOR_VIEW,
                       "document", document,
                       NULL);
}

static void
gb_editor_view_action_set_state (GbEditorView *view,
                                 const gchar  *action_name,
                                 GVariant     *state)
{
  GActionGroup *group;
  GAction *action;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (action_name);
  g_return_if_fail (state);

  group = gtk_widget_get_action_group (GTK_WIDGET (view), "editor-view");
  action = g_action_map_lookup_action (G_ACTION_MAP (group), action_name);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), state);
}

gboolean
gb_editor_view_get_use_spaces (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  return view->priv->use_spaces;
}

void
gb_editor_view_set_use_spaces (GbEditorView *view,
                               gboolean      use_spaces)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  view->priv->use_spaces = use_spaces;
  gb_editor_view_action_set_state (view, "use-spaces",
                                   g_variant_new_boolean (use_spaces));
  g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs [PROP_USE_SPACES]);
}

static void
gb_editor_view_notify_language (GbEditorView     *view,
                                GParamSpec       *pspec,
                                GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  g_object_notify (G_OBJECT (view), "can-preview");
}

static void
gb_editor_view_notify_progress (GbEditorView     *view,
                                GParamSpec       *pspec,
                                GbEditorDocument *document)
{
  GbEditorViewPrivate *priv;
  gdouble progress;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  priv = view->priv;

  progress = gb_editor_document_get_progress (document);

  if (!gtk_widget_get_visible (GTK_WIDGET (priv->progress_bar)))
    {
      gtk_progress_bar_set_fraction (priv->progress_bar, 0.0);
      gtk_widget_set_opacity (GTK_WIDGET (priv->progress_bar), 1.0);
      gtk_widget_show (GTK_WIDGET (priv->progress_bar));
    }

  if (priv->progress_anim)
    gb_animation_stop (priv->progress_anim);

  gb_clear_weak_pointer (&priv->progress_anim);

  priv->progress_anim = gb_object_animate (priv->progress_bar,
                                           GB_ANIMATION_LINEAR,
                                           250,
                                           NULL,
                                           "fraction", progress,
                                           NULL);
  gb_set_weak_pointer (priv->progress_anim, &priv->progress_anim);

  if (progress == 1.0)
    gb_widget_fade_hide (GTK_WIDGET (priv->progress_bar));
}

static gboolean
gb_editor_view_get_can_preview (GbDocumentView *view)
{
  GbEditorViewPrivate *priv;
  GtkSourceLanguage *language;
  GtkSourceBuffer *buffer;
  const gchar *lang_id;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  priv = GB_EDITOR_VIEW (view)->priv;

  buffer = GTK_SOURCE_BUFFER (priv->document);
  language = gtk_source_buffer_get_language (buffer);
  if (!language)
    return FALSE;

  lang_id = gtk_source_language_get_id (language);
  if (!lang_id)
    return FALSE;

  return (g_str_equal (lang_id, "html") ||
          g_str_equal (lang_id, "markdown"));
}

/**
 * gb_editor_view_create_preview:
 * @view: A #GbEditorView.
 *
 * Creates a new document that can be previewed by calling
 * gb_document_create_view() on the document.
 *
 * Returns: (transfer full): A #GbDocument.
 */
static GbDocument *
gb_editor_view_create_preview (GbDocumentView *view)
{
  GbEditorView *self = (GbEditorView *)view;
  GbDocument *document;
  GbHtmlDocumentTransform transform = NULL;
  GtkSourceBuffer *buffer;
  GtkSourceLanguage *language;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (self), NULL);

  buffer = GTK_SOURCE_BUFFER (self->priv->document);
  language = gtk_source_buffer_get_language (buffer);

  if (language)
    {
      const gchar *lang_id;

      lang_id = gtk_source_language_get_id (language);

      if (g_strcmp0 (lang_id, "markdown") == 0)
        transform = gb_html_markdown_transform;
    }

  document = g_object_new (GB_TYPE_HTML_DOCUMENT,
                           "buffer", buffer,
                           NULL);

  if (transform)
    gb_html_document_set_transform_func (GB_HTML_DOCUMENT (document),
                                         transform);

  return document;
}

GbEditorFrame *
gb_editor_view_get_frame1 (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), NULL);

  return view->priv->frame;
}

GbEditorFrame *
gb_editor_view_get_frame2 (GbEditorView *view)
{
  GtkWidget *child2;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), NULL);

  child2 = gtk_paned_get_child2 (view->priv->paned);
  if (GB_IS_EDITOR_FRAME (child2))
    return GB_EDITOR_FRAME (child2);

  return NULL;
}

static void
gb_editor_view_hide_revealer_child (GtkRevealer *revealer)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  gtk_revealer_set_reveal_child (revealer, FALSE);
}

static void
gb_editor_view_file_changed_on_volume (GbEditorView     *view,
                                       GParamSpec       *pspec,
                                       GbEditorDocument *document)
{
  GtkSourceFile *source_file;
  GFile *location;
  gchar *path;
  gchar *str;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  source_file = gb_editor_document_get_file (document);
  location = gtk_source_file_get_location (source_file);

  if (!location)
    return;

  if (g_file_is_native (location))
    path = g_file_get_path (location);
  else
    path = g_file_get_uri (location);

  str = g_strdup_printf (_("The file “%s” was modified outside of Builder."),
                         path);

  gtk_label_set_label (view->priv->modified_label, str);
  gtk_revealer_set_reveal_child (view->priv->modified_revealer, TRUE);

  g_free (path);
  g_free (str);
}

static void
gb_editor_view_reload_document (GbEditorView *view,
                                GtkButton    *button)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  gb_editor_document_reload (view->priv->document);
  gtk_revealer_set_reveal_child (view->priv->modified_revealer, FALSE);
}

static void
gb_editor_view_notify_error (GbEditorView     *view,
                             GParamSpec       *pspec,
                             GbEditorDocument *document)
{
  const GError *error;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (pspec);
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  error = gb_editor_document_get_error (document);

  /* Ignore file not found errors */
  if (error &&
      (error->domain == G_IO_ERROR) &&
      (error->code == G_IO_ERROR_NOT_FOUND))
    error = NULL;

  if (!error)
    {
      if (gtk_revealer_get_reveal_child (view->priv->error_revealer))
        gtk_revealer_set_reveal_child (view->priv->error_revealer, FALSE);
    }
  else
    {
      gtk_label_set_label (view->priv->error_label, error->message);
      gtk_revealer_set_reveal_child (view->priv->error_revealer, TRUE);
    }
}

static gboolean
transform_language_to_string (GBinding     *binding,
                              const GValue *from_value,
                              GValue       *to_value,
                              gpointer      user_data)
{
  GtkSourceLanguage *language;
  const gchar *str = _("Plain Text");

  language = g_value_get_object (from_value);
  if (language)
    str = gtk_source_language_get_name (language);
  g_value_set_string (to_value, str);

  return TRUE;
}

static void
gb_editor_view_connect (GbEditorView     *view,
                        GbEditorDocument *document)
{
  GtkWidget *child2;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  gb_editor_frame_set_document (view->priv->frame, document);

  child2 = gtk_paned_get_child2 (view->priv->paned);
  if (GB_IS_EDITOR_FRAME (child2))
    gb_editor_frame_set_document (GB_EDITOR_FRAME (child2), document);

  g_signal_connect_object (document,
                           "notify::language",
                           G_CALLBACK (gb_editor_view_notify_language),
                           view,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (document,
                           "notify::progress",
                           G_CALLBACK (gb_editor_view_notify_progress),
                           view,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (view->priv->modified_cancel_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_hide_revealer_child),
                           view->priv->modified_revealer,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (view->priv->modified_reload_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_reload_document),
                           view,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (document,
                           "notify::error",
                           G_CALLBACK (gb_editor_view_notify_error),
                           view,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (view->priv->error_close_button,
                           "clicked",
                           G_CALLBACK (gb_editor_view_hide_revealer_child),
                           view->priv->error_revealer,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (document,
                           "notify::file-changed-on-volume",
                           G_CALLBACK (gb_editor_view_file_changed_on_volume),
                           view,
                           G_CONNECT_SWAPPED);

  g_object_bind_property_full (document, "language",
                               view->priv->tweak_button, "label",
                               G_BINDING_SYNC_CREATE,
                               transform_language_to_string,
                               NULL, NULL, NULL);
}

static void
gb_editor_view_disconnect (GbEditorView     *view,
                           GbEditorDocument *document)
{
  GtkWidget *child2;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  gb_editor_frame_set_document (view->priv->frame, NULL);

  child2 = gtk_paned_get_child2 (view->priv->paned);
  if (GB_IS_EDITOR_FRAME (child2))
    gb_editor_frame_set_document (GB_EDITOR_FRAME (child2), document);

  g_signal_handlers_disconnect_by_func (document,
                                        G_CALLBACK (gb_editor_view_notify_language),
                                        view);
  g_signal_handlers_disconnect_by_func (document,
                                        G_CALLBACK (gb_editor_view_notify_progress),
                                        view);
}

static GbDocument *
gb_editor_view_get_document (GbDocumentView *view)
{
  GbEditorViewPrivate *priv;

  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), NULL);

  priv = GB_EDITOR_VIEW (view)->priv;

  return GB_DOCUMENT (priv->document);
}

static void
gb_editor_view_set_document (GbEditorView     *view,
                             GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  if (document != view->priv->document)
    {
      if (view->priv->document)
        {
          gb_editor_view_disconnect (view, document);
          g_clear_object (&view->priv->document);
        }

      if (document)
        {
          view->priv->document = g_object_ref (document);
          gb_editor_view_connect (view, document);
        }

      g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs [PROP_DOCUMENT]);
    }
}

static void
gb_editor_view_toggle_split (GbEditorView *view)
{
  GbEditorViewPrivate *priv;
  GtkWidget *child2;
  gboolean active;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  priv = view->priv;

  if ((child2 = gtk_paned_get_child2 (priv->paned)))
    {
      gtk_widget_destroy (child2);
      gtk_widget_grab_focus (GTK_WIDGET (priv->frame));
      active = FALSE;
    }
  else
    {
      child2 = g_object_new (GB_TYPE_EDITOR_FRAME,
                             "document", view->priv->document,
                             "visible", TRUE,
                             NULL);
      g_object_bind_property (view, "use-spaces",
                              GB_EDITOR_FRAME (child2)->priv->source_view,
                              "insert-spaces-instead-of-tabs",
                              G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_container_add_with_properties (GTK_CONTAINER (priv->paned), child2,
                                         "shrink", TRUE,
                                         "resize", TRUE,
                                         NULL);
      gtk_widget_grab_focus (child2);
      active = TRUE;
    }

  gb_editor_view_action_set_state (view, "toggle-split",
                                   g_variant_new_boolean (active));

  EXIT;
}

gboolean
gb_editor_view_get_split_enabled (GbEditorView *view)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIEW (view), FALSE);

  return !!gb_editor_view_get_frame2 (view);
}

void
gb_editor_view_set_split_enabled (GbEditorView *view,
                                  gboolean      split_enabled)
{
  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  if (split_enabled == gb_editor_view_get_split_enabled (view))
    return;

  gb_editor_view_toggle_split (view);
  g_object_notify_by_pspec (G_OBJECT (view),
                            gParamSpecs [PROP_SPLIT_ENABLED]);
}

static void
gb_editor_view_switch_pane (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  GbEditorView *view = user_data;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  if (!gtk_widget_has_focus (GTK_WIDGET (view->priv->frame->priv->source_view)))
    gtk_widget_grab_focus (GTK_WIDGET (view->priv->frame));
  else
    {
      GtkWidget *child2;

      child2 = gtk_paned_get_child2 (view->priv->paned);
      if (child2)
        gtk_widget_grab_focus (child2);
    }

  EXIT;
}

static void
gb_editor_view_grab_focus (GtkWidget *widget)
{
  GbEditorView *view = (GbEditorView *)widget;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  gtk_widget_grab_focus (GTK_WIDGET (view->priv->frame));

  EXIT;
}

static void
apply_state_split (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  GbEditorView *view = user_data;
  gboolean split_enabled;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  split_enabled = g_variant_get_boolean (parameter);
  gb_editor_view_set_split_enabled (view, split_enabled);

  EXIT;
}

static void
apply_state_spaces (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  GbEditorView *view = user_data;
  gboolean use_spaces;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_VIEW (view));

  use_spaces = g_variant_get_boolean (parameter);
  gb_editor_view_set_use_spaces (view, use_spaces);

  EXIT;
}

static void
gb_editor_view_finalize (GObject *object)
{
  GbEditorView *view = (GbEditorView *)object;

  g_clear_object (&view->priv->document);

  G_OBJECT_CLASS (gb_editor_view_parent_class)->finalize (object);
}

static void
gb_editor_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, self->priv->document);
      break;

    case PROP_SPLIT_ENABLED:
      g_value_set_boolean (value, gb_editor_view_get_split_enabled (self));
      break;

    case PROP_USE_SPACES:
      g_value_set_boolean (value, gb_editor_view_get_use_spaces (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbEditorView *self = GB_EDITOR_VIEW (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      gb_editor_view_set_document (self, g_value_get_object (value));
      break;

    case PROP_SPLIT_ENABLED:
      gb_editor_view_set_split_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_USE_SPACES:
      gb_editor_view_set_use_spaces (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_view_class_init (GbEditorViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbDocumentViewClass *view_class = GB_DOCUMENT_VIEW_CLASS (klass);

  object_class->finalize = gb_editor_view_finalize;
  object_class->get_property = gb_editor_view_get_property;
  object_class->set_property = gb_editor_view_set_property;

  widget_class->grab_focus = gb_editor_view_grab_focus;

  view_class->get_document = gb_editor_view_get_document;
  view_class->get_can_preview = gb_editor_view_get_can_preview;
  view_class->create_preview = gb_editor_view_create_preview;

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The document edited by the view."),
                         GB_TYPE_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT,
                                   gParamSpecs [PROP_DOCUMENT]);

  gParamSpecs [PROP_SPLIT_ENABLED] =
    g_param_spec_boolean ("split-enabled",
                         _("Split Enabled"),
                         _("If the view split is enabled."),
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SPLIT_ENABLED,
                                   gParamSpecs [PROP_SPLIT_ENABLED]);

  gParamSpecs [PROP_USE_SPACES] =
    g_param_spec_boolean ("use-spaces",
                         _("Use Spaces"),
                         _("If spaces should be used instead of tabs."),
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_USE_SPACES,
                                   gParamSpecs [PROP_USE_SPACES]);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-view.ui");
  GB_WIDGET_CLASS_BIND (klass, GbEditorView, frame);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, paned);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, progress_bar);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, split_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, tweak_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, modified_revealer);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, modified_label);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, modified_cancel_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, modified_reload_button);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, error_label);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, error_revealer);
  GB_WIDGET_CLASS_BIND (widget_class, GbEditorView, error_close_button);

  g_type_ensure (GB_TYPE_EDITOR_FRAME);
  g_type_ensure (GB_TYPE_EDITOR_TWEAK_WIDGET);
}

static void
gb_editor_view_init (GbEditorView *self)
{
  const GActionEntry entries[] = {
    { "toggle-split", NULL, NULL, "false", apply_state_split },
    { "use-spaces", NULL, NULL, "false", apply_state_spaces },
    { "switch-pane",  gb_editor_view_switch_pane },
  };
  GSimpleActionGroup *actions;
  GtkWidget *controls;

  self->priv = gb_editor_view_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries,
                                   G_N_ELEMENTS (entries), self);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor-view",
                                  G_ACTION_GROUP (actions));

  controls = gb_document_view_get_controls (GB_DOCUMENT_VIEW (self));
  gtk_widget_insert_action_group (GTK_WIDGET (controls), "editor-view",
                                  G_ACTION_GROUP (actions));

  g_clear_object (&actions);

  g_object_bind_property (self->priv->frame->priv->source_view,
                          "insert-spaces-instead-of-tabs",
                          self, "use-spaces",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}
