/* gb-beautifier-editor-addin.c
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <ide.h>

#include "gb-beautifier-private.h"
#include "gb-beautifier-helper.h"
#include "gb-beautifier-process.h"

#include "gb-beautifier-editor-addin.h"

static void editor_addin_iface_init (IdeEditorAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbBeautifierEditorAddin, gb_beautifier_editor_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN, editor_addin_iface_init))

static void
process_launch_async_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)object;
  g_autoptr(GError) error = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!gb_beautifier_process_launch_finish (self, result, &error))
    g_warning ("\"%s\"", error->message);
}

static void
view_activate_beautify_action_cb (GSimpleAction *action,
                                  GVariant      *variant,
                                  gpointer       user_data)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)user_data;
  IdeEditorView *view;
  IdeSourceView *source_view;
  GtkTextBuffer *buffer;
  GCancellable *cancellable;
  GbBeautifierConfigEntry *entry;
  const gchar *param;
  GtkTextIter begin;
  GtkTextIter end;
  guint64 index;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));
  g_assert (G_IS_SIMPLE_ACTION (action));

  view = g_object_get_data (G_OBJECT (action), "gb-beautifier-editor-addin");
  if (view == NULL || !IDE_IS_EDITOR_VIEW (view))
    return;

  source_view = ide_editor_view_get_view (view);
  if (!GTK_SOURCE_IS_VIEW (source_view))
    {
      g_warning ("Beautifier Plugin: the view is not a GtkSourceView");
      return;
    }

  if (!gtk_text_view_get_editable (GTK_TEXT_VIEW (source_view)))
    {
      g_warning ("Beautifier Plugin: the buffer is not writable");
      return;
    }

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  if (gtk_text_iter_equal (&begin, &end))
    {
      g_warning ("Beautifier Plugin: Nothing selected");
      return;
    }

  param = g_variant_get_string (variant, NULL);
  index = g_ascii_strtod (param, NULL);
  entry = &g_array_index (self->entries, GbBeautifierConfigEntry, index);
  g_assert (entry != NULL);

  cancellable = g_cancellable_new ();
  gb_beautifier_process_launch_async (self,
                                      source_view,
                                      &begin,
                                      &end,
                                      entry,
                                      process_launch_async_cb,
                                      cancellable,
                                      NULL);
}

static void
view_populate_submenu (GbBeautifierEditorAddin *self,
                       IdeSourceView           *view,
                       GMenu                   *submenu,
                       GArray                  *entries)
{
  const gchar *lang_id;
  GtkApplication *app;
  gchar *default_action_name;
  GMenu *default_menu;
  gboolean has_entries = FALSE;
  gboolean default_set = FALSE;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (G_IS_MENU (submenu));
  g_assert (entries != NULL);

  app = GTK_APPLICATION (g_application_get_default ());

  if (NULL != (default_action_name = g_object_get_data (G_OBJECT (view), "gb-beautifier-default-action")))
    {
      gtk_application_set_accels_for_action (app, default_action_name, (const gchar*[]) {NULL});
      g_object_set_data_full (G_OBJECT (view), "gb-beautifier-default-action", NULL, g_free);
    }

  default_menu = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT, "gb-beautify-default-section");
  g_menu_remove_all (default_menu);

  lang_id = gb_beautifier_helper_get_lang_id (self, view);
  for (guint i = 0; i < entries->len; ++i)
    {
      GbBeautifierConfigEntry *entry;
      g_autoptr(GMenuItem) item = NULL;
      g_autofree gchar *param = NULL;

      entry = &g_array_index (entries, GbBeautifierConfigEntry, i);
      if (0 == g_strcmp0 (entry->lang_id, lang_id))
        {
          param = g_strdup_printf ("%i", i);
          if (!default_set && entry->is_default)
            {
              item = g_menu_item_new (entry->name, NULL);
              default_action_name = g_strdup_printf ("view.beautify-default::%i", i);
              g_object_set_data_full (G_OBJECT (view),
                                      "gb-beautifier-default-action",
                                      default_action_name,
                                      g_free);
              g_menu_item_set_action_and_target (item, "view.beautify-default", "s", param);
              gtk_application_set_accels_for_action (app,
                                                     default_action_name,
                                                     (const gchar*[]) {"<Control><Alt>b", NULL});

              g_menu_append_item (default_menu, item);
              default_set = TRUE;
            }
          else
            {
              item = g_menu_item_new (entry->name, NULL);
              g_menu_item_set_action_and_target (item, "view.beautify", "s", param);
              g_menu_append_item (submenu, item);
            }

          has_entries = TRUE;
        }
    }

  if (!has_entries)
    {
      g_autofree gchar *label = NULL;
      g_autoptr(GMenuItem) item = NULL;
      GtkTextBuffer *buffer;
      GtkSourceLanguage *source_language;
      const gchar *language;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      source_language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
      if (source_language != NULL &&
          NULL != (language = gtk_source_language_get_name (source_language)))
        label = g_strdup_printf (_("No beautifier available for “%s”"), language);
      else
        label = g_strdup_printf (_("No beautifier available"));

      item = g_menu_item_new (label, NULL);
      /* Set the item to a disable state */
      g_menu_item_set_action_and_target (item, "view.beautify-menu", NULL);
      g_menu_append_item (submenu, item);
    }
}

static void
view_populate_popup (GbBeautifierEditorAddin *self,
                     GtkWidget               *popup,
                     IdeSourceView           *source_view)
{
  GMenu *submenu;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (GTK_IS_WIDGET (popup));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  submenu = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT, "gb-beautify-profiles-section");
  g_menu_remove_all (submenu);
  view_populate_submenu (self, source_view, submenu, self->entries);
}

static GActionEntry GbBeautifierActions [] = {
  {"beautify", view_activate_beautify_action_cb, "s"},
  {"beautify-default", view_activate_beautify_action_cb, "s"},
};

static void
setup_view_cb (GtkWidget               *widget,
               GbBeautifierEditorAddin *self)
{
  IdeEditorView *view = (IdeEditorView *)widget;
  IdeSourceView *source_view;
  GActionGroup *actions;
  GAction *action;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  actions = gtk_widget_get_action_group (GTK_WIDGET (view), "view");
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   GbBeautifierActions,
                                   G_N_ELEMENTS (GbBeautifierActions),
                                   self);

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "beautify");
  g_object_set_data (G_OBJECT (action), "gb-beautifier-editor-addin", view);
  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "beautify-default");
  g_object_set_data (G_OBJECT (action), "gb-beautifier-editor-addin", view);

  g_object_set_data (G_OBJECT (view), "gb-beautifier-editor-addin", self);

  source_view = ide_editor_view_get_view (view);
  g_signal_connect_object (source_view,
                           "populate-popup",
                           G_CALLBACK (view_populate_popup),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
cleanup_view_cb (GtkWidget               *widget,
                 GbBeautifierEditorAddin *self)
{
  IdeEditorView *view = (IdeEditorView *)widget;
  GActionGroup *actions;
  GtkApplication *app;
  gchar *default_action_name;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  app = GTK_APPLICATION (g_application_get_default ());

  if (NULL != (actions = gtk_widget_get_action_group (GTK_WIDGET (view), "view")))
    {
      g_action_map_remove_action (G_ACTION_MAP (actions), "beautify");
      g_action_map_remove_action (G_ACTION_MAP (actions), "beautify-default");
    }

  g_object_set_data (G_OBJECT (view), "gb-beautifier-editor-addin", NULL);
  if (NULL != (default_action_name = g_object_get_data (G_OBJECT (view), "gb-beautifier-default-action")))
    {
      gtk_application_set_accels_for_action (app, default_action_name, (const gchar*[]) {NULL});
      g_object_set_data_full (G_OBJECT (view), "gb-beautifier-default-action", NULL, g_free);
    }
}

static void
gb_beautifier_editor_addin_load (IdeEditorAddin       *addin,
                                 IdeEditorPerspective *editor)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)addin;
  IdeWorkbench *workbench;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  ide_set_weak_pointer (&self->editor, editor);
  workbench = ide_widget_get_workbench (GTK_WIDGET (editor));
  self->context = ide_workbench_get_context (workbench);
  self->entries = gb_beautifier_config_get_entries (self);

  ide_perspective_views_foreach (IDE_PERSPECTIVE (self->editor), (GtkCallback)setup_view_cb, self);
}

static void
gb_beautifier_editor_addin_unload (IdeEditorAddin       *addin,
                                   IdeEditorPerspective *editor)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)addin;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (editor));

  ide_perspective_views_foreach (IDE_PERSPECTIVE (self->editor), (GtkCallback)cleanup_view_cb, self);
  if (self->entries != NULL)
    {
      g_array_free (self->entries, TRUE);
      self->entries = NULL;
    }

  ide_clear_weak_pointer (&self->editor);
  self->context = NULL;
}

static void
gb_beautifier_editor_addin_view_set (IdeEditorAddin *addin,
                                     IdeLayoutView  *view)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)addin;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  /* If there is currently a view set, and this is
   * a new view, then we want to clean it up.
   */

  if (self->current_view != NULL)
    {
      if (view == IDE_LAYOUT_VIEW (self->current_view))
        return;

      if (view != NULL)
        cleanup_view_cb (GTK_WIDGET (view), self);
    }

  self->current_view = view;
  if (view != NULL)
    setup_view_cb (GTK_WIDGET (view), self);
}

static void
gb_beautifier_editor_addin_class_init (GbBeautifierEditorAddinClass *klass)
{
}

static void
gb_beautifier_editor_addin_init (GbBeautifierEditorAddin *self)
{
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = gb_beautifier_editor_addin_load;
  iface->unload = gb_beautifier_editor_addin_unload;
  iface->view_set = gb_beautifier_editor_addin_view_set;
}
