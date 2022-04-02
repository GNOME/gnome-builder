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

#include <libide-threading.h>

#include "ide-gui-global.h"
#include "ide-page.h"
#include "ide-workspace-private.h"

typedef struct
{
  GList        mru_link;

  const char  *menu_id;
  const char  *icon_name;
  char        *title;
  GIcon       *icon;

  guint        failed : 1;
  guint        modified : 1;
  guint        can_split : 1;
} IdePagePrivate;

enum {
  PROP_0,
  PROP_CAN_SPLIT,
  PROP_FAILED,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_MENU_ID,
  PROP_MODIFIED,
  PROP_TITLE,
  N_PROPS
};

enum {
  CREATE_SPLIT,
  N_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdePage, ide_page, PANEL_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

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
  GtkWidget *toplevel;

  g_assert (IDE_IS_PAGE (self));

  GTK_WIDGET_CLASS (ide_page_parent_class)->root (widget);

  toplevel = GTK_WIDGET (gtk_widget_get_native (widget));

  if (IDE_IS_WORKSPACE (toplevel))
    _ide_workspace_add_page_mru (IDE_WORKSPACE (toplevel), &priv->mru_link);
}

static void
ide_page_unroot (GtkWidget *widget)
{
  IdePage *self = (IdePage *)widget;
  IdePagePrivate *priv = ide_page_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (IDE_IS_PAGE (self));

  toplevel = GTK_WIDGET (gtk_widget_get_native (widget));

  if (IDE_IS_WORKSPACE (toplevel))
    _ide_workspace_remove_page_mru (IDE_WORKSPACE (toplevel), &priv->mru_link);

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
ide_page_finalize (GObject *object)
{
  IdePage *self = (IdePage *)object;
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);
  g_clear_object (&priv->icon);

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

    case PROP_ICON_NAME:
      g_value_set_static_string (value, ide_page_get_icon_name (self));
      break;

    case PROP_ICON:
      g_value_set_object (value, ide_page_get_icon (self));
      break;

    case PROP_MENU_ID:
      g_value_set_static_string (value, ide_page_get_menu_id (self));
      break;

    case PROP_MODIFIED:
      g_value_set_boolean (value, ide_page_get_modified (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_page_get_title (self));
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

    case PROP_ICON_NAME:
      ide_page_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_ICON:
      ide_page_set_icon (self, g_value_get_object (value));
      break;

    case PROP_MENU_ID:
      ide_page_set_menu_id (self, g_value_get_string (value));
      break;

    case PROP_MODIFIED:
      ide_page_set_modified (self, g_value_get_boolean (value));
      break;

    case PROP_TITLE:
      ide_page_set_title (self, g_value_get_string (value));
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

  properties [PROP_ICON] =
    g_param_spec_object ("icon",
                         "Icon",
                         "A GIcon for the view",
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "The icon-name describing the view content",
                         "text-x-generic-symbolic",
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_MENU_ID] =
    g_param_spec_string ("menu-id",
                         "Menu ID",
                         "The identifier of the GMenu to use in the document popover",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_MODIFIED] =
    g_param_spec_boolean ("modified",
                          "Modified",
                          "If the view has been modified from the saved content",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the document or view",
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
}

static void
ide_page_init (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);
  g_autoptr(GSimpleActionGroup) group = g_simple_action_group_new ();

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);

  priv->mru_link.data = self;
  priv->icon_name = g_intern_string ("text-x-generic-symbolic");

  /* Add an action group out of convenience to plugins that want to
   * stash a simple action somewhere.
   */
  gtk_widget_insert_action_group (GTK_WIDGET (self), "view", G_ACTION_GROUP (group));
}

GtkWidget *
ide_page_new (void)
{
  return g_object_new (IDE_TYPE_PAGE, NULL);
}

const char *
ide_page_get_title (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PAGE (self), NULL);

  return priv->title;
}

void
ide_page_set_title (IdePage    *self,
                    const char *title)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));

  if (g_strcmp0 (title, priv->title) != 0)
    {
      g_free (priv->title);
      priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
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
      priv->menu_id = menu_id;
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
ide_page_get_modified (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PAGE (self), FALSE);

  return priv->modified;
}

void
ide_page_set_modified (IdePage  *self,
                       gboolean  modified)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));

  modified = !!modified;

  if (priv->modified != modified)
    {
      priv->modified = modified;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODIFIED]);
    }
}

/**
 * ide_page_get_icon:
 * @self: a #IdePage
 *
 * Gets the #GIcon to represent the view.
 *
 * Returns: (transfer none) (nullable): A #GIcon or %NULL
 */
GIcon *
ide_page_get_icon (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PAGE (self), NULL);

  if (priv->icon == NULL)
    {
      if (priv->icon_name != NULL)
        priv->icon = g_icon_new_for_string (priv->icon_name, NULL);
    }

  return priv->icon;
}

void
ide_page_set_icon (IdePage *self,
                   GIcon   *icon)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));

  if (g_set_object (&priv->icon, icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON]);
}

const char *
ide_page_get_icon_name (IdePage *self)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PAGE (self), NULL);

  return priv->icon_name;
}

void
ide_page_set_icon_name (IdePage    *self,
                        const char *icon_name)
{
  IdePagePrivate *priv = ide_page_get_instance_private (self);

  g_return_if_fail (IDE_IS_PAGE (self));

  icon_name = g_intern_string (icon_name);

  if (icon_name != priv->icon_name)
    {
      priv->icon_name = icon_name;
      g_clear_object (&priv->icon);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON_NAME]);
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
  gtk_widget_insert_after (GTK_WIDGET (infobar), GTK_WIDGET (self), NULL);
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
