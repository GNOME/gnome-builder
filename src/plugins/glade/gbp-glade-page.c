/* gbp-glade-page.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-glade-page"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-glade-page.h"
#include "gbp-glade-private.h"

G_DEFINE_TYPE (GbpGladePage, gbp_glade_page, IDE_TYPE_PAGE)

enum {
  PROP_0,
  PROP_PROJECT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * gbp_glade_page_new:
 *
 * Create a new #GbpGladePage.
 *
 * Returns: (transfer full): a newly created #GbpGladePage
 */
GbpGladePage *
gbp_glade_page_new (void)
{
  return g_object_new (GBP_TYPE_GLADE_PAGE, NULL);
}

static void
gbp_glade_page_notify_modified_cb (GbpGladePage *self,
                                   GParamSpec   *pspec,
                                   GladeProject *project)
{
  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (GLADE_IS_PROJECT (project));

  ide_page_set_modified (IDE_PAGE (self),
                                glade_project_get_modified (project));
}

static void
gbp_glade_page_changed_cb (GbpGladePage *self,
                           GladeCommand *command,
                           gboolean      execute,
                           GladeProject *project)
{
  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (!command || GLADE_IS_COMMAND (command));
  g_assert (GLADE_IS_PROJECT (project));

  if (project != self->project)
    return;

  _gbp_glade_page_update_actions (self);
}

static void
gbp_glade_page_set_project (GbpGladePage *self,
                            GladeProject *project)
{
  GladeProject *old_project = NULL;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (GLADE_IS_PROJECT (project));

  if (project == self->project)
    return;

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  if (self->project != NULL)
    {
      old_project = g_object_ref (self->project);
      glade_app_remove_project (self->project);
      dzl_signal_group_set_target (self->project_signals, NULL);
      g_clear_object (&self->project);
    }

  if (project != NULL)
    {
      self->project = g_object_ref (project);
      glade_app_add_project (self->project);
      dzl_signal_group_set_target (self->project_signals, project);
    }

  if (self->designer != NULL)
    {
      gtk_widget_destroy (GTK_WIDGET (self->designer));
      g_assert (self->designer == NULL);

      if (self->project != NULL)
        {
          self->designer = g_object_new (GLADE_TYPE_DESIGN_VIEW,
                                         "project", self->project,
                                         "vexpand", TRUE,
                                         "visible", TRUE,
                                         NULL);
          g_signal_connect (self->designer,
                            "destroy",
                            G_CALLBACK (gtk_widget_destroyed),
                            &self->designer);
          dzl_gtk_widget_add_style_class (GTK_WIDGET (self->designer), "glade-designer");
          gtk_container_add_with_properties (GTK_CONTAINER (self->main_box), GTK_WIDGET (self->designer),
                                             "pack-type", GTK_PACK_START,
                                             "position", 0,
                                             NULL);
        }
    }

  if (self->chooser != NULL)
    glade_adaptor_chooser_set_project (self->chooser, self->project);

  ide_page_set_modified (IDE_PAGE (self),
                                self->project != NULL && glade_project_get_modified (self->project));

  g_clear_object (&old_project);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROJECT]);
}

gboolean
_gbp_glade_page_reload (GbpGladePage *self)
{
  GladeProject *project;

  g_return_val_if_fail (GBP_IS_GLADE_PAGE (self), FALSE);
  g_return_val_if_fail (GLADE_IS_PROJECT (self->project), FALSE);

  /*
   * Switch to a new GladeProject object, which is rather tricky
   * because we need to update everything that connected to it.
   * Sadly we can't reuse existing GladeProject objects.
   */
  project = glade_project_new ();
  gbp_glade_page_set_project (self, project);
  gbp_glade_page_load_file_async (self, self->file, NULL, NULL, NULL);
  g_clear_object (&project);

  /*
   * This is sort of a hack, buf if we want everything to adapt to our
   * new project, we need to signal that the view changed so that it
   * grabs the new version of our GladeProject.
   */
  if (gtk_widget_get_visible (GTK_WIDGET (self)) &&
      gtk_widget_get_child_visible (GTK_WIDGET (self)))
    {
      gtk_widget_hide (GTK_WIDGET (self));
      gtk_widget_show (GTK_WIDGET (self));

      return TRUE;
    }

  return FALSE;
}

gboolean
_gbp_glade_page_save (GbpGladePage  *self,
                      GError       **error)
{
  const gchar *path;

  g_return_val_if_fail (GBP_IS_GLADE_PAGE (self), FALSE);
  g_return_val_if_fail (GLADE_IS_PROJECT (self->project), FALSE);

  if (self->file == NULL || !(path = g_file_peek_path (self->file)))
    {
      /* Implausible path */
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "No file has been set for the view");
      return FALSE;
    }

  if (glade_project_save (self->project, path, error))
    {
      IdeBufferManager *bufmgr;
      IdeContext *context;
      IdeBuffer *buffer;

      context = ide_widget_get_context (GTK_WIDGET (self));
      bufmgr = ide_buffer_manager_from_context (context);

      /* We successfully wrote the file, so trigger a full reload of the
       * IdeBuffer if there is one already currently open.
       */

      if ((buffer = ide_buffer_manager_find_buffer (bufmgr, self->file)))
        {
          ide_buffer_manager_load_file_async (bufmgr,
                                              ide_buffer_get_file (buffer),
                                              IDE_BUFFER_OPEN_FLAGS_NO_VIEW | IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD,
                                              NULL, NULL, NULL, NULL);
        }

      return TRUE;
    }

  return FALSE;
}

static void
gbp_glade_page_agree_to_close_async (IdePage       *view,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  GbpGladePage *self = (GbpGladePage *)view;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_glade_page_agree_to_close_async);

  if (ide_page_get_modified (view))
    {
      if (!_gbp_glade_page_save (self, &error))
        {
          if (error != NULL)
            {
              ide_task_return_error (task, g_steal_pointer (&error));
              return;
            }

          /* No was clicked on an internal glade save dialog, fallthrough */
        }
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_glade_page_agree_to_close_finish (IdePage  *view,
                                      GAsyncResult   *result,
                                      GError        **error)
{
  g_assert (GBP_IS_GLADE_PAGE (view));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
viewport_style_changed_cb (GbpGladePage    *self,
                           GtkStyleContext *style_context)
{
  GdkRGBA bg, fg;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (GTK_IS_STYLE_CONTEXT (style_context));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_style_context_get_color (style_context, GTK_STATE_FLAG_NORMAL, &fg);
  gtk_style_context_get_background_color (style_context, GTK_STATE_FLAG_NORMAL, &bg);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  ide_page_set_primary_color_bg (IDE_PAGE (self), &bg);
  ide_page_set_primary_color_fg (IDE_PAGE (self), &fg);
}

static void
gbp_glade_page_buffer_saved_cb (GbpGladePage     *self,
                                IdeBuffer        *buffer,
                                IdeBufferManager *bufmgr)
{
  GFile *file;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));

  if (self->file == NULL)
    return;

  file = ide_buffer_get_file (buffer);

  if (g_file_equal (file, self->file))
    _gbp_glade_page_reload (self);
}

static void
gbp_glade_page_context_set (GtkWidget  *widget,
                            IdeContext *context)
{
  GbpGladePage *self = (GbpGladePage *)widget;
  IdeBufferManager *bufmgr;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    return;

  /* Track when buffers are saved so that we can reload the view */
  bufmgr = ide_buffer_manager_from_context (context);
  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (gbp_glade_page_buffer_saved_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_glade_page_add_signal_handler_cb (GbpGladePage      *self,
                                      GladeWidget       *widget,
                                      const GladeSignal *gsignal,
                                      GladeProject      *project)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (GLADE_IS_WIDGET (widget));
  g_assert (GLADE_IS_SIGNAL (gsignal));
  g_assert (GLADE_IS_PROJECT (project));

  g_print ("add signal handler: %s\n",
           glade_signal_get_handler (gsignal));

  IDE_EXIT;
}

static void
gbp_glade_page_remove_signal_handler_cb (GbpGladePage      *self,
                                         GladeWidget       *widget,
                                         const GladeSignal *gsignal,
                                         GladeProject      *project)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (GLADE_IS_WIDGET (widget));
  g_assert (GLADE_IS_SIGNAL (gsignal));
  g_assert (GLADE_IS_PROJECT (project));

  g_print ("remove signal handler: %s\n",
           glade_signal_get_handler (gsignal));

  IDE_EXIT;
}

static void
gbp_glade_page_change_signal_handler_cb (GbpGladePage      *self,
                                         GladeWidget       *widget,
                                         const GladeSignal *old_gsignal,
                                         const GladeSignal *new_gsignal,
                                         GladeProject      *project)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (GLADE_IS_WIDGET (widget));
  g_assert (GLADE_IS_SIGNAL (old_gsignal));
  g_assert (GLADE_IS_SIGNAL (new_gsignal));
  g_assert (GLADE_IS_PROJECT (project));

  g_print ("change signal handler: %s => %s\n",
           glade_signal_get_handler (old_gsignal),
           glade_signal_get_handler (new_gsignal));

  IDE_EXIT;
}

static void
gbp_glade_page_activate_signal_handler_cb (GbpGladePage      *self,
                                           GladeWidget       *widget,
                                           const GladeSignal *gsignal,
                                           GladeProject      *project)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (GLADE_IS_WIDGET (widget));
  g_assert (GLADE_IS_SIGNAL (gsignal));
  g_assert (GLADE_IS_PROJECT (project));

  g_print ("activate signal handler: %s\n",
           glade_signal_get_handler (gsignal));

  IDE_EXIT;
}

static void
gbp_glade_page_dispose (GObject *object)
{
  GbpGladePage *self = (GbpGladePage *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->project);

  if (self->project_signals != NULL)
    {
      dzl_signal_group_set_target (self->project_signals, NULL);
      g_clear_object (&self->project_signals);
    }

  G_OBJECT_CLASS (gbp_glade_page_parent_class)->dispose (object);
}

static void
gbp_glade_page_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpGladePage *self = GBP_GLADE_PAGE (object);

  switch (prop_id)
    {
    case PROP_PROJECT:
      g_value_set_object (value, gbp_glade_page_get_project (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_glade_page_class_init (GbpGladePageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdePageClass *view_class = IDE_PAGE_CLASS (klass);

  object_class->dispose = gbp_glade_page_dispose;
  object_class->get_property = gbp_glade_page_get_property;

  view_class->agree_to_close_async = gbp_glade_page_agree_to_close_async;
  view_class->agree_to_close_finish = gbp_glade_page_agree_to_close_finish;

  properties [PROP_PROJECT] =
    g_param_spec_object ("project",
                         "Project",
                         "The project for the view",
                         GLADE_TYPE_PROJECT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "gbpgladeview");
}

static void
gbp_glade_page_init (GbpGladePage *self)
{
  GtkBox *box;
  GtkViewport *viewport;
  GtkStyleContext *style_context;
  GladeProject *project = NULL;
  static const struct {
    const gchar *action_target;
    const gchar *icon_name;
    const gchar *tooltip;
  } pointers[] = {
    { "select", "pointer-mode-select-symbolic", N_("Switch to selection mode") },
    { "drag-resize", "pointer-mode-drag-symbolic", N_("Switch to drag-resize mode") },
    { "margin-edit", "pointer-mode-resize-symbolic", N_("Switch to margin editor") },
    { "align-edit", "pointer-mode-pin-symbolic", N_("Switch to alignment editor") },
  };

  ide_page_set_can_split (IDE_PAGE (self), FALSE);
  ide_page_set_menu_id (IDE_PAGE (self), "gbp-glade-page-menu");
  ide_page_set_title (IDE_PAGE (self), _("Unnamed Glade project"));
  ide_page_set_icon_name (IDE_PAGE (self), "org.gnome.Glade-symbolic");
  ide_page_set_menu_id (IDE_PAGE (self), "gbp-glade-page-document-menu");

  self->project_signals = dzl_signal_group_new (GLADE_TYPE_PROJECT);

  dzl_signal_group_connect_object (self->project_signals,
                                   "notify::modified",
                                   G_CALLBACK (gbp_glade_page_notify_modified_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->project_signals,
                                   "changed",
                                   G_CALLBACK (gbp_glade_page_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->project_signals,
                                   "add-signal-handler",
                                   G_CALLBACK (gbp_glade_page_add_signal_handler_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->project_signals,
                                   "remove-signal-handler",
                                   G_CALLBACK (gbp_glade_page_remove_signal_handler_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->project_signals,
                                   "change-signal-handler",
                                   G_CALLBACK (gbp_glade_page_change_signal_handler_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  dzl_signal_group_connect_object (self->project_signals,
                                   "activate-signal-handler",
                                   G_CALLBACK (gbp_glade_page_activate_signal_handler_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  ide_widget_set_context_handler (self, gbp_glade_page_context_set);

  project = glade_project_new ();
  gbp_glade_page_set_project (self, project);
  g_clear_object (&project);

  self->main_box = g_object_new (GTK_TYPE_BOX,
                                 "orientation", GTK_ORIENTATION_VERTICAL,
                                 "visible", TRUE,
                                 NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->main_box));

  self->chooser = g_object_new (GLADE_TYPE_ADAPTOR_CHOOSER,
                                "project", self->project,
                                "visible", TRUE,
                                NULL);
  g_signal_connect (self->chooser,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->chooser);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->chooser), "glade-chooser");
  gtk_container_add_with_properties (GTK_CONTAINER (self->main_box), GTK_WIDGET (self->chooser),
                                     "pack-type", GTK_PACK_END,
                                     NULL);

  self->designer = g_object_new (GLADE_TYPE_DESIGN_VIEW,
                                 "project", self->project,
                                 "vexpand", TRUE,
                                 "visible", TRUE,
                                 NULL);
  g_signal_connect (self->designer,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->designer);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->designer), "glade-designer");
  gtk_container_add (GTK_CONTAINER (self->main_box), GTK_WIDGET (self->designer));

  /* Discover viewport so that we can track the background color changes
   * from CSS. That is used to set our primary color.
   */
  viewport = dzl_gtk_widget_find_child_typed (GTK_WIDGET (self->designer), GTK_TYPE_VIEWPORT);
  style_context = gtk_widget_get_style_context (GTK_WIDGET (viewport));
  g_signal_connect_object (style_context,
                           "changed",
                           G_CALLBACK (viewport_style_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  viewport_style_changed_cb (self, style_context);

  /* Setup pointer-mode controls */

  box = g_object_new (GTK_TYPE_BOX,
                      "visible", TRUE,
                      NULL);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (box), "linked");
  gtk_container_add (GTK_CONTAINER (self->chooser), GTK_WIDGET (box));

  for (guint i = 0; i < G_N_ELEMENTS (pointers); i++)
    {
      g_autoptr(GVariant) param = NULL;
      GtkButton *button;
      GtkImage *image;

      param = g_variant_take_ref (g_variant_new_string (pointers[i].action_target));

      image = g_object_new (GTK_TYPE_IMAGE,
                            "icon-name", pointers[i].icon_name,
                            "pixel-size", 16,
                            "visible", TRUE,
                            NULL);
      button = g_object_new (GTK_TYPE_BUTTON,
                             "action-name", "glade-view.pointer-mode",
                             "action-target", param,
                             "child", image,
                             "has-tooltip", TRUE,
                             "tooltip-text", gettext(pointers[i].tooltip),
                             "visible", TRUE,
                             NULL);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (button), "image-button");
      gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (button));
    }

  /* Setup action state and shortcuts */

  _gbp_glade_page_init_actions (self);
  _gbp_glade_page_init_shortcuts (GTK_WIDGET (self));
}

/**
 * gbp_glade_page_get_project:
 *
 * Returns: (transfer none): A #GladeProject or %NULL
 */
GladeProject *
gbp_glade_page_get_project (GbpGladePage *self)
{
  g_return_val_if_fail (GBP_IS_GLADE_PAGE (self), NULL);

  return self->project;
}

static gboolean
file_missing_or_empty (GFile *file)
{
  g_autoptr(GFileInfo) info = NULL;

  g_assert (G_IS_FILE (file));

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            NULL);

  return info == NULL || g_file_info_get_size (info) == 0;
}

static void
gbp_glade_page_load_file_map_cb (GladeDesignView *designer,
                                 IdeTask         *task)
{
  g_autofree gchar *name = NULL;
  GbpGladePage *self;
  const gchar *path;
  GFile *file;

  g_assert (GLADE_IS_DESIGN_VIEW (designer));
  g_assert (IDE_IS_TASK (task));
  g_assert (gtk_widget_get_mapped (GTK_WIDGET (designer)));

  self = ide_task_get_source_object (task);
  file = ide_task_get_task_data (task);

  g_signal_handlers_disconnect_by_func (self->designer,
                                        G_CALLBACK (gbp_glade_page_load_file_map_cb),
                                        task);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "File must be a local file");
      return;
    }

  /*
   * If the file is empty, nothing to load for now. Just go
   * ahead and wait until we save to overwrite it.
   */
  if (file_missing_or_empty (file))
    {
      name = g_file_get_basename (file);
      ide_page_set_title (IDE_PAGE (self), name);
      ide_task_return_boolean (task, TRUE);
      return;
    }

  path = g_file_peek_path (file);

  if (!glade_project_load_from_file (self->project, path))
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to load glade project");
  else
    ide_task_return_boolean (task, TRUE);

  name = glade_project_get_name (self->project);
  ide_page_set_title (IDE_PAGE (self), name);
}

void
gbp_glade_page_load_file_async (GbpGladePage        *self,
                                GFile               *file,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (GBP_IS_GLADE_PAGE (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_glade_page_load_file_async);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  g_set_object (&self->file, file);

  /* We can't load the file until we have been mapped or else we see an issue
   * where toplevels cannot be parented properly. If we come across that, then
   * delay until the widget is mapped.
   */
  if (!gtk_widget_get_mapped (GTK_WIDGET (self->designer)))
    g_signal_connect_data (self->designer,
                           "map",
                           G_CALLBACK (gbp_glade_page_load_file_map_cb),
                           g_steal_pointer (&task),
                           (GClosureNotify)g_object_unref,
                           0);
  else
    gbp_glade_page_load_file_map_cb (self->designer, task);
}

gboolean
gbp_glade_page_load_file_finish (GbpGladePage  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (GBP_IS_GLADE_PAGE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * gbp_glade_page_get_file:
 *
 * Returns: (nullable) (transfer none): a #GFile or %NULL
 */
GFile *
gbp_glade_page_get_file (GbpGladePage *self)
{
  g_return_val_if_fail (GBP_IS_GLADE_PAGE (self), NULL);

  return self->file;
}
