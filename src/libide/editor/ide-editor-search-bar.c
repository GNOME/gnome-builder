/* ide-editor-search-bar.c
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

#define G_LOG_DOMAIN "ide-editor-search-bar"

#include <glib/gi18n.h>

#include "ide-macros.h"

#include "application/ide-application.h"
#include "editor/ide-editor-private.h"
#include "editor/ide-editor-search-bar.h"

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_SETTINGS,
  N_PROPS
};

enum {
  STOP_SEARCH,
  N_SIGNALS
};

G_DEFINE_TYPE (IdeEditorSearchBar, ide_editor_search_bar, DZL_TYPE_BIN)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

gboolean
ide_editor_search_bar_get_replace_mode (IdeEditorSearchBar *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self), FALSE);

  return gtk_widget_get_visible (GTK_WIDGET (self->replace_entry));
}

void
ide_editor_search_bar_set_replace_mode (IdeEditorSearchBar *self,
                                        gboolean            replace_mode)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  gtk_widget_set_visible (GTK_WIDGET (self->replace_entry), replace_mode);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_button), replace_mode);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_all_button), replace_mode);
}

static gboolean
maybe_escape_regex (GBinding     *binding,
                    const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  IdeEditorSearchBar *self = user_data;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (from_value != NULL);
  g_assert (to_value != NULL);

  if (g_value_get_string (from_value) == NULL)
    g_value_set_static_string (to_value, "");
  else
    {
      const gchar *entry_text = g_value_get_string (from_value);
      g_autofree gchar *unescaped = NULL;

      if (!gtk_source_search_settings_get_regex_enabled (self->settings))
        entry_text = unescaped = gtk_source_utils_unescape_search_text (entry_text);

      g_value_set_string (to_value, entry_text);
    }

  return TRUE;
}

static gboolean
pacify_null_text (GBinding     *binding,
                  const GValue *from_value,
                  GValue       *to_value,
                  gpointer      user_data)
{
  g_assert (from_value != NULL);
  g_assert (to_value != NULL);

  if (g_value_get_string (from_value) == NULL)
    g_value_set_static_string (to_value, "");
  else
    g_value_copy (from_value, to_value);

  return TRUE;
}

static void
update_replace_actions_sensitivity (IdeEditorSearchBar *self)
{
  g_autoptr(GError) regex_error = NULL;
  g_autoptr(GError) replace_regex_error = NULL;
  GtkSourceBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  const gchar *search_text;
  const gchar *replace_text;
  gint pos;
  gint count;
  gboolean enable_replace;
  gboolean enable_replace_all;
  gboolean replace_regex_valid;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (self->context == NULL || self->settings == NULL)
    return;

  buffer = gtk_source_search_context_get_buffer (self->context);

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

  /* Gather enough info to determine if Replace or Replace All would make sense */
  search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
  pos = gtk_source_search_context_get_occurrence_position (self->context, &begin, &end);
  count = gtk_source_search_context_get_occurrences_count (self->context);
  regex_error = gtk_source_search_context_get_regex_error (self->context);
  replace_regex_valid = gtk_source_search_settings_get_regex_enabled (self->settings) ?
                        g_regex_check_replacement (replace_text, NULL, &replace_regex_error) :
                        TRUE;

  enable_replace = (!ide_str_empty0 (search_text) &&
                    regex_error == NULL &&
                    replace_regex_valid &&
                    pos > 0);

  enable_replace_all = (!ide_str_empty0 (search_text) &&
                        regex_error == NULL &&
                        replace_regex_valid &&
                        count > 0);

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "search-bar", "replace",
                             "enabled", enable_replace,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "search-bar", "replace-all",
                             "enabled", enable_replace_all,
                             NULL);
}

static void
on_notify_search_text (IdeEditorSearchBar      *self,
                       GParamSpec              *pspec,
                       GtkSourceSearchSettings *search_settings)
{
  GtkWidget *widget;
  IdeEditorView *editor_view;
  IdeSourceView *view;
  GtkSourceSearchContext *view_search_context;
  GtkSourceSearchSettings *view_search_settings;
  const gchar *search_text;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_SETTINGS (search_settings));

  /* We set the view context search text for keymodes searching */
  if (self->context == NULL)
    {
      if (NULL != (widget = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_EDITOR_VIEW)))
        {
          editor_view = IDE_EDITOR_VIEW (widget);
          view = ide_editor_view_get_view (editor_view);

          search_text = gtk_source_search_settings_get_search_text (search_settings);

          if (NULL != (view_search_context = ide_source_view_get_search_context (view)))
            {
              view_search_settings = gtk_source_search_context_get_settings (view_search_context);
              gtk_source_search_settings_set_search_text (view_search_settings, search_text);
            }
        }
    }

  update_replace_actions_sensitivity (self);
}

static void
set_position_label (IdeEditorSearchBar *self,
                    const gchar        *text)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (ide_str_empty0 (text))
    {
      if (self->search_entry_tag != NULL)
        {
          gd_tagged_entry_remove_tag (self->search_entry, self->search_entry_tag);
          g_clear_object (&self->search_entry_tag);
        }

      return;
    }

  if (self->search_entry_tag == NULL)
    {
      self->search_entry_tag = gd_tagged_entry_tag_new ("");
      gd_tagged_entry_add_tag (self->search_entry, self->search_entry_tag);
      gd_tagged_entry_tag_set_style (self->search_entry_tag,
                                     "search-occurrences-tag");
    }

  gd_tagged_entry_tag_set_label (self->search_entry_tag, text);
}

static void
update_search_position_label (IdeEditorSearchBar *self)
{
  g_autofree gchar *text = NULL;
  GtkStyleContext *context;
  GtkSourceBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  const gchar *search_text;
  gint count;
  gint pos;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (self->settings == NULL || self->context == NULL)
    return;

  buffer = gtk_source_search_context_get_buffer (self->context);

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  pos = gtk_source_search_context_get_occurrence_position (self->context, &begin, &end);
  count = gtk_source_search_context_get_occurrences_count (self->context);

  if ((pos == -1) || (count == -1))
    {
      /*
       * We are not yet done scanning the buffer.
       * We will be updated when we know more, so just hide it for now.
       */
      set_position_label (self, NULL);
      return;
    }

  context = gtk_widget_get_style_context (GTK_WIDGET (self->search_entry));
  search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

  /* We use our own error class because we don't want to colide with styling
   * from GTK+ themes.
   */
  if ((count == 0) && !ide_str_empty0 (search_text))
    gtk_style_context_add_class (context, "search-missing");
  else
    gtk_style_context_remove_class (context, "search-missing");

  /* translators: first %u is the Nth position of second %u N occurrences */
  text = g_strdup_printf (_("%u of %u"), pos, count);
  set_position_label (self, text);
}

static void
on_notify_occurrences_count (IdeEditorSearchBar     *self,
                             GParamSpec             *pspec,
                             GtkSourceSearchContext *search_context)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  update_search_position_label (self);
  update_replace_actions_sensitivity (self);
}

static void
on_cursor_moved (IdeEditorSearchBar *self,
                 const GtkTextIter  *iter,
                 IdeBuffer          *buffer)
{
  gint count;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (iter != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  count = gtk_source_search_context_get_occurrences_count (self->context);

  if (count != -1)
    {
      update_search_position_label (self);
      update_replace_actions_sensitivity (self);
    }
}

static void
on_notify_regex_error (IdeEditorSearchBar     *self,
                       GParamSpec             *pspec,
                       GtkSourceSearchContext *search_context)
{
  g_autoptr(GError) error = NULL;
  PangoAttrList *attrs = NULL;
  const gchar *tooltip_text = NULL;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  /*
   * If the regular expression is invalid, add a white squiggly underline;
   * otherwise remove it. We will also set the tooltip-text to the error
   * that occurred while parsing the regex.
   */

  error = gtk_source_search_context_get_regex_error (search_context);

  if (error != NULL)
    {
      attrs = pango_attr_list_new ();
      pango_attr_list_insert (attrs, pango_attr_underline_new (PANGO_UNDERLINE_ERROR));
      pango_attr_list_insert (attrs, pango_attr_underline_color_new (65535, 65535, 65535));
      tooltip_text = error->message;
    }

  gtk_entry_set_attributes (GTK_ENTRY (self->search_entry), attrs);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->search_entry), tooltip_text);

  update_replace_actions_sensitivity (self);

  pango_attr_list_unref (attrs);
}

static void
check_replace_text (IdeEditorSearchBar *self)
{
  g_autoptr(GError) error = NULL;
  PangoAttrList *attrs = NULL;
  const gchar *tooltip_text = NULL;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (self->settings != NULL);

  if (self->context == NULL)
    return;

  /*
   * If the replace expression is invalid, add a white squiggly underline;
   * otherwise remove it. Also set the error message to the tooltip text
   * so that the user can get some info on the error.
   */
  if (gtk_source_search_settings_get_regex_enabled (self->settings))
    {
      const gchar *replace_text;

      replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

      if (!g_regex_check_replacement (replace_text, NULL, &error))
        {
          attrs = pango_attr_list_new ();
          pango_attr_list_insert (attrs, pango_attr_underline_new (PANGO_UNDERLINE_ERROR));
          pango_attr_list_insert (attrs, pango_attr_underline_color_new (65535, 65535, 65535));
          tooltip_text = error->message;
        }
    }

  gtk_entry_set_attributes (GTK_ENTRY (self->replace_entry), attrs);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->replace_entry), tooltip_text);

  pango_attr_list_unref (attrs);
}

static void
on_notify_regex_enabled (IdeEditorSearchBar      *self,
                         GParamSpec              *pspec,
                         GtkSourceSearchSettings *search_settings)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_SETTINGS (search_settings));

  check_replace_text (self);
}

static void
ide_editor_search_bar_grab_focus (GtkWidget *widget)
{
  IdeEditorSearchBar *self = (IdeEditorSearchBar *)widget;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  /* Be careful to not reselect or it can reselect the whole
   * entry text (causing next character to overwrite).
   */
  if (!gtk_widget_has_focus (GTK_WIDGET (self->search_entry)))
    gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static void
ide_editor_search_bar_bind_context (IdeEditorSearchBar     *self,
                                    GtkSourceSearchContext *context,
                                    DzlSignalGroup         *context_signals)
{
  GtkSourceBuffer *buffer;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));
  g_assert (DZL_IS_SIGNAL_GROUP (context_signals));

  self->quick_highlight_enabled = g_settings_get_boolean (self->quick_highlight_settings, "enabled");
  if (self->quick_highlight_enabled)
    g_settings_set_boolean (self->quick_highlight_settings, "enabled", FALSE);

  buffer = gtk_source_search_context_get_buffer (context);
  dzl_signal_group_set_target (self->buffer_signals, buffer);
}

static void
ide_editor_search_bar_unbind_context (IdeEditorSearchBar *self,
                                      DzlSignalGroup     *context_signals)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (DZL_IS_SIGNAL_GROUP (context_signals));

  if (self->quick_highlight_enabled)
    g_settings_set_boolean (self->quick_highlight_settings, "enabled", TRUE);

  if (self->buffer_signals != NULL)
    dzl_signal_group_set_target (self->buffer_signals, NULL);
}

static void
ide_editor_search_bar_bind_settings (IdeEditorSearchBar      *self,
                                     GtkSourceSearchSettings *settings,
                                     DzlSignalGroup          *settings_signals)
{
  g_autoptr(DzlPropertiesGroup) group = NULL;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_SETTINGS (settings));
  g_assert (DZL_IS_SIGNAL_GROUP (settings_signals));

  g_object_bind_property_full (self->search_entry, "text",
                               settings, "search-text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               maybe_escape_regex, pacify_null_text,
                               self, NULL);

  group = dzl_properties_group_new (G_OBJECT (settings));
  dzl_properties_group_add_all_properties (group);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "search-settings",
                                  G_ACTION_GROUP (group));
}

static void
search_entry_populate_popup (IdeEditorSearchBar *self,
                             GtkWidget          *widget,
                             GdTaggedEntry      *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_MENU (widget));
  g_assert (GTK_IS_ENTRY (entry));

  if (GTK_IS_MENU (widget))
    {
      DzlApplication *app = DZL_APPLICATION (IDE_APPLICATION_DEFAULT);
      GMenu *menu = dzl_application_get_menu_by_id (app, "ide-editor-search-bar-entry-menu");
      gtk_menu_shell_bind_model (GTK_MENU_SHELL (widget), G_MENU_MODEL (menu), NULL, TRUE);
    }
}

static void
search_entry_stop_search (IdeEditorSearchBar *self,
                          GtkSearchEntry     *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  g_signal_emit (self, signals [STOP_SEARCH], 0);
}

static void
search_entry_previous_match (IdeEditorSearchBar *self,
                             GtkSearchEntry     *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  dzl_gtk_widget_action (GTK_WIDGET (self),
                         "editor-view",
                         "move-previous-search-result",
                         NULL);
}

static void
search_entry_next_match (IdeEditorSearchBar *self,
                         GtkSearchEntry     *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  dzl_gtk_widget_action (GTK_WIDGET (self),
                         "editor-view",
                         "move-next-search-result",
                         NULL);
}

static void
search_entry_activate (IdeEditorSearchBar *self,
                       GtkSearchEntry     *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  dzl_gtk_widget_action (GTK_WIDGET (self),
                         "editor-view",
                         "activate-next-search-result",
                         NULL);
}

static void
ide_editor_search_bar_destroy (GtkWidget *widget)
{
  IdeEditorSearchBar *self = (IdeEditorSearchBar *)widget;

  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->context);
  g_clear_object (&self->context_signals);
  g_clear_object (&self->search_entry_tag);
  g_clear_object (&self->settings);
  g_clear_object (&self->settings_signals);
  g_clear_object (&self->quick_highlight_settings);

  GTK_WIDGET_CLASS (ide_editor_search_bar_parent_class)->destroy (widget);
}

static void
ide_editor_search_bar_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeEditorSearchBar *self = IDE_EDITOR_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_search_bar_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeEditorSearchBar *self = IDE_EDITOR_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_editor_search_bar_set_context (self, g_value_get_object (value));
      break;

    case PROP_SETTINGS:
      ide_editor_search_bar_set_settings (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_search_bar_class_init (IdeEditorSearchBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_editor_search_bar_get_property;
  object_class->set_property = ide_editor_search_bar_set_property;

  widget_class->destroy = ide_editor_search_bar_destroy;
  widget_class->grab_focus = ide_editor_search_bar_grab_focus;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The search context for locating matches",
                         GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings",
                         "Settings",
                         "The search settings for locating matches",
                         GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [STOP_SEARCH] =
    g_signal_new ("stop-search",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-search-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, case_sensitive);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_all_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, search_options);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, use_regex);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, whole_word);

  gtk_widget_class_set_css_name (widget_class, "ideeditorsearchbar");

  g_type_ensure (GD_TYPE_TAGGED_ENTRY);
}

static void
ide_editor_search_bar_init (IdeEditorSearchBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (self->search_entry,
                            "activate",
                            G_CALLBACK (search_entry_activate),
                            self);

  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  dzl_signal_group_connect_swapped (self->buffer_signals,
                                    "cursor-moved",
                                    G_CALLBACK (on_cursor_moved),
                                    self);

  self->context_signals = dzl_signal_group_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT);

  dzl_signal_group_connect_swapped (self->context_signals,
                                    "notify::occurrences-count",
                                    G_CALLBACK (on_notify_occurrences_count),
                                    self);

  dzl_signal_group_connect_swapped (self->context_signals,
                                    "notify::regex-error",
                                    G_CALLBACK (on_notify_regex_error),
                                    self);

  g_signal_connect_swapped (self->context_signals,
                            "bind",
                            G_CALLBACK (ide_editor_search_bar_bind_context),
                            self);

  g_signal_connect_swapped (self->context_signals,
                            "unbind",
                            G_CALLBACK (ide_editor_search_bar_unbind_context),
                            self);

  self->settings_signals = dzl_signal_group_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS);

  dzl_signal_group_connect_swapped (self->settings_signals,
                                    "notify::search-text",
                                    G_CALLBACK (on_notify_search_text),
                                    self);

  dzl_signal_group_connect_swapped (self->settings_signals,
                                    "notify::regex-enabled",
                                    G_CALLBACK (on_notify_regex_enabled),
                                    self);

  g_signal_connect_swapped (self->settings_signals,
                            "bind",
                            G_CALLBACK (ide_editor_search_bar_bind_settings),
                            self);

  dzl_widget_action_group_attach (self->search_entry, "entry");

  g_signal_connect_swapped (self->search_entry,
                            "populate-popup",
                            G_CALLBACK (search_entry_populate_popup),
                            self);

  g_signal_connect_swapped (self->search_entry,
                            "stop-search",
                            G_CALLBACK (search_entry_stop_search),
                            self);

  g_signal_connect_swapped (self->search_entry,
                            "previous-match",
                            G_CALLBACK (search_entry_previous_match),
                            self);

  g_signal_connect_swapped (self->search_entry,
                            "next-match",
                            G_CALLBACK (search_entry_next_match),
                            self);

  self->quick_highlight_settings =
    g_settings_new_with_path ("org.gnome.builder.extension-type",
                              "/org/gnome/builder/extension-types/quick-highlight-plugin/GbpQuickHighlightViewAddin/");

  _ide_editor_search_bar_init_actions (self);
  _ide_editor_search_bar_init_shortcuts (self);
}

GtkWidget *
ide_editor_search_bar_new (void)
{
  return g_object_new (IDE_TYPE_EDITOR_SEARCH_BAR, NULL);
}

void
ide_editor_search_bar_set_settings (IdeEditorSearchBar      *self,
                                    GtkSourceSearchSettings *settings)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_return_if_fail (!settings || GTK_SOURCE_IS_SEARCH_SETTINGS (settings));

  if (g_set_object (&self->settings, settings))
    {
      dzl_signal_group_set_target (self->settings_signals, settings);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SETTINGS]);
    }
}

void
ide_editor_search_bar_set_context (IdeEditorSearchBar     *self,
                                   GtkSourceSearchContext *context)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_return_if_fail (!context || GTK_SOURCE_IS_SEARCH_CONTEXT (context));


  if (g_set_object (&self->context, context))
    {
      dzl_signal_group_set_target (self->context_signals, context);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
    }
}

void
ide_editor_search_bar_set_search_text (IdeEditorSearchBar *self,
                                       const gchar        *search_text)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (search_text == NULL)
    search_text = "";

  if (self->settings != NULL)
    gtk_source_search_settings_set_search_text (self->settings, search_text);
}
