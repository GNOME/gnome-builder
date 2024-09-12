/* ide-tree.c
 *
 * Copyright 2018-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tree"

#include "config.h"

#include <libide-gtk.h>
#include <libide-plugins.h>

#include "ide-tree.h"
#include "ide-tree-addin.h"
#include "ide-tree-private.h"
#include "ide-tree-empty.h"

typedef struct
{
  IdeExtensionSetAdapter *addins;
  GtkTreeListModel       *tree_model;
  IdeTreeNode            *root;
  char                   *kind;
  GMenuModel             *menu_model;

  GtkScrolledWindow      *scroller;
  GtkListView            *list_view;
  GtkSingleSelection     *selection;

  GdkDragAction           drop_action;
} IdeTreePrivate;

enum {
  PROP_0,
  PROP_KIND,
  PROP_MENU_MODEL,
  PROP_ROOT,
  PROP_SELECTED_NODE,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeTree, ide_tree, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

typedef struct
{
  IdeTree     *tree;
  IdeTreeNode *node;
  GPtrArray   *providers;
} DragPrepare;

typedef struct
{
  IdeTree       *tree;
  GtkDropTarget *drop_target;
  IdeTreeNode   *node;
  GArray        *gtypes;
  GdkDragAction  action;
} DropAccept;

typedef struct
{
  IdeTree       *tree;
  GtkDropTarget *drop_target;
  IdeTreeNode   *node;
  GPtrArray     *active;
} Drop;

typedef struct
{
  IdeTree     *tree;
  IdeTreeNode *node;
  guint        handled : 1;
} NodeActivated;

static void
ide_tree_node_activated_cb (IdeExtensionSetAdapter *addins,
                            PeasPluginInfo         *plugin_info,
                            GObject          *extension,
                            gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)extension;
  NodeActivated *state = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (addins));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (state != NULL);

  if (!state->handled)
    state->handled = ide_tree_addin_node_activated (addin, state->tree, state->node);
}

static void
ide_tree_activate_cb (IdeTree     *self,
                      guint        position,
                      GtkListView *list_view)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(GtkTreeListRow) row = NULL;
  g_autoptr(IdeTreeNode) node = NULL;
  NodeActivated state;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  if (!(row = g_list_model_get_item (G_LIST_MODEL (priv->selection), position)) ||
      !(node = gtk_tree_list_row_get_item (row)))
    IDE_EXIT;

  state.tree = self;
  state.node = node;
  state.handled = FALSE;

  ide_extension_set_adapter_foreach (priv->addins,
                                     ide_tree_node_activated_cb,
                                     &state);

  IDE_EXIT;
}

static void
ide_tree_notify_selected_cb (IdeTree            *self,
                             GParamSpec         *pspec,
                             GtkSingleSelection *selection)
{
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_SINGLE_SELECTION (selection));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTED_NODE]);
}

static void
ide_tree_extension_added_cb (IdeExtensionSetAdapter *addins,
                             PeasPluginInfo         *plugin_info,
                             GObject          *extension,
                             gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)extension;
  IdeTree *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (addins));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));

  ide_tree_addin_load (addin, self);
}

static void
ide_tree_extension_removed_cb (IdeExtensionSetAdapter *addins,
                               PeasPluginInfo         *plugin_info,
                               GObject          *extension,
                               gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)extension;
  IdeTree *self = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (addins));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));

  ide_tree_addin_unload (addin, self);
}

static void
ide_tree_root (GtkWidget *widget)
{
  IdeTree *self = (IdeTree *)widget;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));

  GTK_WIDGET_CLASS (ide_tree_parent_class)->root (widget);

  if (priv->addins != NULL)
    return;

  priv->addins = ide_extension_set_adapter_new (NULL,
                                                peas_engine_get_default (),
                                                IDE_TYPE_TREE_ADDIN,
                                                "Tree-Kind", priv->kind);
  g_signal_connect (priv->addins,
                    "extension-added",
                    G_CALLBACK (ide_tree_extension_added_cb),
                    self);
  g_signal_connect (priv->addins,
                    "extension-removed",
                    G_CALLBACK (ide_tree_extension_removed_cb),
                    self);
  ide_extension_set_adapter_foreach (priv->addins,
                                     ide_tree_extension_added_cb,
                                     self);

  if (priv->root != NULL)
    dex_future_disown (_ide_tree_node_expand (priv->root, G_LIST_MODEL (priv->addins)));
}

static void
ide_tree_click_pressed_cb (GtkGestureClick *click,
                           int              n_press,
                           double           x,
                           double           y,
                           gpointer         user_data)
{
  g_autoptr(IdeTreeNode) node = NULL;
  GdkEventSequence *sequence;
  IdeTreeExpander *expander;
  GtkTreeListRow *row;
  IdeTreePrivate *priv;
  GdkEvent *event;
  IdeTree *tree;

  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (n_press != 1)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (click));
  event = gtk_gesture_get_last_event (GTK_GESTURE (click), sequence);
  expander = IDE_TREE_EXPANDER (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click)));
  tree = IDE_TREE (gtk_widget_get_ancestor (GTK_WIDGET (expander), IDE_TYPE_TREE));
  priv = ide_tree_get_instance_private (tree);
  row = ide_tree_expander_get_list_row (expander);
  node = IDE_TREE_NODE (gtk_tree_list_row_get_item (row));

  gtk_widget_activate_action (GTK_WIDGET (expander), "listitem.select", "(bb)", FALSE, FALSE);

  if (gdk_event_triggers_context_menu (event))
    {
      GtkPopover *popover;

      if (priv->menu_model == NULL)
        return;

      popover = g_object_new (GTK_TYPE_POPOVER_MENU,
                              "menu-model", priv->menu_model,
                              "has-arrow", TRUE,
                              "position", GTK_POS_RIGHT,
                              NULL);

      ide_tree_set_selected_node (tree, node);
      ide_tree_expander_show_popover (expander, popover);

      gtk_gesture_set_sequence_state (GTK_GESTURE (click),
                                      sequence,
                                      GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static void
ide_tree_click_released_cb (GtkGestureClick *click,
                            int              n_press,
                            double           x,
                            double           y,
                            gpointer         user_data)
{
  g_autoptr(IdeTreeNode) node = NULL;
  GdkEventSequence *sequence;
  IdeTreeExpander *expander;
  GtkTreeListRow *row;
  IdeTreePrivate *priv;
  IdeTree *tree;

  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (n_press != 1)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (click));
  expander = IDE_TREE_EXPANDER (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click)));
  tree = IDE_TREE (gtk_widget_get_ancestor (GTK_WIDGET (expander), IDE_TYPE_TREE));
  priv = ide_tree_get_instance_private (tree);
  row = ide_tree_expander_get_list_row (expander);
  node = IDE_TREE_NODE (gtk_tree_list_row_get_item (row));

  if (gtk_gesture_get_sequence_state (GTK_GESTURE (click), sequence) == GTK_EVENT_SEQUENCE_NONE)
    {
      NodeActivated state = {0};

      state.tree = tree;
      state.node = node;
      state.handled = FALSE;

      ide_extension_set_adapter_foreach (priv->addins,
                                         ide_tree_node_activated_cb,
                                         &state);

      if (state.handled)
        gtk_gesture_set_sequence_state (GTK_GESTURE (click),
                                        sequence,
                                        GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static void
ide_tree_drag_source_prepare_addin_cb (IdeExtensionSetAdapter *adapter,
                                       PeasPluginInfo         *plugin_info,
                                       GObject          *exten,
                                       gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;
  DragPrepare *state = user_data;
  GdkContentProvider *provider;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));

  if ((provider = ide_tree_addin_node_draggable (addin, state->node)))
    g_ptr_array_add (state->providers, g_steal_pointer (&provider));
}

static GdkContentProvider *
ide_tree_drag_source_prepare_cb (IdeTree       *self,
                                 double         x,
                                 double         y,
                                 GtkDragSource *source)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(IdeTreeNode) node = NULL;
  g_autoptr(GPtrArray) providers = NULL;
  GdkContentProvider *provider = NULL;
  IdeTreeExpander *expander;
  GtkTreeListRow *row;
  DragPrepare state = {0};

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_DRAG_SOURCE (source));

  expander = IDE_TREE_EXPANDER (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source)));
  row = ide_tree_expander_get_list_row (expander);
  node = IDE_TREE_NODE (gtk_tree_list_row_get_item (row));
  providers = g_ptr_array_new ();

  state.tree = self;
  state.node = node;
  state.providers = providers;

  ide_extension_set_adapter_foreach (priv->addins,
                                     ide_tree_drag_source_prepare_addin_cb,
                                     &state);

  if (providers->len == 0)
    IDE_RETURN (NULL);
  else if (providers->len == 1)
    provider = g_ptr_array_steal_index (providers, 0);
  else
    provider = gdk_content_provider_new_union ((GdkContentProvider **)providers->pdata, providers->len);

  gtk_gesture_set_state (GTK_GESTURE (source), GTK_EVENT_SEQUENCE_CLAIMED);
  gtk_drag_source_set_actions (source, GDK_ACTION_ALL);

  IDE_RETURN (provider);
}

static void
ide_tree_drag_source_drag_begin_cb (IdeTree       *self,
                                    GdkDrag       *drag,
                                    GtkDragSource *source)
{
  g_autoptr(GdkPaintable) paintable = NULL;
  GtkWidget *widget;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GDK_IS_DRAG (drag));
  g_assert (GTK_IS_DRAG_SOURCE (source));

  /* Get our IdeTreeExpander */
  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));

  /* But snapshot the parent to get row content */
  widget = gtk_widget_get_parent (widget);

  if ((paintable = gtk_widget_paintable_new (widget)))
    {
      GtkSnapshot *snapshot = gtk_snapshot_new ();
      double width = gdk_paintable_get_intrinsic_width (paintable);
      double height = gdk_paintable_get_intrinsic_height (paintable);
      g_autoptr(GdkPaintable) with_parent = NULL;

      gdk_paintable_snapshot (paintable, snapshot, width, height);

      with_parent = gtk_snapshot_free_to_paintable (snapshot, &GRAPHENE_SIZE_INIT (width, height));
      gtk_drag_source_set_icon (source, paintable, 0, 0);
    }

  IDE_EXIT;
}

static void
ide_tree_drag_source_drag_end_cb (IdeTree       *self,
                                  GdkDrag       *drag,
                                  gboolean       delete_data,
                                  GtkDragSource *source)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_DRAG_SOURCE (source));
  g_assert (GDK_IS_DRAG (drag));

  gtk_drag_source_set_content (source, NULL);
  gtk_drag_source_set_icon (source, NULL, 0, 0);

  IDE_EXIT;
}

static void
ide_tree_drop_target_accept_foreach_cb (IdeExtensionSetAdapter *adapter,
                                        PeasPluginInfo         *plugin_info,
                                        GObject          *exten,
                                        gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;
  DropAccept *state = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));

  state->action |= ide_tree_addin_node_droppable (addin, state->drop_target, state->node, state->gtypes);
}

static gboolean
ide_tree_drop_target_accept_cb (IdeTree       *self,
                                GdkDrop       *drop,
                                GtkDropTarget *drop_target)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(IdeTreeNode) node = NULL;
  g_autoptr(GArray) gtypes = NULL;
  IdeTreeExpander *expander;
  GtkTreeListRow *row;
  DropAccept state = {0};

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GDK_IS_DROP (drop));
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  expander = IDE_TREE_EXPANDER (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drop_target)));
  row = ide_tree_expander_get_list_row (expander);
  node = IDE_TREE_NODE (gtk_tree_list_row_get_item (row));
  gtypes = g_array_new (FALSE, FALSE, sizeof (GType));

  state.tree = self;
  state.drop_target = drop_target;
  state.node = node;
  state.action = 0;
  state.gtypes = gtypes;

  ide_extension_set_adapter_foreach (priv->addins,
                                     ide_tree_drop_target_accept_foreach_cb,
                                     &state);

  gtk_drop_target_set_actions (drop_target, state.action);
  gtk_drop_target_set_gtypes (drop_target, (GType *)(gpointer)gtypes->data, gtypes->len);

  IDE_RETURN (state.action != 0);
}

static GdkDragAction
get_preferred_action (IdeTree      *self,
                      const GValue *value)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (!value || G_IS_VALUE (value));

  if (value == NULL)
    return GDK_ACTION_COPY;

  if (G_VALUE_HOLDS_STRING (value))
    return GDK_ACTION_COPY;

  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    return GDK_ACTION_MOVE;

  return GDK_ACTION_COPY;
}

static void
ide_tree_drop_target_notify_value_cb (IdeTree       *self,
                                      GParamSpec    *pspec,
                                      GtkDropTarget *drop_target)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(IdeTreeNode) node = NULL;
  GtkTreeListRow *row;
  const GValue *value;
  GtkWidget *widget;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  if (!(value = gtk_drop_target_get_value (drop_target)) || !G_IS_VALUE (value))
    IDE_EXIT;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drop_target));
  row = ide_tree_expander_get_list_row (IDE_TREE_EXPANDER (widget));
  node = gtk_tree_list_row_get_item (row);

  g_assert (!node || IDE_IS_TREE_NODE (node));

  if (node == NULL)
    IDE_EXIT;

  priv->drop_action = get_preferred_action (self, value);

  IDE_EXIT;
}

static GdkDragAction
ide_tree_drop_target_enter_cb (IdeTree       *self,
                               double         x,
                               double         y,
                               GtkDropTarget *drop_target)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(IdeTreeNode) node = NULL;
  GtkTreeListRow *row;
  const GValue *value;
  GtkWidget *widget;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  priv->drop_action = 0;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drop_target));
  row = ide_tree_expander_get_list_row (IDE_TREE_EXPANDER (widget));
  node = gtk_tree_list_row_get_item (row);

  g_assert (!node || IDE_IS_TREE_NODE (node));

  if (node == NULL)
    IDE_GOTO (reject);

  value = gtk_drop_target_get_value (drop_target);

  priv->drop_action = get_preferred_action (self, value);

  IDE_RETURN (priv->drop_action);

reject:
  gtk_drop_target_reject (drop_target);

  IDE_RETURN (0);
}

static GdkDragAction
ide_tree_drop_target_motion_cb (IdeTree       *self,
                                double         x,
                                double         y,
                                GtkDropTarget *drop_target)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  IDE_RETURN (priv->drop_action);
}

static void
ide_tree_drop_target_leave_cb (IdeTree       *self,
                               GtkDropTarget *drop_target)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  priv->drop_action = 0;

  IDE_EXIT;
}

static void
drop_finalize (gpointer data)
{
  Drop *drop = data;

  g_clear_object (&drop->tree);
  g_clear_object (&drop->node);
  g_clear_object (&drop->drop_target);
  g_clear_pointer (&drop->active, g_ptr_array_unref);
}

static void
drop_unref (Drop *drop)
{
  g_atomic_rc_box_release_full (drop, drop_finalize);
}

static Drop *
drop_ref (Drop *drop)
{
  return g_atomic_rc_box_acquire (drop);
}

static void
ide_tree_drop_target_drop_addin_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)object;
  g_autoptr(GError) error = NULL;
  Drop *drop = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (drop != NULL);

  if (!ide_tree_addin_node_dropped_finish (addin, result, &error))
    {
      if (!ide_error_ignore (error))
        g_warning ("%s failed to handle drop onto node: %s",
                   G_OBJECT_TYPE_NAME (addin), error->message);
    }

  drop_unref (drop);

  IDE_EXIT;
}

static void
ide_tree_drop_target_drop_foreach_cb (IdeExtensionSetAdapter *set,
                                      PeasPluginInfo         *plugin_info,
                                      GObject          *exten,
                                      gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;
  g_autoptr(GArray) gtypes = NULL;
  Drop *drop = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));

  gtypes = g_array_new (FALSE, FALSE, sizeof (GType));

  if (ide_tree_addin_node_droppable (addin, drop->drop_target, drop->node, gtypes))
    ide_tree_addin_node_dropped_async (addin,
                                       drop->drop_target,
                                       drop->node,
                                       NULL,
                                       ide_tree_drop_target_drop_addin_cb,
                                       drop_ref (drop));
}

static void
ide_tree_drop_target_drop_cb (IdeTree       *self,
                              const GValue  *value,
                              double         x,
                              double         y,
                              GtkDropTarget *drop_target)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(IdeTreeNode) node = NULL;
  GtkTreeListRow *row;
  GtkWidget *widget;
  Drop *drop;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_DROP_TARGET (drop_target));

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drop_target));
  row = ide_tree_expander_get_list_row (IDE_TREE_EXPANDER (widget));
  node = gtk_tree_list_row_get_item (row);

  drop = g_atomic_rc_box_new0 (Drop);
  drop->tree = g_object_ref (self);
  drop->drop_target = g_object_ref (drop_target);
  drop->node = g_object_ref (node);
  drop->active = g_ptr_array_new_with_free_func (g_object_unref);

  ide_extension_set_adapter_foreach (priv->addins,
                                     ide_tree_drop_target_drop_foreach_cb,
                                     drop);

  drop_unref (drop);

  IDE_EXIT;
}

static void
ide_tree_list_item_setup_cb (IdeTree                  *self,
                             GtkListItem              *item,
                             GtkSignalListItemFactory *factory)
{
  IdeTreeExpander *expander;
  GtkDragSource *drag;
  GtkDropTarget *drop;
  GtkGesture *gesture;
  GtkImage *image;

  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_LIST_ITEM (item));
  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));

  image = g_object_new (GTK_TYPE_IMAGE, NULL);
  expander = g_object_new (IDE_TYPE_TREE_EXPANDER,
                           "suffix", image,
                           "has-tooltip", TRUE,
                           NULL);
  gtk_list_item_set_child (item, GTK_WIDGET (expander));

  /* Handle click events before IdeTreeExpander handles expansion of
   * rows which have children. We need to deal with right-click context
   * menus and IdeTreeAddin handling activation before.
   */
  gesture = gtk_gesture_click_new ();
  gtk_event_controller_set_name (GTK_EVENT_CONTROLLER (gesture), "ide-tree-click");
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect (gesture,
                    "pressed",
                    G_CALLBACK (ide_tree_click_pressed_cb),
                    NULL);
  g_signal_connect (gesture,
                    "released",
                    G_CALLBACK (ide_tree_click_released_cb),
                    NULL);
  gtk_widget_add_controller (GTK_WIDGET (expander), GTK_EVENT_CONTROLLER (gesture));

  /* Setup Drag gesture for this row */
  drag = gtk_drag_source_new ();
  gtk_event_controller_set_name (GTK_EVENT_CONTROLLER (drag), "ide-tree-drag");
  gtk_drag_source_set_actions (drag, GDK_ACTION_ALL);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (drag),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect_object (drag,
                           "prepare",
                           G_CALLBACK (ide_tree_drag_source_prepare_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (drag,
                           "drag-begin",
                           G_CALLBACK (ide_tree_drag_source_drag_begin_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (drag,
                           "drag-end",
                           G_CALLBACK (ide_tree_drag_source_drag_end_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (expander), GTK_EVENT_CONTROLLER (drag));

  /* Setup drop site for this row */
  drop = gtk_drop_target_new (G_TYPE_INVALID, GDK_ACTION_ALL);
  gtk_drop_target_set_preload (drop, TRUE);
  gtk_event_controller_set_name (GTK_EVENT_CONTROLLER (drop), "ide-tree-drop");
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (drop),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect_object (drop,
                           "accept",
                           G_CALLBACK (ide_tree_drop_target_accept_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (drop,
                           "enter",
                           G_CALLBACK (ide_tree_drop_target_enter_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (drop,
                           "leave",
                           G_CALLBACK (ide_tree_drop_target_leave_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (drop,
                           "motion",
                           G_CALLBACK (ide_tree_drop_target_motion_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (drop,
                           "notify::value",
                           G_CALLBACK (ide_tree_drop_target_notify_value_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (drop,
                           "drop",
                           G_CALLBACK (ide_tree_drop_target_drop_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (expander), GTK_EVENT_CONTROLLER (drop));
}

static void
ide_tree_list_item_teardown_cb (IdeTree                  *self,
                                GtkListItem              *item,
                                GtkSignalListItemFactory *factory)
{
  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_LIST_ITEM (item));
  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));

  gtk_list_item_set_child (item, NULL);
}

static void
ide_tree_row_notify_expanded_cb (IdeTree        *self,
                                 GParamSpec     *pspec,
                                 GtkTreeListRow *row)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(IdeTreeNode) node = NULL;

  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_TREE_LIST_ROW (row));

  node = gtk_tree_list_row_get_item (row);

  if (gtk_tree_list_row_get_expanded (row))
    {
      if (!_ide_tree_node_children_built (node))
        dex_future_disown (_ide_tree_node_expand (node, G_LIST_MODEL (priv->addins)));
    }
  else
    {
      if (node != NULL)
        _ide_tree_node_collapsed (node);
    }
}

static inline GIcon *
get_icon (GIcon      **icon,
          const char  *name)
{
  if G_UNLIKELY (*icon == NULL)
    *icon = g_themed_icon_new (name);
  return *icon;
}

static gboolean
flags_to_icon (GBinding     *binding,
               const GValue *from_value,
               GValue       *to_value,
               gpointer      user_data)
{
  static GIcon *changed_icon;
  static GIcon *added_icon;

  IdeTreeNodeFlags flags = g_value_get_flags (from_value);
  g_autoptr(GObject) suffix = g_binding_dup_target (binding);
  GIcon *icon;

  if (flags & IDE_TREE_NODE_FLAGS_ADDED)
    icon = get_icon (&added_icon, "builder-vcs-added-symbolic");
  else if (flags & IDE_TREE_NODE_FLAGS_CHANGED)
    icon = get_icon (&changed_icon, "builder-vcs-changed-symbolic");
  else
    icon = NULL;

  g_value_set_object (to_value, icon);
  gtk_widget_set_visible (GTK_WIDGET (suffix), icon != NULL);

  return TRUE;
}

static gboolean
ide_tree_attach_popover_to_row (IdeTreeNode     *node,
                                GtkPopover      *popover,
                                IdeTreeExpander *expander)
{
  IDE_ENTRY;

  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (GTK_IS_POPOVER (popover));
  g_assert (IDE_IS_TREE_EXPANDER (expander));

  ide_tree_expander_show_popover (expander, popover);

  IDE_RETURN (TRUE);
}

static void
ide_tree_list_item_bind_cb (IdeTree                  *self,
                            GtkListItem              *item,
                            GtkSignalListItemFactory *factory)
{
  g_autoptr(IdeTreeNode) node = NULL;
  IdeTreeExpander *expander;
  GtkTreeListRow *row;
  GtkWidget *suffix;

  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_LIST_ITEM (item));
  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));

  gtk_list_item_set_focusable (item, FALSE);

  row = GTK_TREE_LIST_ROW (gtk_list_item_get_item (item));
  expander = IDE_TREE_EXPANDER (gtk_list_item_get_child (item));
  node = gtk_tree_list_row_get_item (row);
  suffix = ide_tree_expander_get_suffix (expander);

  g_assert (GTK_IS_TREE_LIST_ROW (row));
  g_assert (IDE_IS_TREE_EXPANDER (expander));
  g_assert (IDE_IS_TREE_NODE (node));

  ide_tree_expander_set_list_row (expander, row);

#define BIND_PROPERTY(name, to) \
  G_STMT_START { \
    GBinding *binding = g_object_bind_property (node, name, expander, to, G_BINDING_SYNC_CREATE); \
    g_object_set_data_full (G_OBJECT (expander), "BINDING_" name to, g_object_ref (binding), g_object_unref); \
  } G_STMT_END

  BIND_PROPERTY ("expanded-icon", "expanded-icon");
  BIND_PROPERTY ("icon", "icon");
  BIND_PROPERTY ("title", "title");
  BIND_PROPERTY ("vcs-ignored", "ignored");
  BIND_PROPERTY ("title", "tooltip-text");
  BIND_PROPERTY ("use-markup", "use-markup");

  g_object_set_data_full (G_OBJECT (expander),
                          "BINDING_flagsflags",
                          g_object_ref (g_object_bind_property_full (node, "flags",
                                                                     suffix, "gicon",
                                                                     G_BINDING_SYNC_CREATE,
                                                                     flags_to_icon, NULL, NULL, NULL)),
                          g_object_unref);

#undef BIND_PROPERTY

  g_signal_connect_object (row,
                           "notify::expanded",
                           G_CALLBACK (ide_tree_row_notify_expanded_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (node,
                           "show-popover",
                           G_CALLBACK (ide_tree_attach_popover_to_row),
                           expander,
                           0);
}

static void
ide_tree_list_item_unbind_cb (IdeTree                  *self,
                              GtkListItem              *item,
                                GtkSignalListItemFactory *factory)
{
  g_autoptr(IdeTreeNode) node = NULL;
  IdeTreeExpander *expander;
  GtkTreeListRow *row;

  g_assert (IDE_IS_TREE (self));
  g_assert (GTK_IS_LIST_ITEM (item));
  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));

  row = GTK_TREE_LIST_ROW (gtk_list_item_get_item (item));
  expander = IDE_TREE_EXPANDER (gtk_list_item_get_child (item));
  node = gtk_tree_list_row_get_item (row);

  if (node != NULL)
    g_signal_handlers_disconnect_by_func (node,
                                          G_CALLBACK (ide_tree_attach_popover_to_row),
                                          expander);

  g_signal_handlers_disconnect_by_func (row,
                                        G_CALLBACK (ide_tree_row_notify_expanded_cb),
                                        self);

#define UNBIND_PROPERTY(name, to) \
  G_STMT_START { \
    GBinding *binding = g_object_steal_data (G_OBJECT (expander), "BINDING_" name); \
    if (binding != NULL) \
      { \
        g_binding_unbind (binding); \
        g_object_unref (binding); \
      } \
  } G_STMT_END

  UNBIND_PROPERTY ("expanded-icon", "expanded-icon");
  UNBIND_PROPERTY ("icon", "icon");
  UNBIND_PROPERTY ("title", "title");
  UNBIND_PROPERTY ("vcs-ignored", "ignored");
  UNBIND_PROPERTY ("title", "tooltip-text");
  UNBIND_PROPERTY ("use-markup", "use-markup");
  UNBIND_PROPERTY ("flags", "flags");

#undef UNBIND_PROPERTY

  g_object_set (expander,
                "expanded-icon", NULL,
                "icon", NULL,
                "title", NULL,
                "use-markup", FALSE,
                NULL);

  ide_tree_expander_set_list_row (expander, NULL);
}

static void
invalidate_all_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *param)
{
  ide_tree_invalidate_all (IDE_TREE (widget));
}

static void
ide_tree_dispose (GObject *object)
{
  IdeTree *self = (IdeTree *)object;
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  ide_clear_and_destroy_object (&priv->addins);

  g_clear_pointer ((GtkWidget **)&priv->scroller, gtk_widget_unparent);

  ide_tree_set_root (self, NULL);

  if (priv->selection != NULL)
    gtk_single_selection_set_model (priv->selection, NULL);

  g_clear_object (&priv->tree_model);
  g_clear_object (&priv->menu_model);

  g_clear_pointer (&priv->kind, g_free);

  G_OBJECT_CLASS (ide_tree_parent_class)->dispose (object);
}

static void
ide_tree_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  IdeTree *self = IDE_TREE (object);
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_string (value, priv->kind);
      break;

    case PROP_MENU_MODEL:
      g_value_set_object (value, ide_tree_get_menu_model (self));
      break;

    case PROP_ROOT:
      g_value_set_object (value, ide_tree_get_root (self));
      break;

    case PROP_SELECTED_NODE:
      g_value_set_object (value, ide_tree_get_selected_node (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  IdeTree *self = IDE_TREE (object);
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_KIND:
      priv->kind = g_value_dup_string (value);
      break;

    case PROP_MENU_MODEL:
      ide_tree_set_menu_model (self, g_value_get_object (value));
      break;

    case PROP_ROOT:
      ide_tree_set_root (self, g_value_get_object (value));
      break;

    case PROP_SELECTED_NODE:
      ide_tree_set_selected_node (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_class_init (IdeTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_tree_dispose;
  object_class->get_property = ide_tree_get_property;
  object_class->set_property = ide_tree_set_property;

  widget_class->root = ide_tree_root;

  properties[PROP_KIND] =
    g_param_spec_string ("kind", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_MENU_MODEL] =
    g_param_spec_object ("menu-model", NULL, NULL,
                         G_TYPE_MENU_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_ROOT] =
    g_param_spec_object ("root", NULL, NULL,
                         IDE_TYPE_TREE_NODE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SELECTED_NODE] =
    g_param_spec_object ("selected-node", NULL, NULL,
                         IDE_TYPE_TREE_NODE,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-tree/ide-tree.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_bind_template_child_private (widget_class, IdeTree, list_view);
  gtk_widget_class_bind_template_child_private (widget_class, IdeTree, selection);
  gtk_widget_class_bind_template_child_private (widget_class, IdeTree, scroller);

  gtk_widget_class_bind_template_callback (widget_class, ide_tree_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_tree_notify_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_tree_list_item_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_tree_list_item_unbind_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_tree_list_item_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_tree_list_item_teardown_cb);

  gtk_widget_class_install_action (widget_class, "tree.invalidate-all", NULL, invalidate_all_action);
}

static void
ide_tree_init (IdeTree *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static GListModel *
ide_tree_create_child_model_cb (gpointer item,
                                gpointer user_data)
{
  IdeTreeNode *node = item;

  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (user_data == NULL);

  if (ide_tree_node_get_children_possible (node))
    return ide_tree_empty_new (node);

  return NULL;
}

/**
 * ide_tree_get_root:
 * @self: a #IdeTree
 *
 * Gets the root node.
 *
 * Returns: (transfer none) (nullable): an IdeTreeNode or %NULL
 */
IdeTreeNode *
ide_tree_get_root (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);

  return priv->root;
}

void
ide_tree_set_root (IdeTree     *self,
                   IdeTreeNode *root)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (!root || IDE_IS_TREE_NODE (root));

  if (priv->root == root)
    return;

  gtk_single_selection_set_model (priv->selection, NULL);
  g_clear_object (&priv->tree_model);

  if (priv->root != NULL)
    {
      g_object_set_data (G_OBJECT (priv->root), "IDE_TREE", NULL);
      g_clear_object (&priv->root);
    }

  g_set_object (&priv->root, root);

  if (priv->root != NULL)
    {
      GListModel *base_model = G_LIST_MODEL (priv->root);

      g_object_set_data (G_OBJECT (priv->root), "IDE_TREE", self);

      priv->tree_model = gtk_tree_list_model_new (g_object_ref (base_model),
                                                  FALSE, /* Passthrough */
                                                  FALSE,  /* Autoexpand */
                                                  ide_tree_create_child_model_cb,
                                                  NULL, NULL);
      gtk_single_selection_set_model (priv->selection, G_LIST_MODEL (priv->tree_model));

      if (priv->addins != NULL)
        dex_future_disown (_ide_tree_node_expand (priv->root, G_LIST_MODEL (priv->addins)));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ROOT]);
}

void
ide_tree_show_popover_at_node (IdeTree     *self,
                               IdeTreeNode *node,
                               GtkPopover  *popover)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeListRow *row;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (GTK_IS_POPOVER (popover));

  if ((row = _ide_tree_get_row_at_node (self, node, TRUE)))
    {
      guint position = gtk_tree_list_row_get_position (row);

      gtk_widget_activate_action (GTK_WIDGET (priv->list_view), "list.scroll-to-item", "u", position);

      if (!_ide_tree_node_show_popover (node, popover))
        {
          g_warning ("Failed to show popover, no signal handler consumed popover!");
          g_object_ref_sink (popover);
          g_object_unref (popover);
        }
    }
}

static GtkTreeListRow *
_ide_tree_get_row_at_node_recurse (IdeTree     *self,
                                   IdeTreeNode *node,
                                   gboolean     expand_to_node)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(GtkTreeListRow) row = NULL;
  IdeTreeNode *parent;
  guint index;

  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_NODE (node));

  /* The root node cannot have a GtkTreeListRow */
  if (!(parent = ide_tree_node_get_parent (node)))
    return NULL;

  /* Get our index for offset use within models */
  index = _ide_tree_node_get_child_index (parent, node);

  /* Handle children of the root specially by getting their
   * row from the GtkTreeListModel.
   */
  if (parent == priv->root)
    return gtk_tree_list_model_get_child_row (priv->tree_model, index);

  /* Expand the parent row and use the resulting row to locate
   * the child within that.
   */
  if ((row = _ide_tree_get_row_at_node_recurse (self, parent, expand_to_node)))
    {
      if (expand_to_node)
        gtk_tree_list_row_set_expanded (row, TRUE);
      return gtk_tree_list_row_get_child_row (row, index);
    }

  /* Failed to get row, probably due to something not expanded */
  return NULL;
}

GtkTreeListRow *
_ide_tree_get_row_at_node (IdeTree     *self,
                           IdeTreeNode *node,
                           gboolean     expand_to_node)
{
  g_return_val_if_fail (IDE_IS_TREE (self), NULL);
  g_return_val_if_fail (!node || IDE_IS_TREE_NODE (node), NULL);

  if (node == NULL)
    return NULL;

  return _ide_tree_get_row_at_node_recurse (self, node, expand_to_node);
}

gboolean
ide_tree_is_node_expanded (IdeTree     *self,
                           IdeTreeNode *node)
{
  g_autoptr(GtkTreeListRow) row = NULL;

  g_return_val_if_fail (IDE_IS_TREE (self), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), FALSE);

  if ((row = _ide_tree_get_row_at_node (self, node, FALSE)))
    return gtk_tree_list_row_get_expanded (row);

  return FALSE;
}

/**
 * ide_tree_get_selected_node:
 * @self: a #IdeTree
 *
 * Gets the selected item.
 *
 * Returns: (transfer none) (nullable): an #IdeTreeNode or %NULL
 */
IdeTreeNode *
ide_tree_get_selected_node (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  GtkTreeListRow *row;

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);

  if ((row = gtk_single_selection_get_selected_item (priv->selection)))
    {
      g_autoptr(GObject) item = gtk_tree_list_row_get_item (row);

      g_assert (IDE_IS_TREE_NODE (item));

      /* Return borrowed instance, which we are sure will stick
       * around after the unref as it's part of node tree.
       */
      return IDE_TREE_NODE (item);
    }

  return NULL;
}

/**
 * ide_tree_set_selected_node:
 * @self: a #IdeTree
 * @node: (nullable): an #IdeTreeNode or %NULL
 *
 * Sets the selected item in the tree.
 *
 * If @node is %NULL, the current selection is cleared.
 */
void
ide_tree_set_selected_node (IdeTree     *self,
                            IdeTreeNode *node)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(GtkTreeListRow) row = NULL;
  guint position = GTK_INVALID_LIST_POSITION;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (!node || IDE_IS_TREE_NODE (node));

  if ((row = _ide_tree_get_row_at_node (self, node, TRUE)))
    position = gtk_tree_list_row_get_position (row);

  gtk_single_selection_set_selected (priv->selection, position);

  gtk_widget_activate_action (GTK_WIDGET (priv->list_view), "list.scroll-to-item", "u", position);
}

void
ide_tree_invalidate_all (IdeTree *self)
{
  IdeTreeNode *root;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE (self));

  if (!(root = ide_tree_get_root (self)))
    IDE_EXIT;

  g_object_ref (root);
  ide_tree_set_root (self, NULL);
  ide_tree_set_root (self, root);
  g_object_unref (root);

  IDE_EXIT;
}

void
ide_tree_expand_to_node (IdeTree     *self,
                         IdeTreeNode *node)
{
  g_autoptr(GtkTreeListRow) row = NULL;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  row = _ide_tree_get_row_at_node (self, node, TRUE);
}

static DexFuture *
ide_tree_expand_node_cb (DexFuture *completed,
                         gpointer   user_data)
{
  IdeTreeNode *node = user_data;
  g_autoptr(GtkTreeListRow) row = NULL;
  IdeTree *self;

  IDE_ENTRY;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (IDE_IS_TREE_NODE (node));

  if (!(self = _ide_tree_node_get_tree (node)))
    IDE_RETURN (dex_future_new_for_boolean (TRUE));

  g_assert (IDE_IS_TREE (self));

  if ((row = _ide_tree_get_row_at_node (self, node, TRUE)))
    gtk_tree_list_row_set_expanded (row, TRUE);

  IDE_RETURN (dex_future_new_for_boolean (TRUE));
}

void
ide_tree_expand_node_async (IdeTree             *self,
                            IdeTreeNode         *node,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);
  g_autoptr(DexAsyncResult) result = NULL;

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  result = dex_async_result_new (self, cancellable, callback, user_data);

  dex_async_result_await (result,
                          dex_future_then (_ide_tree_node_expand (node,
                                                                  G_LIST_MODEL (priv->addins)),
                                           ide_tree_expand_node_cb,
                                           g_object_ref (node),
                                           g_object_unref));
}

gboolean
ide_tree_expand_node_finish (IdeTree       *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (IDE_IS_TREE (self), FALSE);
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (result), FALSE);

  return dex_async_result_propagate_boolean (DEX_ASYNC_RESULT (result), error);
}

void
ide_tree_expand_node (IdeTree     *self,
                      IdeTreeNode *node)
{
  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));

  ide_tree_expand_node_async (self, node, NULL, NULL, NULL);
}

void
ide_tree_collapse_node (IdeTree     *self,
                        IdeTreeNode *node)
{
  g_autoptr(GtkTreeListRow) row = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (self));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (node != ide_tree_get_root (self));

  if ((row = _ide_tree_get_row_at_node (self, node, FALSE)))
    gtk_tree_list_row_set_expanded (row, FALSE);
}

/**
 * ide_tree_get_menu_model:
 * @self: a #IdeTree
 *
 * Gets the menu model for the tree.
 *
 * Returns: (transfer none) (nullable): a #GMenuModel or %NULL
 */
GMenuModel *
ide_tree_get_menu_model (IdeTree *self)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TREE (self), NULL);

  return priv->menu_model;
}

void
ide_tree_set_menu_model (IdeTree    *self,
                         GMenuModel *menu_model)
{
  IdeTreePrivate *priv = ide_tree_get_instance_private (self);

  g_return_if_fail (IDE_IS_TREE (self));
  g_return_if_fail (!menu_model || G_IS_MENU_MODEL (menu_model));

  if (g_set_object (&priv->menu_model, menu_model))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MENU_MODEL]);
}
