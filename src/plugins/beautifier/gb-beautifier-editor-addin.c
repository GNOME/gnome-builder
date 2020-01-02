/* gb-beautifier-editor-addin.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "beautifier-plugin"

#include "config.h"

#include <dazzle.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libide-editor.h>

#include "gb-beautifier-editor-addin.h"
#include "gb-beautifier-helper.h"
#include "gb-beautifier-private.h"
#include "gb-beautifier-process.h"

#define I_(s) g_intern_static_string(s)

static void editor_addin_iface_init (IdeEditorAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbBeautifierEditorAddin, gb_beautifier_editor_addin, IDE_TYPE_OBJECT, 0,
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
    {
      /* translators: %s is replaced with the error message */
      ide_object_warning (self, _("Beautifier plugin: %s"), error->message);
    }
}

static void
view_activate_beautify_action_cb (GSimpleAction *action,
                                  GVariant      *variant,
                                  gpointer       user_data)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)user_data;
  IdeEditorPage *view;
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
  if (view == NULL || !IDE_IS_EDITOR_PAGE (view))
    return;

  source_view = ide_editor_page_get_view (view);
  if (!GTK_SOURCE_IS_VIEW (source_view))
    {
      ide_object_warning (self, _("Beautifier Plugin: the view is not a GtkSourceView"));
      return;
    }

  param = g_variant_get_string (variant, NULL);
  if (g_strcmp0 (param, "none") == 0)
    {
      ide_object_warning (self, _("Beautifier Plugin: no default beautifier found"));
      return;
    }

  if (!gtk_text_view_get_editable (GTK_TEXT_VIEW (source_view)))
    {
      ide_object_warning (self, _("Beautifier Plugin: the buffer is not writable"));
      return;
    }

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  if (gtk_text_iter_equal (&begin, &end))
    {
      ide_object_warning (self, _("Beautifier Plugin: Nothing selected"));
      return;
    }

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
set_default_keybinding (GbBeautifierEditorAddin *self,
                        const gchar             *action_name)
{
  static const gchar *accel = "<primary><Alt>b";
  DzlShortcutController *controller;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (action_name != NULL);

  if (self->current_view == NULL)
    return;

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self->current_view));
  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.editor-view.beautifier-default",
                                              I_(accel),
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              action_name);
}

static void
setup_default_action (GbBeautifierEditorAddin *self,
                      IdeSourceView           *view)
{
  const gchar *lang_id;
  gchar *default_action_name;
  gboolean default_set = FALSE;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  lang_id = gb_beautifier_helper_get_lang_id (self, view);
  for (guint i = 0; i < self->entries->len; ++i)
    {
      GbBeautifierConfigEntry *entry;
      g_autofree gchar *param = NULL;

      entry = &g_array_index (self->entries, GbBeautifierConfigEntry, i);
      if (entry->is_default &&
          0 == g_strcmp0 (entry->lang_id, lang_id))
        {
          param = g_strdup_printf ("%i", i);
          default_action_name = g_strdup_printf ("view.beautify-default::%i", i);
          set_default_keybinding (self, default_action_name);
          default_set = TRUE;

          break;
        }
    }

  if (!default_set)
    set_default_keybinding (self, "view.beautify-default::none");
}

static void
view_populate_submenu (GbBeautifierEditorAddin *self,
                       IdeSourceView           *view,
                       GMenu                   *submenu,
                       GArray                  *entries)
{
  const gchar *lang_id;
  GMenu *default_menu;
  gboolean has_entries = FALSE;
  gboolean default_set = FALSE;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (view));
  g_assert (G_IS_MENU (submenu));
  g_assert (entries != NULL);

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
              g_menu_item_set_action_and_target (item, "view.beautify-default", "s", param);
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
  IdeEditorPage *view = (IdeEditorPage *)widget;
  IdeSourceView *source_view;
  GActionGroup *actions;
  GAction *action;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));

  if (!IDE_IS_EDITOR_PAGE (view))
    return;

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

  source_view = ide_editor_page_get_view (view);
  g_signal_connect_object (source_view,
                           "populate-popup",
                           G_CALLBACK (view_populate_popup),
                           self,
                           G_CONNECT_SWAPPED);

  if (self->has_default)
    setup_default_action (self, IDE_SOURCE_VIEW (source_view));
  else
    set_default_keybinding (self, "view.beautify-default::none");
}

static void
cleanup_view_cb (GtkWidget               *widget,
                 GbBeautifierEditorAddin *self)
{
  IdeEditorPage *view = (IdeEditorPage *)widget;
  GActionGroup *actions;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));

  if (!IDE_IS_EDITOR_PAGE (view))
    return;

  if (NULL != (actions = gtk_widget_get_action_group (GTK_WIDGET (view), "view")))
    {
      g_action_map_remove_action (G_ACTION_MAP (actions), "beautify");
      g_action_map_remove_action (G_ACTION_MAP (actions), "beautify-default");
    }

  /* TODO: if we close the view we are fine but if we desactivate the plugin, we should remove
   * the dzl shortcut and action mapping from the controler, dzl do not have this feature yet.
   */
}

static const DzlShortcutEntry beautifier_shortcut_entry[] = {
  { "org.gnome.builder.editor-view.beautifier-default",
    0,
    "<primary><Alt>b",
    N_("Editor shortcuts"),
    N_("Editing"),
    N_("Beautify the code"),
    N_("Trigger the default entry") },
};

static void
add_shortcut_window_entry (GbBeautifierEditorAddin *self)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             beautifier_shortcut_entry,
                                             G_N_ELEMENTS (beautifier_shortcut_entry),
                                             GETTEXT_PACKAGE);
}

static void
gb_beautifier_editor_addin_async_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  g_autoptr(GbBeautifierEditorAddin) self = (GbBeautifierEditorAddin *)object;
  GbBeautifierEntriesResult *ret;
  g_autoptr(GError) error = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (NULL == (ret = gb_beautifier_config_get_entries_finish (self, result, &error)))
    {
      /* translators: %s is replaced with the error message */
      ide_object_warning (self, _("Beautifier plugin: no valid entries, disabling: %s"), error->message);

      /* TODO: properly disable the plugin */
      return;
    }

  self->entries = g_steal_pointer (&ret->entries);
  self->has_default = ret->has_default;
  gb_beautifier_entries_result_free (ret);

  if (!self->has_default)
    set_default_keybinding (self, "view.beautify-default::none");

  ide_surface_foreach_page (IDE_SURFACE (self->editor), (GtkCallback)setup_view_cb, self);

  add_shortcut_window_entry (self);
}

static void
gb_beautifier_editor_addin_reap_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  DzlDirectoryReaper *reaper = (DzlDirectoryReaper *)object;
  g_autoptr(GbBeautifierEditorAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (DZL_IS_DIRECTORY_REAPER (reaper));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));

  if (!dzl_directory_reaper_execute_finish (reaper, result, &error))
    g_warning ("Failed to reap old beautifier data: %s", error->message);

  if (g_mkdir_with_parents (self->tmp_dir, 0750) != 0)
    {
      g_warning ("Failed to initialized the Beautifier plugin, can't write the temporary directory");
      return;
    }

  gb_beautifier_config_get_entries_async (self,
                                          &self->has_default,
                                          gb_beautifier_editor_addin_async_cb,
                                          NULL,
                                          g_object_ref (self));
}

static void
gb_beautifier_editor_addin_load (IdeEditorAddin       *addin,
                                 IdeEditorSurface *editor)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)addin;
  IdeWorkbench *workbench;
  g_autoptr(DzlDirectoryReaper) reaper = NULL;
  g_autoptr (GFile) tmp_file = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (editor));

  g_set_weak_pointer (&self->editor, editor);
  workbench = ide_widget_get_workbench (GTK_WIDGET (editor));
  self->context = ide_workbench_get_context (workbench);
  ide_object_set_context (IDE_OBJECT (self), self->context);

  if (self->tmp_dir == NULL)
    self->tmp_dir = ide_context_cache_filename (self->context, "beautifier", NULL);

  /* Cleanup old beautifier cache directory */
  reaper = dzl_directory_reaper_new ();
  tmp_file = g_file_new_for_path (self->tmp_dir);
  dzl_directory_reaper_add_directory (reaper, tmp_file, 0);

  dzl_directory_reaper_execute_async (reaper,
                                      NULL,
                                      gb_beautifier_editor_addin_reap_cb,
                                      g_object_ref (self));
}

static void
gb_beautifier_editor_addin_unload (IdeEditorAddin   *addin,
                                   IdeEditorSurface *editor)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)addin;
  GbBeautifierConfigEntry *entry;
  g_autoptr (GFile) tmp_file = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (editor));

  ide_surface_foreach_page (IDE_SURFACE (self->editor), (GtkCallback)cleanup_view_cb, self);
  if (self->entries != NULL)
    {
      for (guint i = 0; i < self->entries->len; i++)
        {
          entry = &g_array_index (self->entries, GbBeautifierConfigEntry, i);
          gb_beautifier_helper_config_entry_remove_temp_files (self, entry);
        }

      g_clear_pointer (&self->entries, g_array_unref);
    }

  g_clear_weak_pointer (&self->editor);
  if (self->tmp_dir != NULL)
    {
      tmp_file = g_file_new_for_path (self->tmp_dir);
      gb_beautifier_helper_remove_temp_for_file (self, tmp_file);
      g_clear_pointer (&self->tmp_dir, g_free);
    }

  self->context = NULL;
}

static void
gb_beautifier_editor_addin_page_set (IdeEditorAddin *addin,
                                     IdePage  *view)
{
  GbBeautifierEditorAddin *self = (GbBeautifierEditorAddin *)addin;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (!view || IDE_IS_PAGE (view));

  /* If there is currently a view set, and this is
   * a new view, then we want to clean it up.
   */

  if (!IDE_IS_EDITOR_PAGE (view))
    return;

  if (self->current_view != NULL)
    {
      if (view == self->current_view)
        return;

      cleanup_view_cb (GTK_WIDGET (self->current_view), self);
    }

  if (view != NULL)
    {
      g_set_weak_pointer (&self->current_view, view);
      setup_view_cb (GTK_WIDGET (view), self);
    }
  else
    self->current_view = NULL;
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
  iface->page_set = gb_beautifier_editor_addin_page_set;
}
