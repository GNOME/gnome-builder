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

#include "config.h"

#include <dazzle.h>
#include <libpeas/peas.h>
#include <pango/pangofc-fontmap.h>

#include "ide-internal.h"
#include "ide-macros.h"

#include "editor/ide-editor-private.h"
#include "util/ide-gtk.h"

#define AUTO_HIDE_TIMEOUT_SECONDS 5

enum {
  PROP_0,
  PROP_AUTO_HIDE_MAP,
  PROP_BUFFER,
  PROP_SHOW_MAP,
  PROP_VIEW,
  N_PROPS
};

enum {
  DND_TARGET_URI_LIST = 100,
};

static void ide_editor_view_update_reveal_timer (IdeEditorView *self);

G_DEFINE_TYPE (IdeEditorView, ide_editor_view, IDE_TYPE_LAYOUT_VIEW)

DZL_DEFINE_COUNTER (instances, "Editor", "N Views", "Number of editor views");

static GParamSpec *properties [N_PROPS];
static FcConfig *localFontConfig;

static void
ide_editor_view_load_fonts (IdeEditorView *self)
{
  PangoFontMap *font_map;
  PangoFontDescription *font_desc;

  if (g_once_init_enter (&localFontConfig))
    {
      const gchar *font_path = PACKAGE_DATADIR "/gnome-builder/fonts/BuilderBlocks.ttf";
      FcConfig *config = FcInitLoadConfigAndFonts ();

      if (g_getenv ("GB_IN_TREE_FONTS") != NULL)
        font_path = "data/fonts/BuilderBlocks.ttf";

      if (!g_file_test (font_path, G_FILE_TEST_IS_REGULAR))
        g_warning ("Failed to locate \"%s\"", font_path);

      FcConfigAppFontAddFile (config, (const FcChar8 *)font_path);

      g_once_init_leave (&localFontConfig, config);
    }

  font_map = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
  pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (font_map), localFontConfig);
  gtk_widget_set_font_map (GTK_WIDGET (self->map), font_map);
  font_desc = pango_font_description_from_string ("Builder Blocks 1");

  g_assert (localFontConfig != NULL);
  g_assert (font_map != NULL);
  g_assert (font_desc != NULL);

  g_object_set (self->map, "font-desc", font_desc, NULL);

  pango_font_description_free (font_desc);
  g_object_unref (font_map);
}

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
ide_editor_view_buffer_loaded (IdeEditorView *self,
                               IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  /* Scroll to the insertion location once the buffer
   * has loaded. This is useful if it is not onscreen.
   */
  ide_source_view_scroll_to_insert (self->source_view);
}

static void
ide_editor_view_buffer_modified_changed (IdeEditorView *self,
                                         IdeBuffer     *buffer)
{
  gboolean modified = FALSE;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (!_ide_buffer_get_loading (buffer))
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
ide_editor_view_buffer_notify_language (IdeEditorView *self,
                                        GParamSpec    *pspec,
                                        IdeBuffer     *buffer)
{
  GtkSourceLanguage *language;
  const gchar *language_id = NULL;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->addins == NULL)
    return;

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  if (language != NULL)
    language_id = gtk_source_language_get_id (language);

  ide_extension_set_adapter_set_value (self->addins, language_id);
  ide_extension_set_adapter_foreach (self->addins,
                                     ide_editor_view_buffer_notify_language_cb,
                                     (gpointer)language_id);
}

static void
ide_editor_view_buffer_notify_style_scheme (IdeEditorView *self,
                                            GParamSpec    *pspec,
                                            IdeBuffer     *buffer)
{
  g_autofree gchar *background = NULL;
  g_autofree gchar *foreground = NULL;
  GtkSourceStyleScheme *scheme;
  GtkSourceStyle *style;
  gboolean background_set = FALSE;
  gboolean foreground_set = FALSE;
  GdkRGBA rgba;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (NULL == (scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer))) ||
      NULL == (style = gtk_source_style_scheme_get_style (scheme, "text")))
    goto unset_primary_color;

  g_object_get (style,
                "background-set", &background_set,
                "background", &background,
                "foreground-set", &foreground_set,
                "foreground", &foreground,
                NULL);

  if (!background_set || background == NULL || !gdk_rgba_parse (&rgba, background))
    goto unset_primary_color;

  if (background_set && background != NULL && gdk_rgba_parse (&rgba, background))
    ide_layout_view_set_primary_color_bg (IDE_LAYOUT_VIEW (self), &rgba);
  else
    goto unset_primary_color;

  if (foreground_set && foreground != NULL && gdk_rgba_parse (&rgba, foreground))
    ide_layout_view_set_primary_color_fg (IDE_LAYOUT_VIEW (self), &rgba);
  else
    ide_layout_view_set_primary_color_fg (IDE_LAYOUT_VIEW (self), NULL);

  return;

unset_primary_color:
  ide_layout_view_set_primary_color_bg (IDE_LAYOUT_VIEW (self), NULL);
  ide_layout_view_set_primary_color_fg (IDE_LAYOUT_VIEW (self), NULL);
}

static gboolean
ide_editor_view_source_view_event (IdeEditorView *self,
                                   GdkEvent      *event,
                                   IdeSourceView *source_view)
{
  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (event != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view) || GTK_SOURCE_IS_MAP (source_view));

  if (self->auto_hide_map)
    {
      ide_editor_view_update_reveal_timer (self);
      gtk_revealer_set_reveal_child (self->map_revealer, TRUE);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_editor_view_bind_signals (IdeEditorView  *self,
                              IdeBuffer      *buffer,
                              DzlSignalGroup *buffer_signals)
{
  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  ide_editor_view_buffer_modified_changed (self, buffer);
  ide_editor_view_buffer_notify_language (self, NULL, buffer);
  ide_editor_view_buffer_notify_style_scheme (self, NULL, buffer);
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

  /*
   * We don't need to chain up today, but if IdeLayoutView starts
   * using the hierarchy_changed signal to handle anything, we want
   * to make sure we aren't surprised.
   */
  if (GTK_WIDGET_CLASS (ide_editor_view_parent_class)->hierarchy_changed)
    GTK_WIDGET_CLASS (ide_editor_view_parent_class)->hierarchy_changed (widget, old_toplevel);

  context = ide_widget_get_context (GTK_WIDGET (self));

  /*
   * We don't want to create addins until the widget has been placed into
   * the widget tree. That way the addins can get access to the context
   * or other useful details.
   */
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
ide_editor_view_update_map (IdeEditorView *self)
{
  GtkWidget *parent;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self->map));

  g_object_ref (self->map);

  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (self->map));

  if (self->auto_hide_map)
    gtk_container_add (GTK_CONTAINER (self->map_revealer), GTK_WIDGET (self->map));
  else
    gtk_container_add (GTK_CONTAINER (self->scroller_box), GTK_WIDGET (self->map));

  gtk_widget_set_visible (GTK_WIDGET (self->map_revealer), self->show_map && self->auto_hide_map);
  gtk_widget_set_visible (GTK_WIDGET (self->map), self->show_map);
  gtk_revealer_set_reveal_child (self->map_revealer, self->show_map);

  ide_editor_view_update_reveal_timer (self);

  g_object_unref (self->map);
}

static void
search_revealer_notify_reveal_child (IdeEditorView *self,
                                     GParamSpec    *pspec,
                                     GtkRevealer   *revealer)
{
  GtkSourceCompletion *completion;

  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));
  g_return_if_fail (pspec != NULL);
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self->source_view));

  if (!gtk_revealer_get_reveal_child (revealer))
    {
      /*
       * Cancel any pending work by the context and release it. We don't need
       * to hold onto these when they aren't being used because they handle
       * buffer signals and other extraneous operations.
       */
      ide_editor_search_bar_set_context (self->search_bar, NULL);
      g_clear_object (&self->search_context);

      /* Restore completion that we blocked below. */
      gtk_source_completion_unblock_interactive (completion);
    }
  else
    {
      g_assert (self->search_context == NULL);

      self->search_context = g_object_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                                           "buffer", self->buffer,
                                           "highlight", TRUE,
                                           "settings", self->search_settings,
                                           NULL);
      ide_editor_search_bar_set_context (self->search_bar, self->search_context);

      /*
       * Block the completion while the search bar is set. It only
       * slows things down like search/replace functionality. We'll
       * restore it above when we clear state.
       */
      gtk_source_completion_block_interactive (completion);
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

  g_signal_connect_swapped (self->source_view,
                            "motion-notify-event",
                            G_CALLBACK (ide_editor_view_source_view_event),
                            self);

  g_signal_connect_swapped (self->source_view,
                            "scroll-event",
                            G_CALLBACK (ide_editor_view_source_view_event),
                            self);

  g_signal_connect_swapped (self->map,
                            "motion-notify-event",
                            G_CALLBACK (ide_editor_view_source_view_event),
                            self);

  /*
   * We want to track when the search revealer is visible. We will discard
   * the search context when the revealer is not visible so that we don't
   * continue performing expensive buffer operations.
   */
  g_signal_connect_swapped (self->search_revealer,
                            "notify::reveal-child",
                            G_CALLBACK (search_revealer_notify_reveal_child),
                            self);

  ide_editor_search_bar_set_settings (self->search_bar, self->search_settings);

  ide_editor_view_load_fonts (self);
  ide_editor_view_update_map (self);
}

static void
ide_editor_view_destroy (GtkWidget *widget)
{
  IdeEditorView *self = (IdeEditorView *)widget;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  ide_clear_source (&self->toggle_map_source);

  gtk_widget_insert_action_group (widget, "editor-view", NULL);

  g_clear_object (&self->addins);

  g_cancellable_cancel (self->destroy_cancellable);
  g_clear_object (&self->destroy_cancellable);

  g_clear_object (&self->search_settings);
  g_clear_object (&self->search_context);
  g_clear_object (&self->editor_settings);
  g_clear_object (&self->insight_settings);

  g_clear_object (&self->buffer);

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

  GTK_WIDGET_CLASS (ide_editor_view_parent_class)->destroy (widget);
}

static void
ide_editor_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_editor_view_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
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
    case PROP_AUTO_HIDE_MAP:
      g_value_set_boolean (value, ide_editor_view_get_auto_hide_map (self));
      break;

    case PROP_BUFFER:
      g_value_set_object (value, ide_editor_view_get_buffer (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, ide_editor_view_get_view (self));
      break;

    case PROP_SHOW_MAP:
      g_value_set_boolean (value, ide_editor_view_get_show_map (self));
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
    case PROP_AUTO_HIDE_MAP:
      ide_editor_view_set_auto_hide_map (self, g_value_get_boolean (value));
      break;

    case PROP_BUFFER:
      ide_editor_view_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_SHOW_MAP:
      ide_editor_view_set_show_map (self, g_value_get_boolean (value));
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

  object_class->finalize = ide_editor_view_finalize;
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

  properties [PROP_SHOW_MAP] =
    g_param_spec_boolean ("show-map",
                          "Show Map",
                          "If the overview map should be shown",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_AUTO_HIDE_MAP] =
    g_param_spec_boolean ("auto-hide-map",
                          "Auto Hide Map",
                          "If the overview map should be auto-hidden",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The view for editing the buffer",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, map);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, map_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, overlay);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, progress_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorView, scroller_box);
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

  DZL_COUNTER_INC (instances);

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_layout_view_set_can_split (IDE_LAYOUT_VIEW (self), TRUE);
  ide_layout_view_set_menu_id (IDE_LAYOUT_VIEW (self), "ide-editor-view-document-menu");

  self->destroy_cancellable = g_cancellable_new ();

  /* Setup signals to monitor on the buffer. */
  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "loaded",
                                    G_CALLBACK (ide_editor_view_buffer_loaded),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "modified-changed",
                                    G_CALLBACK (ide_editor_view_buffer_modified_changed),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::language",
                                    G_CALLBACK (ide_editor_view_buffer_notify_language),
                                    self);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "notify::style-scheme",
                                    G_CALLBACK (ide_editor_view_buffer_notify_style_scheme),
                                    self);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (ide_editor_view_bind_signals),
                            self);

  /*
   * Setup our search context. The sourceview has it's own search
   * infrastructure that we want to reserve for use by vim keybindings
   * and other transient keybinding features. Instead, we have our own
   * that can have separate state from those.
   *
   * We try to avoid creating/maintaining the search-context except
   * when necessary because has some expensive operations associated
   * with it's handling of changes to the underlying buffer.
   */
  self->search_settings = g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                                        "at-word-boundaries", FALSE,
                                        "case-sensitive", FALSE,
                                        "wrap-around", TRUE,
                                        NULL);


  /* Setup bindings for the buffer. */
  self->buffer_bindings = dzl_binding_group_new ();
  dzl_binding_group_bind (self->buffer_bindings, "title", self, "title", 0);

  /* Setup Drag and Drop support. */
  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (self->source_view));
  if (target_list != NULL)
    gtk_target_list_add_uri_targets (target_list, DND_TARGET_URI_LIST);

  /* Load our custom font for the overview map. */
  gtk_source_map_set_view (self->map, GTK_SOURCE_VIEW (self->source_view));
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

/**
 * ide_editor_view_scroll_to_line:
 * @self: a #IdeEditorView
 *
 * This is a helper to quickly jump to a given line without all the frills. It
 * will also ensure focus on the editor view, so that refocusing the view
 * afterwards does not cause the view to restore the cursor to the previous
 * location.
 */
void
ide_editor_view_scroll_to_line (IdeEditorView *self,
                                guint          line)
{
  GtkTextIter iter;

  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));
  g_return_if_fail (self->buffer != NULL);
  g_return_if_fail (line <= G_MAXINT);

  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));

  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self->buffer), &iter, line);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &iter, &iter);
  ide_source_view_scroll_to_insert (self->source_view);
}

gboolean
ide_editor_view_get_auto_hide_map (IdeEditorView *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_VIEW (self), FALSE);

  return self->auto_hide_map;
}

static gboolean
ide_editor_view_auto_hide_cb (gpointer user_data)
{
  IdeEditorView *self = user_data;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  self->toggle_map_source = 0;
  gtk_revealer_set_reveal_child (self->map_revealer, FALSE);

  return G_SOURCE_REMOVE;
}

static void
ide_editor_view_update_reveal_timer (IdeEditorView *self)
{
  g_assert (IDE_IS_EDITOR_VIEW (self));

  ide_clear_source (&self->toggle_map_source);

  if (self->auto_hide_map && gtk_revealer_get_reveal_child (self->map_revealer))
    {
      self->toggle_map_source =
        gdk_threads_add_timeout_seconds_full (G_PRIORITY_LOW,
                                              AUTO_HIDE_TIMEOUT_SECONDS,
                                              ide_editor_view_auto_hide_cb,
                                              g_object_ref (self),
                                              g_object_unref);
    }
}

void
ide_editor_view_set_auto_hide_map (IdeEditorView *self,
                                   gboolean       auto_hide_map)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));

  auto_hide_map = !!auto_hide_map;

  if (auto_hide_map != self->auto_hide_map)
    {
      self->auto_hide_map = auto_hide_map;
      ide_editor_view_update_map (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AUTO_HIDE_MAP]);
    }
}

gboolean
ide_editor_view_get_show_map (IdeEditorView *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_VIEW (self), FALSE);

  return self->show_map;
}

void
ide_editor_view_set_show_map (IdeEditorView *self,
                              gboolean       show_map)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));

  show_map = !!show_map;

  if (show_map != self->show_map)
    {
      self->show_map = show_map;
      g_object_set (self->scroller,
                    "vscrollbar-policy", show_map ? GTK_POLICY_EXTERNAL : GTK_POLICY_AUTOMATIC,
                    NULL);
      ide_editor_view_update_map (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AUTO_HIDE_MAP]);
    }
}

/**
 * ide_editor_view_set_language:
 * @self: a #IdeEditorView
 *
 * This is a convenience function to set the language on the underlying
 * #IdeBuffer text buffer.
 *
 * Since: 3.26
 */
void
ide_editor_view_set_language (IdeEditorView     *self,
                              GtkSourceLanguage *language)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));
  g_return_if_fail (!language || GTK_SOURCE_IS_LANGUAGE (language));

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (self->buffer), language);
}

/**
 * ide_editor_view_get_language:
 * @self: a #IdeEditorView
 *
 * Gets the #GtkSourceLanguage that is used by the underlying buffer.
 *
 * Returns: (transfer none) (nullable): A #GtkSourceLanguage or %NULL.
 *
 * Since: 3.26
 */
GtkSourceLanguage *
ide_editor_view_get_language (IdeEditorView *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_VIEW (self), NULL);

  return gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (self->buffer));
}

/**
 * ide_editor_view_move_next_error:
 * @self: a #IdeEditorView
 *
 * Moves to the next error, if any.
 *
 * If there is no error, the insertion cursor is not moved.
 *
 * Since: 3.26
 */
void
ide_editor_view_move_next_error (IdeEditorView *self)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));

  g_signal_emit_by_name (self->source_view, "move-error", GTK_DIR_DOWN);
}

/**
 * ide_editor_view_move_previous_error:
 * @self: a #IdeEditorView
 *
 * Moves the insertion cursor to the previous error.
 *
 * If there is no error, the insertion cursor is not moved.
 *
 * Since: 3.26
 */
void
ide_editor_view_move_previous_error (IdeEditorView *self)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));

  g_signal_emit_by_name (self->source_view, "move-error", GTK_DIR_UP);
}

static void
ide_editor_view_move_next_search_result_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GtkSourceSearchContext *context = (GtkSourceSearchContext *)object;
  g_autoptr(IdeEditorView) self = user_data;
  g_autoptr(GError) error = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_wrapped = FALSE;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (self->buffer == NULL)
    return;

  if (gtk_source_search_context_forward_finish2 (context, result, &begin, &end, &has_wrapped, &error))
    {
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &begin, &end);
      ide_source_view_scroll_to_insert (self->source_view);
    }
}

/**
 * ide_editor_view_move_next_search_result:
 * @self: a #IdeEditorView
 *
 * Moves the insertion cursor to the next search result.
 *
 * If there is no search result, the insertion cursor is not moved.
 *
 * Since: 3.26
 */
void
ide_editor_view_move_next_search_result (IdeEditorView *self)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));
  g_return_if_fail (self->destroy_cancellable != NULL);
  g_return_if_fail (self->buffer != NULL);

  if (self->search_context == NULL)
    return;

  if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end))
    gtk_text_iter_order (&begin, &end);

  gtk_source_search_context_forward_async (self->search_context,
                                           &end,
                                           self->destroy_cancellable,
                                           ide_editor_view_move_next_search_result_cb,
                                           g_object_ref (self));
}

static void
ide_editor_view_move_previous_search_result_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GtkSourceSearchContext *context = (GtkSourceSearchContext *)object;
  g_autoptr(IdeEditorView) self = user_data;
  g_autoptr(GError) error = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_wrapped = FALSE;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (self->buffer == NULL)
    return;

  if (gtk_source_search_context_backward_finish2 (context, result, &begin, &end, &has_wrapped, &error))
    {
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &begin, &end);
      ide_source_view_scroll_to_insert (self->source_view);
    }
}

/**
 * ide_editor_view_move_previous_search_result:
 * @self: a #IdeEditorView
 *
 * Moves the insertion cursor to the previous search result.
 *
 * If there is no search result, the insertion cursor is not moved.
 *
 * Since: 3.26
 */
void
ide_editor_view_move_previous_search_result (IdeEditorView *self)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));
  g_return_if_fail (self->destroy_cancellable != NULL);
  g_return_if_fail (self->buffer != NULL);

  if (self->search_context == NULL)
    return;

  if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end))
    gtk_text_iter_order (&begin, &end);

  gtk_source_search_context_backward_async (self->search_context,
                                            &begin,
                                            self->destroy_cancellable,
                                            ide_editor_view_move_previous_search_result_cb,
                                            g_object_ref (self));
}
