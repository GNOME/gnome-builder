/* ide-editor-search-bar.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-editor-search-bar"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-editor-private.h"
#include "ide-editor-search.h"
#include "ide-editor-search-bar.h"
#include "ide-source-view-private.h"

struct _IdeEditorSearchBar
{
  DzlBin                   parent_instance;

  DzlSignalGroup          *search_signals;
  DzlBindingGroup         *search_bindings;
  IdeEditorSearch         *search;

  IdeTaggedEntryTag       *search_entry_tag;

  GtkCheckButton          *case_sensitive;
  GtkButton               *replace_all_button;
  GtkButton               *replace_button;
  GtkSearchEntry          *replace_entry;
  IdeTaggedEntry          *search_entry;
  GtkGrid                 *search_options;
  GtkCheckButton          *use_regex;
  GtkCheckButton          *whole_word;
  GtkLabel                *search_text_error;
  GtkButton               *close_button;

  guint                    match_source;

  guint                    show_options : 1;
  guint                    replace_mode : 1;
};

enum {
  PROP_0,
  PROP_REPLACE_MODE,
  PROP_SHOW_OPTIONS,
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

  return self->replace_mode;
}

void
ide_editor_search_bar_set_replace_mode (IdeEditorSearchBar *self,
                                        gboolean            replace_mode)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  replace_mode = !!replace_mode;

  if (replace_mode != self->replace_mode)
    {
      self->replace_mode = replace_mode;
      gtk_widget_set_visible (GTK_WIDGET (self->replace_entry), replace_mode);
      gtk_widget_set_visible (GTK_WIDGET (self->replace_button), replace_mode);
      gtk_widget_set_visible (GTK_WIDGET (self->replace_all_button), replace_mode);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPLACE_MODE]);
    }
}

static gboolean
maybe_escape_regex (GBinding     *binding,
                    const GValue *from_value,
                    GValue       *to_value,
                    gpointer      user_data)
{
  IdeEditorSearchBar *self = user_data;
  const gchar *entry_text;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (from_value != NULL);
  g_assert (to_value != NULL);

  entry_text = g_value_get_string (from_value);

  if (entry_text == NULL)
    {
      g_value_set_static_string (to_value, "");
    }
  else
    {
      g_autofree gchar *unescaped = NULL;

      if (self->search != NULL && !ide_editor_search_get_regex_enabled (self->search))
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
  g_assert (G_VALUE_HOLDS_STRING (from_value));
  g_assert (G_VALUE_HOLDS_STRING (to_value));

  if (g_value_get_string (from_value) == NULL)
    g_value_set_static_string (to_value, "");
  else
    g_value_copy (from_value, to_value);

  return TRUE;
}

static void
ide_editor_search_bar_grab_focus (GtkWidget *widget)
{
  IdeEditorSearchBar *self = (IdeEditorSearchBar *)widget;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static void
search_entry_populate_popup (IdeEditorSearchBar *self,
                             GtkWidget          *widget,
                             IdeTaggedEntry     *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_MENU (widget));
  g_assert (GTK_IS_ENTRY (entry));

  if (GTK_IS_MENU (widget))
    {
      g_autoptr(DzlPropertiesGroup) group = NULL;
      GtkWidget *item;
      GtkWidget *sep;
      guint pos = 0;

      item = gtk_check_menu_item_new_with_label (_("Regular expressions"));
      gtk_actionable_set_action_name (GTK_ACTIONABLE (item), "search-settings.regex-enabled");
      gtk_menu_shell_insert (GTK_MENU_SHELL (widget), item, pos++);
      gtk_widget_show (item);

      item = gtk_check_menu_item_new_with_label (_("Case sensitive"));
      gtk_actionable_set_action_name (GTK_ACTIONABLE (item), "search-settings.case-sensitive");
      gtk_menu_shell_insert (GTK_MENU_SHELL (widget), item, pos++);
      gtk_widget_show (item);

      item = gtk_check_menu_item_new_with_label (_("Match whole word only"));
      gtk_actionable_set_action_name (GTK_ACTIONABLE (item), "search-settings.at-word-boundaries");
      gtk_menu_shell_insert (GTK_MENU_SHELL (widget), item, pos++);
      gtk_widget_show (item);

      sep = gtk_separator_menu_item_new ();
      gtk_menu_shell_insert (GTK_MENU_SHELL (widget), sep, pos++);
      gtk_widget_show (sep);

      if (self->search != NULL)
        {
          group = dzl_properties_group_new (G_OBJECT (self->search));
          dzl_properties_group_add_all_properties (group);
        }

      gtk_widget_insert_action_group (widget, "search-settings", G_ACTION_GROUP (group));
    }
}

static void
ide_editor_search_bar_real_stop_search (IdeEditorSearchBar *self)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
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

  if (self->search != NULL)
    {
      ide_editor_search_set_extend_selection (self->search, IDE_EDITOR_SEARCH_SELECT_NONE);
      ide_editor_search_set_repeat (self->search, 0);
      ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_BACKWARD);
    }
}

static void
search_entry_next_match (IdeEditorSearchBar *self,
                         GtkSearchEntry     *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  if (self->search != NULL)
    {
      ide_editor_search_set_extend_selection (self->search, IDE_EDITOR_SEARCH_SELECT_NONE);
      ide_editor_search_set_repeat (self->search, 0);
      ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_FORWARD);
    }
}

static void
search_entry_activate (IdeEditorSearchBar *self,
                       IdeTaggedEntry     *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (IDE_IS_TAGGED_ENTRY (entry));

  if (self->search != NULL)
    {
      GtkWidget *page;

      /* If the user is already on a match occurrence, then we don't
       * want to advance them to the next position (instead, we'll drop
       * them back in the editor at the current position.
       */
      if (ide_editor_search_get_match_position (self->search) == 0)
        {
          ide_editor_search_set_extend_selection (self->search, IDE_EDITOR_SEARCH_SELECT_NONE);
          ide_editor_search_set_repeat (self->search, 0);
          ide_editor_search_move (self->search, IDE_EDITOR_SEARCH_NEXT);
        }

      if ((page = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_EDITOR_PAGE)))
        {
          IdeSourceView *view = ide_editor_page_get_view (IDE_EDITOR_PAGE (page));

          _ide_source_view_clear_saved_mark (view);
        }
    }

  g_signal_emit (self, signals [STOP_SEARCH], 0);
}

static void
search_entry_changed (IdeEditorSearchBar *self,
                      IdeTaggedEntry     *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (IDE_IS_TAGGED_ENTRY (entry));

  /*
   * After the text has been changed, ask the IdeEditorSearch to see if
   * the search request is valid. Highlight the invalid range of text with
   * squigglies to denote what is broken.
   *
   * Also, add a tooltip to ensure that the user can figure out what they
   * broke in the process.
   */

  if (self->search != NULL)
    {
      g_autoptr(GError) error = NULL;
      PangoAttrList *attrs = NULL;
      guint begin = 0;
      guint end = 0;

      if (ide_editor_search_get_search_text_invalid (self->search, &begin, &end, &error))
        {
          PangoAttribute *attr;

          attrs = pango_attr_list_new ();

          attr = pango_attr_underline_new (PANGO_UNDERLINE_ERROR);
          attr->start_index = begin;
          attr->end_index = end;
          pango_attr_list_insert (attrs, attr);

          attr = pango_attr_underline_color_new (65535, 0, 0);
          pango_attr_list_insert (attrs, attr);
        }

      gtk_entry_set_attributes (GTK_ENTRY (entry), attrs);
      gtk_label_set_label (self->search_text_error,
                           error ? error->message : NULL);
      gtk_widget_set_visible (GTK_WIDGET (self->search_text_error), error != NULL);

      g_clear_pointer (&attrs, pango_attr_list_unref);
    }
}

static gboolean
update_match_positions (gpointer user_data)
{
  IdeEditorSearchBar *self = user_data;
  GtkStyleContext *style;
  g_autofree gchar *str = NULL;
  guint count;
  guint pos;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  self->match_source = 0;

  count = ide_editor_search_get_match_count (self->search);
  pos = ide_editor_search_get_match_position (self->search);

  if (count > 0)
    str = g_strdup_printf (_("%u of %u"), pos, count);

  if (str == NULL)
    {
      if (self->search_entry_tag != NULL)
        {
          ide_tagged_entry_remove_tag (self->search_entry, self->search_entry_tag);
          g_clear_object (&self->search_entry_tag);
        }
    }
  else
    {
      if (self->search_entry_tag == NULL)
        {
          self->search_entry_tag = ide_tagged_entry_tag_new ("");
          ide_tagged_entry_add_tag (self->search_entry, self->search_entry_tag);
          ide_tagged_entry_tag_set_style (self->search_entry_tag, "search-occurrences-tag");
        }

      ide_tagged_entry_tag_set_label (self->search_entry_tag, str);
    }

  style = gtk_widget_get_style_context (GTK_WIDGET(self->search_entry));

  if (count == 0 && gtk_entry_get_text_length (GTK_ENTRY (self->search_entry)) > 0)
    gtk_style_context_add_class (style, GTK_STYLE_CLASS_ERROR);
  else
    gtk_style_context_remove_class(style, GTK_STYLE_CLASS_ERROR);

  return G_SOURCE_REMOVE;
}

static void
ide_editor_search_bar_notify_match (IdeEditorSearchBar *self,
                                    GParamSpec         *pspec,
                                    IdeEditorSearch    *search)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (IDE_IS_EDITOR_SEARCH (search));

  /* Queue an update to our match positions, but only
   * do so after returning to the main loop to avoid
   * doing lots of extra work during heavy scanning.
   */
  if (self->match_source == 0)
    self->match_source = gdk_threads_add_idle_full (G_PRIORITY_LOW,
                                                    update_match_positions,
                                                    g_object_ref (self),
                                                    g_object_unref);
}

static void
on_close_button_clicked (IdeEditorSearchBar *self,
                         GtkButton          *button)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_BUTTON (button));

  g_signal_emit (self, signals [STOP_SEARCH], 0);

  IDE_EXIT;
}

static void
ide_editor_search_bar_destroy (GtkWidget *widget)
{
  IdeEditorSearchBar *self = (IdeEditorSearchBar *)widget;

  dzl_clear_source (&self->match_source);

  g_clear_object (&self->search_signals);
  g_clear_object (&self->search_bindings);
  g_clear_object (&self->search);
  g_clear_object (&self->search_entry_tag);

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
    case PROP_REPLACE_MODE:
      g_value_set_boolean (value, ide_editor_search_bar_get_replace_mode (self));
      break;

    case PROP_SHOW_OPTIONS:
      g_value_set_boolean (value, ide_editor_search_bar_get_show_options (self));
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
    case PROP_REPLACE_MODE:
      ide_editor_search_bar_set_replace_mode (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_OPTIONS:
      ide_editor_search_bar_set_show_options (self, g_value_get_boolean (value));
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

  properties [PROP_REPLACE_MODE] =
    g_param_spec_boolean ("replace-mode", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties [PROP_SHOW_OPTIONS] =
    g_param_spec_boolean ("show-options", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [STOP_SEARCH] =
    g_signal_new_class_handler ("stop-search",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_editor_search_bar_real_stop_search),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ui/ide-editor-search-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, case_sensitive);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, close_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_all_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, search_options);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, search_text_error);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, use_regex);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, whole_word);

  gtk_widget_class_set_css_name (widget_class, "ideeditorsearchbar");
}

static void
ide_editor_search_bar_init (IdeEditorSearchBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->search_signals = dzl_signal_group_new (IDE_TYPE_EDITOR_SEARCH);

  dzl_signal_group_connect_swapped (self->search_signals,
                                    "notify::match-count",
                                    G_CALLBACK (ide_editor_search_bar_notify_match),
                                    self);

  dzl_signal_group_connect_swapped (self->search_signals,
                                    "notify::match-position",
                                    G_CALLBACK (ide_editor_search_bar_notify_match),
                                    self);

  self->search_bindings = dzl_binding_group_new ();

  dzl_binding_group_bind_full (self->search_bindings, "search-text",
                               self->search_entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               maybe_escape_regex, pacify_null_text, self, NULL);

  dzl_binding_group_bind_full (self->search_bindings, "replacement-text",
                               self->replace_entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               pacify_null_text, pacify_null_text, NULL, NULL);

  dzl_binding_group_bind (self->search_bindings, "regex-enabled",
                          self->use_regex, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  dzl_binding_group_bind (self->search_bindings, "at-word-boundaries",
                          self->whole_word, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  dzl_binding_group_bind (self->search_bindings, "case-sensitive",
                          self->case_sensitive, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_signal_connect_swapped (self->search_entry,
                            "activate",
                            G_CALLBACK (search_entry_activate),
                            self);

  g_signal_connect_data (self->search_entry,
                         "changed",
                         G_CALLBACK (search_entry_changed),
                         self, NULL,
                         G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_swapped (self->search_entry,
                            "populate-popup",
                            G_CALLBACK (search_entry_populate_popup),
                            self);

  g_signal_connect_swapped (self->search_entry,
                            "stop-search",
                            G_CALLBACK (search_entry_stop_search),
                            self);
  g_signal_connect_swapped (self->replace_entry,
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

  g_signal_connect_swapped (self->close_button,
                            "clicked",
                            G_CALLBACK (on_close_button_clicked),
                            self);

  _ide_editor_search_bar_init_shortcuts (self);
}

gboolean
ide_editor_search_bar_get_show_options (IdeEditorSearchBar *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self), FALSE);

  return self->show_options;
}

void
ide_editor_search_bar_set_show_options (IdeEditorSearchBar *self,
                                        gboolean            show_options)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  show_options = !!show_options;

  if (self->show_options != show_options)
    {
      self->show_options = show_options;
      gtk_widget_set_visible (GTK_WIDGET (self->search_options), show_options);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_OPTIONS]);
    }
}

/**
 * ide_editor_search_bar_get_search:
 * @self: a #IdeEditorSearchBar
 *
 * Gets the #IdeEditorSearch used by the search bar.
 *
 * Returns: (transfer none) (nullable): An #IdeEditorSearch or %NULL.
 *
 * Since: 3.32
 */
IdeEditorSearch *
ide_editor_search_bar_get_search (IdeEditorSearchBar *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self), NULL);

  return self->search;
}

void
ide_editor_search_bar_set_search (IdeEditorSearchBar *self,
                                  IdeEditorSearch    *search)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (g_set_object (&self->search, search))
    {
      dzl_signal_group_set_target (self->search_signals, search);
      dzl_binding_group_set_source (self->search_bindings, search);
    }
}
