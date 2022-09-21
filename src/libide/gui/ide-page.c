/* ide-page.c
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

#define G_LOG_DOMAIN "ide-page"

#include "config.h"

#include <string.h>

#include <libide-gtk.h>
#include <libide-threading.h>

#include "ide-application.h"
#include "ide-gui-global.h"
#include "ide-page-private.h"
#include "ide-workbench-private.h"
#include "ide-workspace-private.h"

typedef struct
{
  GList           mru_link;

  const char     *menu_id;

  GtkBox         *content_box;
  GtkOverlay     *overlay;
  GtkProgressBar *progress_bar;

  guint           in_mru : 1;
  guint           failed : 1;
  guint           modified : 1;
  guint           can_split : 1;
} IdePagePrivate;

enum {
  PROP_0,
  PROP_CAN_SPLIT,
  PROP_FAILED,
  PROP_MENU_ID,
  N_PROPS
};

enum {
  CREATE_SPLIT,
  N_SIGNALS
};

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdePage, ide_page, PANEL_TYPE_WIDGET,
                                  G_ADD_PRIVATE (IdePage)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GtkBuildableIface *parent_buildable;
static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

GList *
_ide_page_get_mru_link (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);
  g_assert (IDE_IS_PAGE (self));
  return &priv->mru_link;
}

static void
ide_page_real_agree_to_close_async (IdePage             *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_PAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, ide_page_agree_to_close_async);
  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_page_real_agree_to_close_finish (IdePage       *self,
                                     GAsyncResult  *result,
                                     GError       **error)
{
  g_assert (IDE_IS_PAGE (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_page_root (GtkWidget *widget)
{
  IdePage *self = (IdePage *)widget;
  IdePagePrivate *priv = ide_page_get_instance_private (self);
  g_autoptr(PanelPosition) position = NULL;
  GtkWidget *toplevel;

  g_assert (IDE_IS_PAGE (self));

  GTK_WIDGET_CLASS (ide_page_parent_class)->root (widget);

  /* Ignore any IdePage placed into panels, such as the terminal */
  if (!(toplevel = GTK_WIDGET (gtk_widget_get_root (widget))) ||
      !IDE_IS_WORKSPACE (toplevel) ||
      !(position = panel_widget_get_position (PANEL_WIDGET (widget))) ||
      !panel_position_get_area_set (position) ||
      panel_position_get_area (position) != PANEL_AREA_CENTER)
    return;

  _ide_workspace_add_page_mru (IDE_WORKSPACE (toplevel), &priv->mru_link);

  priv->in_mru = TRUE;
}

static void
ide_page_unroot (GtkWidget *widget)
{
  IdePage *self = (IdePage *)widget;
  IdePagePrivate *priv = ide_page_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (IDE_IS_PAGE (self));

  if (priv->in_mru &&
      (toplevel = GTK_WIDGET (gtk_widget_get_root (widget))) &&
      IDE_IS_WORKSPACE (toplevel))
    {
      _ide_workspace_remove_page_mru (IDE_WORKSPACE (toplevel), &priv->mru_link);
      priv->in_mru = FALSE;
    }

  GTK_WIDGET_CLASS (ide_page_parent_class)->unroot (widget);
}

/**
 * ide_page_mark_used:
 * @self: a #IdePage
 *
 * This function marks the page as used by updating it's position in the
 * workspaces MRU (most-recently-used) queue.
 *
 * Pages should call this when their contents have been focused.
 */
void
ide_page_mark_used (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);
  IdeWorkspace *workspace;

  g_return_if_fail (IDE_IS_PAGE (self));

  if ((workspace = ide_widget_get_workspace (GTK_WIDGET (self))))
    _ide_workspace_move_front_page_mru (workspace, &priv->mru_link);
}

static void
open_in_new_workspace_action (GtkWidget  *widget,
                              const char *action_name,
                              GVariant   *param)
{
  IdePage *self = (IdePage *)widget;
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkspace *workspace;
  IdeWorkbench *workbench;
  IdePage *split;

  IDE_ENTRY;

  g_assert (IDE_IS_PAGE (self));

  if (!(split = ide_page_create_split (self)))
    IDE_EXIT;

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  workspace = _ide_workbench_create_secondary (workbench);
  position = panel_position_new ();

  ide_workspace_add_page (IDE_WORKSPACE (workspace), IDE_PAGE (split), position);

  gtk_window_present (GTK_WINDOW (workspace));

  IDE_EXIT;
}

static void
open_in_new_frame_action (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  IdePage *self = (IdePage *)widget;
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkspace *workspace;
  IdePage *split;

  IDE_ENTRY;

  g_assert (IDE_IS_PAGE (self));

  if (!(split = ide_page_create_split (self)))
    IDE_EXIT;

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  position = panel_widget_get_position (PANEL_WIDGET (self));
  panel_position_set_column (position, panel_position_get_column (position) + 1);

  ide_workspace_add_page (IDE_WORKSPACE (workspace), IDE_PAGE (split), position);

  IDE_EXIT;
}

static void
split_action (GtkWidget  *widget,
              const char *action_name,
              GVariant   *param)
{
  IdePage *self = (IdePage *)widget;
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkspace *workspace;
  IdePage *split;

  IDE_ENTRY;

  g_assert (IDE_IS_PAGE (self));

  if (!(split = ide_page_create_split (self)))
    IDE_EXIT;

  workspace = ide_widget_get_workspace (GTK_WIDGET (self));
  position = panel_widget_get_position (PANEL_WIDGET (self));
  panel_position_set_row (position, panel_position_get_row (position) + 1);

  ide_workspace_add_page (IDE_WORKSPACE (workspace), IDE_PAGE (split), position);

  IDE_EXIT;
}

static void
ide_page_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_page_parent_class)->finalize (object);
}

static void
ide_page_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  IdePage *self = IDE_PAGE (object);

  switch (prop_id)
    {
    case PROP_CAN_SPLIT:
      g_value_set_boolean (value, ide_page_get_can_split (self));
      break;

    case PROP_FAILED:
      g_value_set_boolean (value, ide_page_get_failed (self));
      break;

    case PROP_MENU_ID:
      g_value_set_static_string (value, ide_page_get_menu_id (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_page_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  IdePage *self = IDE_PAGE (object);

  switch (prop_id)
    {
    case PROP_CAN_SPLIT:
      ide_page_set_can_split (self, g_value_get_boolean (value));
      break;

    case PROP_FAILED:
      ide_page_set_failed (self, g_value_get_boolean (value));
      break;

    case PROP_MENU_ID:
      ide_page_set_menu_id (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_page_class_init (IdePageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  PanelWidgetClass *panel_widget_class = PANEL_WIDGET_CLASS (klass);

  object_class->finalize = ide_page_finalize;
  object_class->get_property = ide_page_get_property;
  object_class->set_property = ide_page_set_property;

  widget_class->root = ide_page_root;
  widget_class->unroot = ide_page_unroot;

  klass->agree_to_close_async = ide_page_real_agree_to_close_async;
  klass->agree_to_close_finish = ide_page_real_agree_to_close_finish;

  properties [PROP_CAN_SPLIT] =
    g_param_spec_boolean ("can-split",
                          "Can Split",
                          "If the view can be split into a second view",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FAILED] =
    g_param_spec_boolean ("failed",
                          "Failed",
                          "If the view has failed or crashed",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_MENU_ID] =
    g_param_spec_string ("menu-id",
                         "Menu ID",
                         "The identifier of the GMenu to use in the document popover",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdePage::create-split:
   * @self: an #IdePage
   *
   * This signal is emitted when the view is requested to make a split
   * version of itself. This happens when the user requests that a second
   * version of the file to be displayed, often side-by-side.
   *
   * This signal will only be emitted when #IdePage:can-split is
   * set to %TRUE. The default is %FALSE.
   *
   * Returns: (transfer full): A newly created #IdePage
   */
  signals [CREATE_SPLIT] =
    g_signal_new (g_intern_static_string ("create-split"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdePageClass, create_split),
                  g_signal_accumulator_first_wins, NULL,
                  NULL, IDE_TYPE_PAGE, 0);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "page");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-page.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdePage, content_box);
  gtk_widget_class_bind_template_child_private (widget_class, IdePage, overlay);
  gtk_widget_class_bind_template_child_private (widget_class, IdePage, progress_bar);

  panel_widget_class_install_action (panel_widget_class, "open-in-new-workspace", NULL, open_in_new_workspace_action);
  panel_widget_class_install_action (panel_widget_class, "open-in-new-frame", NULL, open_in_new_frame_action);
  panel_widget_class_install_action (panel_widget_class, "split", NULL, split_action);
}

static void
ide_page_init (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  priv->mru_link.data = self;

  gtk_widget_init_template (GTK_WIDGET (self));

  panel_widget_set_kind (PANEL_WIDGET (self), PANEL_WIDGET_KIND_DOCUMENT);
}

const char *
ide_page_get_menu_id (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PAGE (self), NULL);

  return priv->menu_id;
}

void
ide_page_set_menu_id (IdePage    *self,
                      const char *menu_id)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));

  menu_id = g_intern_string (menu_id);

  if (menu_id != priv->menu_id)
    {
      GMenu *menu;

      priv->menu_id = menu_id;

      menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, menu_id);
      panel_widget_set_menu_model (PANEL_WIDGET (self), G_MENU_MODEL (menu));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MENU_ID]);
    }
}

void
ide_page_agree_to_close_async (IdePage             *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_return_if_fail (IDE_IS_PAGE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_PAGE_GET_CLASS (self)->agree_to_close_async (self, cancellable, callback, user_data);
}

gboolean
ide_page_agree_to_close_finish (IdePage       *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (IDE_IS_PAGE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_PAGE_GET_CLASS (self)->agree_to_close_finish (self, result, error);
}

gboolean
ide_page_get_failed (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PAGE (self), FALSE);

  return priv->failed;
}

void
ide_page_set_failed (IdePage  *self,
                     gboolean  failed)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));

  failed = !!failed;

  if (failed != priv->failed)
    {
      priv->failed = failed;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FAILED]);
    }
}

gboolean
ide_page_get_can_split (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PAGE (self), FALSE);

  return priv->can_split;
}

void
ide_page_set_can_split (IdePage  *self,
                        gboolean  can_split)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));

  can_split = !!can_split;

  if (priv->can_split != can_split)
    {
      priv->can_split = can_split;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_SPLIT]);
    }
}

/**
 * ide_page_create_split:
 * @self: an #IdePage
 *
 * This function requests that the #IdePage create a split version
 * of itself so that the user may view the document in multiple views.
 *
 * The view should be added to an #IdeLayoutStack where appropriate.
 *
 * Returns: (nullable) (transfer full): A newly created #IdePage or %NULL.
 */
IdePage *
ide_page_create_split (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);
  IdePage *ret = NULL;

  g_return_val_if_fail (IDE_IS_PAGE (self), NULL);

  if (priv->can_split)
    {
      g_signal_emit (self, signals [CREATE_SPLIT], 0, &ret);
      g_return_val_if_fail (!ret || IDE_IS_PAGE (ret), NULL);
    }

  return ret;
}

/**
 * ide_page_report_error:
 * @self: a #IdePage
 * @format: a printf-style format string
 *
 * This function reports an error to the user in the layout view.
 *
 * @format should be a printf-style format string followed by the
 * arguments for the format.
 */
void
ide_page_report_error (IdePage    *self,
                       const char *format,
                       ...)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);
  g_autofree char *message = NULL;
  GtkInfoBar *infobar;
  GtkLabel *label;
  va_list args;

  g_return_if_fail (IDE_IS_PAGE (self));

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  infobar = g_object_new (GTK_TYPE_INFO_BAR,
                          "message-type", GTK_MESSAGE_WARNING,
                          "show-close-button", TRUE,
                          "visible", TRUE,
                          NULL);
  g_signal_connect (infobar,
                    "response",
                    G_CALLBACK (gtk_widget_unparent),
                    NULL);
  g_signal_connect (infobar,
                    "close",
                    G_CALLBACK (gtk_widget_unparent),
                    NULL);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", message,
                        "visible", TRUE,
                        "wrap", TRUE,
                        "xalign", 0.0f,
                        NULL);

  gtk_info_bar_add_child (infobar, GTK_WIDGET (label));

  gtk_box_prepend (priv->content_box, GTK_WIDGET (infobar));
}

/**
 * ide_page_get_file_or_directory:
 * @self: a #IdePage
 *
 * Gets a #GFile representing a file or directory that best maps to this
 * page. A terminal might use the current working directory while an editor
 * or designer might use the backing file.
 *
 * Returns: (transfer full) (nullable): a #GFile or %NULL
 */
GFile *
ide_page_get_file_or_directory (IdePage *self)
{
  g_return_val_if_fail (IDE_IS_PAGE (self), NULL);

  if (IDE_PAGE_GET_CLASS (self)->get_file_or_directory)
    return IDE_PAGE_GET_CLASS (self)->get_file_or_directory (self);

  return NULL;
}

void
ide_page_add_content_widget (IdePage   *self,
                             GtkWidget *widget)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_box_append (priv->content_box, widget);
}

static void
ide_page_add_child (GtkBuildable *buildable,
                    GtkBuilder   *builder,
                    GObject      *object,
                    const char   *name)
{
  IdePage *self = (IdePage *)buildable;

  g_assert (IDE_IS_PAGE (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (object));

  if (GTK_IS_WIDGET (object))
    {
      if (g_strcmp0 (name, "content") == 0)
        {
          ide_page_add_content_widget (self, GTK_WIDGET (object));
          return;
        }
    }

  parent_buildable->add_child (buildable, builder, object, name);
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable = g_type_interface_peek_parent (iface);
  iface->add_child = ide_page_add_child;
}

/**
 * ide_page_set_progress:
 * @self: a #IdePage
 * @notification: (nullable): an #IdeNotification or %NULL
 *
 * Set interactive progress for the page.
 *
 * When the operation is completed, the caller shoudl call this method
 * again and reutrn a value of %NULL for @notification.
 */
void
ide_page_set_progress (IdePage         *self,
                       IdeNotification *notification)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));
  g_return_if_fail (!notification || IDE_IS_NOTIFICATION (notification));

  if (notification == NULL)
    {
      ide_gtk_widget_hide_with_fade (GTK_WIDGET (priv->progress_bar));
      return;
    }

  gtk_progress_bar_set_fraction (priv->progress_bar, .0);
  gtk_widget_show (GTK_WIDGET (priv->progress_bar));
  g_object_bind_property (notification, "progress",
                          priv->progress_bar, "fraction",
                          G_BINDING_SYNC_CREATE);
}

/**
 * ide_page_get_position:
 * @self: a #IdePage
 *
 * Gets the position of a page within the workspace.
 *
 * Returns: (transfer full) (nullable): an #PanelPosition or %NULL
 *   if the page is not rooted.
 */
PanelPosition *
ide_page_get_position (IdePage *self)
{
  return panel_widget_get_position (PANEL_WIDGET (self));
}

void
ide_page_destroy (IdePage *self)
{
  GtkWidget *frame;

  g_return_if_fail (IDE_IS_PAGE (self));

  if ((frame = gtk_widget_get_ancestor (GTK_WIDGET (self), PANEL_TYPE_FRAME)))
    panel_frame_remove (PANEL_FRAME (frame), PANEL_WIDGET (self));
}

void
ide_page_observe (IdePage  *self,
                  IdePage **location)
{
  g_return_if_fail (IDE_IS_PAGE (self));
  g_return_if_fail (location != NULL);

  *location = self;
  g_signal_connect_swapped (self,
                            "destroy",
                            G_CALLBACK (g_nullify_pointer),
                            location);
}

void
ide_page_unobserve (IdePage  *self,
                    IdePage **location)
{
  g_return_if_fail (IDE_IS_PAGE (self));
  g_return_if_fail (location != NULL);

  g_signal_handlers_disconnect_by_func (self,
                                        G_CALLBACK (g_nullify_pointer),
                                        location);
  *location = NULL;
}

void
ide_clear_page (IdePage **location)
{
  IdePage *self = *location;

  if (self == NULL)
    return;

  ide_page_unobserve (self, location);
  ide_page_destroy (self);
}
