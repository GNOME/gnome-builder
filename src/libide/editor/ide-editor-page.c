/* ide-editor-page.c
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-page"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-threading.h>

#include "ide-editor-page-addin.h"
#include "ide-editor-page-private.h"

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_GUTTER,
  PROP_VIEW,
  N_PROPS
};

G_DEFINE_TYPE (IdeEditorPage, ide_editor_page, IDE_TYPE_PAGE)

static GParamSpec *properties [N_PROPS];

static void
ide_editor_page_modified_changed_cb (IdeEditorPage *self,
                                     IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  panel_widget_set_modified (PANEL_WIDGET (self),
                             gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)));

  IDE_EXIT;
}

static void
ide_editor_page_style_scheme_changed_cb (IdeEditorPage *self,
                                         GParamSpec    *pspec,
                                         IdeBuffer     *buffer)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->gutter != NULL)
    ide_gutter_style_changed (self->gutter);
}

static void
ide_editor_page_set_buffer (IdeEditorPage *self,
                            IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (g_set_object (&self->buffer, buffer))
    {
      ide_buffer_hold (buffer);

      gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->view), GTK_TEXT_BUFFER (buffer));

      g_signal_connect_object (buffer,
                               "modified-changed",
                               G_CALLBACK (ide_editor_page_modified_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "notify::file-settings",
                               G_CALLBACK (_ide_editor_page_settings_reload),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (buffer,
                               "notify::style-scheme",
                               G_CALLBACK (ide_editor_page_style_scheme_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_object_bind_property (buffer, "title",
                              self, "title",
                              G_BINDING_SYNC_CREATE);

      ide_editor_page_modified_changed_cb (self, buffer);
      _ide_editor_page_settings_init (self);
    }

  IDE_EXIT;
}

static gboolean
ide_editor_page_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (IDE_EDITOR_PAGE (widget)->view));
}

static void
ide_editor_page_focus_enter_cb (IdeEditorPage           *self,
                                GtkEventControllerFocus *controller)
{
  g_autofree char *title = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (controller));

  title = ide_buffer_dup_title (self->buffer);
  g_debug ("Keyboard focus entered page \"%s\"", title);

  ide_page_mark_used (IDE_PAGE (self));

  IDE_EXIT;
}

static void
ide_editor_page_notify_frame_set (IdeExtensionSetAdapter *set,
                                  PeasPluginInfo         *plugin_info,
                                  PeasExtension          *exten,
                                  gpointer                user_data)
{
  IdeFrame *frame = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_FRAME (frame));

  ide_editor_page_addin_frame_set (addin, frame);
}

static void
ide_editor_page_addin_added (IdeExtensionSetAdapter *set,
                             PeasPluginInfo         *plugin_info,
                             PeasExtension          *exten,
                             gpointer                user_data)
{
  IdeEditorPage *self = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_editor_page_addin_load (addin, self);
}

static void
ide_editor_page_addin_removed (IdeExtensionSetAdapter *set,
                               PeasPluginInfo         *plugin_info,
                               PeasExtension          *exten,
                               gpointer                user_data)
{
  IdeEditorPage *self = user_data;
  IdeEditorPageAddin *addin = (IdeEditorPageAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_EDITOR_PAGE_ADDIN (addin));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_editor_page_addin_unload (addin, self);
}

static void
ide_editor_page_root (GtkWidget *widget)
{
  IdeEditorPage *self = (IdeEditorPage *)widget;
  IdeContext *context;
  GtkWidget *frame;

  IDE_ENTRY;

  GTK_WIDGET_CLASS (ide_editor_page_parent_class)->root (widget);

  context = ide_widget_get_context (widget);
  frame = gtk_widget_get_ancestor (widget, IDE_TYPE_FRAME);

  if (self->addins == NULL && context != NULL)
    {
      self->addins = ide_extension_set_adapter_new (IDE_OBJECT (context),
                                                    peas_engine_get_default (),
                                                    IDE_TYPE_EDITOR_PAGE_ADDIN,
                                                    "Editor-Page-Languages",
                                                    ide_buffer_get_language_id (self->buffer));

      g_signal_connect (self->addins,
                        "extension-added",
                        G_CALLBACK (ide_editor_page_addin_added),
                        self);

      g_signal_connect (self->addins,
                        "extension-removed",
                        G_CALLBACK (ide_editor_page_addin_removed),
                        self);

      ide_extension_set_adapter_foreach (self->addins,
                                         ide_editor_page_addin_added,
                                         self);
    }

  if (self->addins != NULL && frame != NULL)
    ide_extension_set_adapter_foreach (self->addins,
                                       ide_editor_page_notify_frame_set,
                                       frame);

  IDE_EXIT;
}

static IdePage *
ide_editor_page_create_split (IdePage *page)
{
  IdeEditorPage *self = (IdeEditorPage *)page;
  GtkWidget *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  ret = ide_editor_page_new (self->buffer);

  IDE_RETURN (IDE_PAGE (ret));
}

static GFile *
ide_editor_page_get_file_or_directory (IdePage *page)
{
  GFile *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_PAGE (page));

  ret = ide_buffer_get_file (IDE_EDITOR_PAGE (page)->buffer);

  if (ret != NULL)
    g_object_ref (ret);

  IDE_RETURN (ret);
}

static void
ide_editor_page_dispose (GObject *object)
{
  IdeEditorPage *self = (IdeEditorPage *)object;

  ide_editor_page_set_gutter (self, NULL);

  ide_clear_and_destroy_object (&self->addins);

  g_clear_object (&self->buffer_file_settings);
  g_clear_object (&self->view_file_settings);

  if (self->buffer != NULL)
    {
      ide_buffer_release (self->buffer);
      g_clear_object (&self->buffer);
    }

  G_OBJECT_CLASS (ide_editor_page_parent_class)->dispose (object);
}

static void
ide_editor_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_editor_page_get_buffer (self));
      break;

    case PROP_GUTTER:
      g_value_set_object (value, ide_editor_page_get_gutter (self));
      break;

    case PROP_VIEW:
      g_value_set_object (value, ide_editor_page_get_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeEditorPage *self = IDE_EDITOR_PAGE (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_editor_page_set_buffer (self, g_value_get_object (value));
      break;

    case PROP_GUTTER:
      ide_editor_page_set_gutter (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_page_class_init (IdeEditorPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdePageClass *page_class = IDE_PAGE_CLASS (klass);

  object_class->dispose = ide_editor_page_dispose;
  object_class->get_property = ide_editor_page_get_property;
  object_class->set_property = ide_editor_page_set_property;

  widget_class->grab_focus = ide_editor_page_grab_focus;
  widget_class->root = ide_editor_page_root;

  page_class->get_file_or_directory = ide_editor_page_get_file_or_directory;
  page_class->create_split = ide_editor_page_create_split;

  /**
   * IdeEditorPage:buffer:
   *
   * The #IdeBuffer that is displayed within the #IdeSourceView.
   */
  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer to be displayed within the page",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeEditorPage:gutter:
   *
   * The "gutter" property contains an #IdeGutter or %NULL, which is a
   * specialized renderer for the sourceview which can bring together a number
   * of types of content which needs to be displayed, in a single renderer.
   */
  properties [PROP_GUTTER] =
    g_param_spec_object ("gutter",
                         "Gutter",
                         "The primary gutter renderer in the left gutter window",
                         IDE_TYPE_GUTTER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeEditorPage:view:
   *
   * The #IdeSourceView contained within the page.
   */
  properties [PROP_VIEW] =
    g_param_spec_object ("view",
                         "View",
                         "The view displaying the buffer",
                         IDE_TYPE_SOURCE_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-editor-page.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, map);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, map_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, scroller);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorPage, view);
  gtk_widget_class_bind_template_callback (widget_class, ide_editor_page_focus_enter_cb);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_s, GDK_CONTROL_MASK, "page.save", NULL);

  _ide_editor_page_class_actions_init (klass);
}

static void
ide_editor_page_init (IdeEditorPage *self)
{
  GtkSourceGutterRenderer *renderer;
  GtkSourceGutter *gutter;
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_page_set_can_split (IDE_PAGE (self), TRUE);

  /* Load menus for editor pages */
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "ide-editor-page-menu");
  panel_widget_set_menu_model (PANEL_WIDGET (self), G_MENU_MODEL (menu));

  /* Add menus to source view */
  menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "ide-source-view-popup-menu");
  ide_source_view_append_menu (self->view, G_MENU_MODEL (menu));

  /* Until we get the omnigutter in place */
  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self->view),
                                       GTK_TEXT_WINDOW_LEFT);
  renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                           "width-request", 2,
                           NULL);
  gtk_source_gutter_insert (gutter, renderer, 100);

  /* Add gutter changes to the overview map */
  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self->map),
                                       GTK_TEXT_WINDOW_LEFT);
  renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                           "width-request", 1,
                           NULL);
  gtk_source_gutter_insert (gutter, renderer, 100);
}

GtkWidget *
ide_editor_page_new (IdeBuffer *buffer)
{
  g_return_val_if_fail (IDE_IS_BUFFER (buffer), NULL);

  return g_object_new (IDE_TYPE_EDITOR_PAGE,
                       "buffer", buffer,
                       NULL);
}

/**
 * ide_editor_page_get_view:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeSourceView for the page.
 *
 * Returns: (transfer none): an #IdeSourceView
 */
IdeSourceView *
ide_editor_page_get_view (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->view;
}

/**
 * ide_editor_page_get_buffer:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeBuffer for the page.
 *
 * Returns: (transfer none): an #IdeBuffer
 */
IdeBuffer *
ide_editor_page_get_buffer (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->buffer;
}

static void
ide_editor_page_save_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeEditorPage *self;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_page_set_progress (IDE_PAGE (self), NULL);

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_editor_page_save_response (GtkFileChooserNative *native,
                               int                   response,
                               IdeTask              *task)
{
  IdeEditorPage *self;
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (GTK_IS_FILE_CHOOSER_NATIVE (native));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  buffer = ide_task_get_task_data (task);

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (response == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));
      g_autoptr(IdeNotification) notif = NULL;

      ide_buffer_save_file_async (buffer,
                                  file,
                                  ide_task_get_cancellable (task),
                                  &notif,
                                  ide_editor_page_save_cb,
                                  g_object_ref (task));

      ide_page_set_progress (IDE_PAGE (self), notif);
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));
  g_object_unref (task);

  IDE_EXIT;
}

void
ide_editor_page_save_async (IdeEditorPage       *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeNotification) notif = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (IDE_IS_BUFFER (self->buffer));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_page_save_async);
  ide_task_set_task_data (task, ide_buffer_hold (self->buffer), ide_buffer_release);

  if (ide_buffer_get_is_temporary (self->buffer))
    {
      g_autoptr(GFile) workdir = NULL;
      GtkFileChooserNative *dialog;
      IdeWorkspace *workspace;
      IdeContext *context;

      workspace = ide_widget_get_workspace (GTK_WIDGET (self));
      context = ide_workspace_get_context (workspace);
      workdir = ide_context_ref_workdir (context);

      dialog = gtk_file_chooser_native_new (_("Save File"),
                                            GTK_WINDOW (workspace),
                                            GTK_FILE_CHOOSER_ACTION_SAVE,
                                            _("Save"), _("Cancel"));

      g_object_set (dialog,
                    "do-overwrite-confirmation", TRUE,
                    "modal", TRUE,
                    "select-multiple", FALSE,
                    "show-hidden", FALSE,
                    NULL);

      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), workdir, NULL);

      g_signal_connect (dialog,
                        "response",
                        G_CALLBACK (ide_editor_page_save_response),
                        g_object_ref (task));

      gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));

      IDE_EXIT;
    }

  ide_buffer_save_file_async (self->buffer,
                              ide_buffer_get_file (self->buffer),
                              cancellable,
                              &notif,
                              ide_editor_page_save_cb,
                              g_steal_pointer (&task));

  ide_page_set_progress (IDE_PAGE (self), notif);

  IDE_EXIT;
}

gboolean
ide_editor_page_save_finish (IdeEditorPage  *self,
                             GAsyncResult   *result,
                             GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_editor_page_discard_changes_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeEditorPage *self;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_page_set_progress (IDE_PAGE (self), NULL);

  if (!(buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  IDE_EXIT;
}

void
ide_editor_page_discard_changes_async (IdeEditorPage       *self,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  IdeBufferManager *bufmgr;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (IDE_IS_BUFFER (self->buffer));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_editor_page_discard_changes_async);
  ide_task_set_task_data (task, ide_buffer_hold (self->buffer), ide_buffer_release);

  if (ide_buffer_get_is_temporary (self->buffer))
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  context = ide_widget_get_context (GTK_WIDGET (self));
  bufmgr = ide_buffer_manager_from_context (context);
  notif = ide_notification_new ();
  ide_page_set_progress (IDE_PAGE (self), notif);

  ide_buffer_manager_load_file_async (bufmgr,
                                      ide_buffer_get_file (self->buffer),
                                      IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD,
                                      notif,
                                      cancellable,
                                      ide_editor_page_discard_changes_cb,
                                      g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_editor_page_discard_changes_finish (IdeEditorPage  *self,
                                        GAsyncResult   *result,
                                        GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_editor_page_get_gutter:
 * @self: a #IdeEditorPage
 *
 * Gets the #IdeGutter displayed in the editor page.
 *
 * Returns: (transfer none) (nullable): an #IdeGutter or %NULL
 */
IdeGutter *
ide_editor_page_get_gutter (IdeEditorPage *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_PAGE (self), NULL);

  return self->gutter;
}

void
ide_editor_page_set_gutter (IdeEditorPage *self,
                            IdeGutter     *gutter)
{
  GtkSourceGutter *container;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));
  g_return_if_fail (!gutter || IDE_IS_GUTTER (gutter));

  if (gutter == self->gutter)
    IDE_EXIT;

  container = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self->view),
                                          GTK_TEXT_WINDOW_LEFT);

  if (self->gutter)
    {
      gtk_source_gutter_remove (container, GTK_SOURCE_GUTTER_RENDERER (self->gutter));
      _ide_editor_page_settings_disconnect_gutter (self, self->gutter);
      g_clear_object (&self->gutter);
    }

  if (gutter)
    {
      g_set_object (&self->gutter, gutter);
      gtk_source_gutter_insert (container, GTK_SOURCE_GUTTER_RENDERER (self->gutter), 0);
      _ide_editor_page_settings_connect_gutter (self, self->gutter);
      ide_gutter_style_changed (self->gutter);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GUTTER]);

  IDE_EXIT;
}
