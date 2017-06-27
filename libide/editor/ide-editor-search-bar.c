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
#include <libgd/gd-tagged-entry.h>

#include "ide-macros.h"

#include "editor/ide-editor-search-bar.h"

struct _IdeEditorSearchBar
{
  DzlBin                   parent_instance;

  /* Owned references */
  GtkSourceSearchSettings *search_settings;
  GtkSourceSearchContext  *search_context;
  DzlSignalGroup          *search_context_signals;
  GdTaggedEntryTag        *search_entry_tag;

  /* Weak pointers */
  IdeBuffer               *buffer;
  IdeSourceView           *view;

  /* Template widgets */
  GtkCheckButton          *case_sensitive;
  GtkButton               *close_button;
  GtkButton               *replace_all_button;
  GtkButton               *replace_button;
  GtkSearchEntry          *replace_entry;
  GdTaggedEntry           *search_entry;
  GtkGrid                 *search_options;
  GtkCheckButton          *use_regex;
  GtkCheckButton          *whole_word;
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_VIEW,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorSearchBar, ide_editor_search_bar, DZL_TYPE_BIN)

static GParamSpec *properties [N_PROPS];

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

      if (!gtk_source_search_settings_get_regex_enabled (self->search_settings))
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
  GtkTextBuffer *buffer;
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

  if (self->search_context == NULL ||
      self->view == NULL ||
      self->buffer == NULL ||
      self->search_settings == NULL)
    return;

  buffer = GTK_TEXT_BUFFER (self->buffer);

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

  /* Gather enough info to determine if Replace or Replace All would make sense */
  search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
  pos = gtk_source_search_context_get_occurrence_position (self->search_context, &begin, &end);
  count = gtk_source_search_context_get_occurrences_count (self->search_context);
  regex_error = gtk_source_search_context_get_regex_error (self->search_context);
  replace_regex_valid = gtk_source_search_settings_get_regex_enabled (self->search_settings) ?
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

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "search-entry", "replace",
                             "enabled", enable_replace,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "search-entry", "replace-all",
                             "enabled", enable_replace_all,
                             NULL);
}

static void
on_notify_search_text (IdeEditorSearchBar      *self,
                       GParamSpec              *pspec,
                       GtkSourceSearchSettings *search_settings)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_SETTINGS (search_settings));

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
                                     "gb-search-entry-occurrences-tag");
    }

  gd_tagged_entry_tag_set_label (self->search_entry_tag, text);
}

static void
update_search_position_label (IdeEditorSearchBar *self)
{
  g_autofree gchar *text = NULL;
  GtkStyleContext *context;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  const gchar *search_text;
  gint count;
  gint pos;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (self->buffer == NULL || self->search_context == NULL)
    return;

  buffer = GTK_TEXT_BUFFER (self->buffer);

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  pos = gtk_source_search_context_get_occurrence_position (self->search_context, &begin, &end);
  count = gtk_source_search_context_get_occurrences_count (self->search_context);

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
  g_assert (self->search_settings != NULL);

  if (self->search_context == NULL)
    return;

  /*
   * If the replace expression is invalid, add a white squiggly underline;
   * otherwise remove it. Also set the error message to the tooltip text
   * so that the user can get some info on the error.
   */
  if (gtk_source_search_settings_get_regex_enabled (self->search_settings))
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
ide_editor_search_bar_finalize (GObject *object)
{
  IdeEditorSearchBar *self = (IdeEditorSearchBar *)object;

  ide_clear_weak_pointer (&self->buffer);
  ide_clear_weak_pointer (&self->view);

  g_clear_object (&self->search_context);
  g_clear_object (&self->search_settings);
  g_clear_object (&self->search_context_signals);
  g_clear_object (&self->search_entry_tag);

  G_OBJECT_CLASS (ide_editor_search_bar_parent_class)->finalize (object);
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
    case PROP_BUFFER:
      g_value_set_object (value, ide_editor_search_bar_get_buffer (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, ide_editor_search_bar_get_view (self));
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
    case PROP_BUFFER:
      ide_editor_search_bar_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_VIEW:
      ide_editor_search_bar_set_view (self, g_value_get_object (value));
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

  object_class->finalize = ide_editor_search_bar_finalize;
  object_class->get_property = ide_editor_search_bar_get_property;
  object_class->set_property = ide_editor_search_bar_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/ide-editor-search-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, case_sensitive);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, close_button);
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
  g_autoptr(GSimpleActionGroup) group = NULL;
  static const gchar *proxy_names[] = {
    "case-sensitive",
    "at-word-boundaries",
    "regex-enabled",
    "wrap-around",
  };

  gtk_widget_init_template (GTK_WIDGET (self));

  self->search_settings = gtk_source_search_settings_new ();

  g_object_bind_property_full (self->search_entry, "text",
                               self->search_settings, "search-text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               maybe_escape_regex, pacify_null_text,
                               self, NULL);

  self->search_context_signals = dzl_signal_group_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT);

  dzl_signal_group_connect_swapped (self->search_context_signals,
                                    "notify::occurrences-count",
                                    G_CALLBACK (on_notify_occurrences_count),
                                    self);

  dzl_signal_group_connect_swapped (self->search_context_signals,
                                    "notify::regex-error",
                                    G_CALLBACK (on_notify_regex_error),
                                    self);

  g_signal_connect_object (self->search_settings,
                           "notify::search-text",
                           G_CALLBACK (on_notify_search_text),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_settings,
                           "notify::regex-enabled",
                           G_CALLBACK (on_notify_regex_enabled),
                           self,
                           G_CONNECT_SWAPPED);

  group = g_simple_action_group_new ();

  for (guint i = 0; i < G_N_ELEMENTS (proxy_names); i++)
    {
      g_autoptr(GPropertyAction) action = NULL;
      const gchar *name = proxy_names[i];

      action = g_property_action_new (name, self->search_settings, name);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
    }

  gtk_widget_insert_action_group (GTK_WIDGET (self), "search-entry", G_ACTION_GROUP (group));
}

GtkWidget *
ide_editor_search_bar_new (void)
{
  return g_object_new (IDE_TYPE_EDITOR_SEARCH_BAR, NULL);
}

/**
 * ide_editor_search_bar_get_buffer:
 * @self: a #IdeEditorSearchBar
 *
 * Gets the buffer used by the search bar.
 *
 * Returns: (nullable) (transfer none): An #IdeBuffer or %NULL
 *
 * Since: 3.26
 */
IdeBuffer *
ide_editor_search_bar_get_buffer (IdeEditorSearchBar *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self), NULL);

  return self->buffer;
}

/**
 * ide_editor_search_bar_set_buffer:
 * @self: a #IdeEditorSearchBar
 *
 * Sets the buffer used by the search bar.
 *
 * Since: 3.26
 */
void
ide_editor_search_bar_set_buffer (IdeEditorSearchBar *self,
                                  IdeBuffer          *buffer)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_return_if_fail (!buffer || IDE_IS_BUFFER (buffer));

  if (ide_set_weak_pointer (&self->buffer, buffer))
    {
      g_clear_object (&self->search_context);
      dzl_signal_group_set_target (self->search_context_signals, NULL);

      if (buffer != NULL)
        {
          self->search_context = gtk_source_search_context_new (GTK_SOURCE_BUFFER (buffer),
                                                                self->search_settings);
          dzl_signal_group_set_target (self->search_context_signals, self->search_context);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUFFER]);
    }
}

/**
 * ide_editor_search_bar_get_view:
 * @self: a #IdeEditorSearchBar
 *
 * Gets the view used by the search bar.
 *
 * Returns: (nullable) (transfer none): An #IdeBuffer or %NULL
 *
 * Since: 3.26
 */
IdeSourceView *
ide_editor_search_bar_get_view (IdeEditorSearchBar *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self), NULL);

  return self->view;
}

/**
 * ide_editor_search_bar_set_view:
 * @self: a #IdeEditorSearchBar
 *
 * Sets the view used by the search bar.
 *
 * Since: 3.26
 */
void
ide_editor_search_bar_set_view (IdeEditorSearchBar *self,
                                IdeSourceView      *view)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_return_if_fail (!view || IDE_IS_SOURCE_VIEW (view));

  if (ide_set_weak_pointer (&self->view, view))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VIEW]);
}
