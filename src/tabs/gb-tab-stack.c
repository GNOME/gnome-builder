/* gb-tab-stack.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "tab-stack"

#include <glib/gi18n.h>

#include "gb-tab-grid.h"
#include "gb-log.h"
#include "gb-tab-stack.h"

struct _GbTabStackPrivate
{
  GtkButton     *close;
  GtkComboBox   *combo;
  GtkStack      *controls;
  GtkBox        *header_box;
  GtkMenuButton *stack_menu;
  GtkStack      *stack;
  GtkListStore  *store;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbTabStack, gb_tab_stack, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_MODEL,
  LAST_PROP
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static void gb_tab_stack_tab_closed (GbTabStack *stack,
                                     GbTab      *tab);

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GtkWidget *
gb_tab_stack_new (void)
{
  return g_object_new (GB_TYPE_TAB_STACK, NULL);
}

static gboolean
gb_tab_stack_queue_draw (gpointer data)
{
  g_return_val_if_fail (GTK_IS_WIDGET (data), FALSE);

  gtk_widget_queue_draw (GTK_WIDGET (data));

  return G_SOURCE_REMOVE;
}

guint
gb_tab_stack_get_n_tabs (GbTabStack *stack)
{
  GbTabStackPrivate *priv;
  GList *children;
  guint n_tabs;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), 0);

  priv = stack->priv;

  children = gtk_container_get_children (GTK_CONTAINER (priv->stack));
  n_tabs = g_list_length (children);
  g_list_free (children);

  return n_tabs;
}

/**
 * gb_tab_stack_get_tabs:
 * @stack: (in): A #GbTabStack.
 *
 * Returns all of the tabs within the stack.
 *
 * Returns: (transfer container) (element-type GbTab*): A #GList of #GbTab.
 */
GList *
gb_tab_stack_get_tabs (GbTabStack *stack)
{
  g_return_val_if_fail (GB_IS_TAB_STACK (stack), NULL);

  return gtk_container_get_children (GTK_CONTAINER (stack->priv->stack));
}

static gboolean
gb_tab_stack_get_tab_iter (GbTabStack  *stack,
                           GbTab       *tab,
                           GtkTreeIter *iter)
{
  GtkTreeModel *model;
  gint position = -1;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), FALSE);
  g_return_val_if_fail (iter, FALSE);

  if (gtk_widget_get_parent (GTK_WIDGET (tab)) == GTK_WIDGET (stack->priv->stack))
    gtk_container_child_get (GTK_CONTAINER (stack->priv->stack), GTK_WIDGET (tab),
                             "position", &position,
                             NULL);

  if (position != -1)
    {
      model = GTK_TREE_MODEL (stack->priv->store);

      if (gtk_tree_model_get_iter_first (model, iter))
        {
          for (; position; position--)
            {
              if (!gtk_tree_model_iter_next (model, iter))
                return FALSE;
            }
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gb_tab_stack_focus_iter (GbTabStack  *stack,
                         GtkTreeIter *iter)
{
  gboolean ret = FALSE;
  GbTab *tab = NULL;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), FALSE);
  g_return_val_if_fail (iter, FALSE);

  gtk_tree_model_get (GTK_TREE_MODEL (stack->priv->store), iter,
                      0, &tab,
                      -1);

  if (GB_IS_TAB (tab))
    {
      gtk_combo_box_set_active_iter (stack->priv->combo, iter);
      gtk_widget_grab_focus (GTK_WIDGET (tab));
      ret = TRUE;
    }

  g_clear_object (&tab);

  return ret;
}

gboolean
gb_tab_stack_focus_tab (GbTabStack *stack,
                        GbTab      *tab)
{
  GtkTreeIter iter;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), FALSE);
  g_return_val_if_fail (GB_IS_TAB (tab), FALSE);

  if (gb_tab_stack_get_tab_iter (stack, tab, &iter))
    {
      gb_tab_stack_focus_iter (stack, &iter);
      return TRUE;
    }

  return FALSE;
}

void
gb_tab_stack_remove_tab (GbTabStack *stack,
                         GbTab      *tab)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_return_if_fail (GB_IS_TAB_STACK (stack));
  g_return_if_fail (GB_IS_TAB (tab));

  model = GTK_TREE_MODEL (stack->priv->store);

  if (gb_tab_stack_get_tab_iter (stack, tab, &iter))
    {
      g_signal_handlers_disconnect_by_func (tab,
                                            gb_tab_stack_tab_closed,
                                            stack);
      g_signal_handlers_disconnect_by_func (tab,
                                            gb_tab_stack_queue_draw,
                                            stack);

      gtk_container_remove (GTK_CONTAINER (stack->priv->controls),
                            gb_tab_get_controls (tab));
      gtk_container_remove (GTK_CONTAINER (stack->priv->stack),
                            GTK_WIDGET (tab));

      if (!gtk_list_store_remove (stack->priv->store, &iter))
        {
          guint count;

          if ((count = gtk_tree_model_iter_n_children (model, NULL)))
            {
              if (gtk_tree_model_iter_nth_child (model, &iter, NULL, count-1))
                gb_tab_stack_focus_iter (stack, &iter);
            }
        }
      else
        gb_tab_stack_focus_iter (stack, &iter);
    }

  g_signal_emit (stack, gSignals [CHANGED], 0);
}

gboolean
gb_tab_stack_focus_next (GbTabStack *stack)
{
  GtkWidget *child;
  GtkTreeIter iter;
  gboolean ret = FALSE;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), FALSE);

  if (!(child = gtk_stack_get_visible_child (stack->priv->stack)))
    RETURN (FALSE);

  if (gb_tab_stack_get_tab_iter (stack, GB_TAB (child), &iter) &&
      gtk_tree_model_iter_next (GTK_TREE_MODEL (stack->priv->store), &iter))
    ret = gb_tab_stack_focus_iter (stack, &iter);

  RETURN (ret);
}

gboolean
gb_tab_stack_focus_previous (GbTabStack *stack)
{
  GtkWidget *child;
  GtkTreeIter iter;
  gboolean ret = FALSE;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), FALSE);

  if (!(child = gtk_stack_get_visible_child (stack->priv->stack)))
    RETURN (FALSE);

  if (gb_tab_stack_get_tab_iter (stack, GB_TAB (child), &iter) &&
      gtk_tree_model_iter_previous (GTK_TREE_MODEL (stack->priv->store), &iter))
    ret = gb_tab_stack_focus_iter (stack, &iter);

  RETURN (ret);
}

gboolean
gb_tab_stack_focus_first (GbTabStack *stack)
{
  GtkTreeIter iter;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), FALSE);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (stack->priv->store),
                                     &iter))
    RETURN (gb_tab_stack_focus_iter (stack, &iter));

  RETURN (FALSE);
}

gboolean
gb_tab_stack_focus_last (GbTabStack *stack)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint n_children;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), FALSE);

  model = GTK_TREE_MODEL (stack->priv->store);
  n_children = gtk_tree_model_iter_n_children (model, NULL);

  if (n_children != 0)
    {
      if (gtk_tree_model_iter_nth_child (model, &iter, NULL, n_children-1))
        RETURN (gb_tab_stack_focus_iter (stack, &iter));
    }

  RETURN (FALSE);
}

gboolean
gb_tab_stack_contains_tab (GbTabStack *stack,
                           GbTab      *tab)
{
  gboolean ret = FALSE;
  GList *list;
  GList *iter;

  g_return_val_if_fail (GB_IS_TAB_STACK (stack), FALSE);
  g_return_val_if_fail (GB_IS_TAB (tab), FALSE);

  list = gb_tab_stack_get_tabs (stack);

  for (iter = list; iter; iter = iter->next)
    {
      if (iter->data == (void *)tab)
        {
          ret = TRUE;
          break;
        }
    }

  g_list_free (list);

  return ret;
}

static void
gb_tab_stack_combobox_changed (GbTabStack  *stack,
                               GtkComboBox *combobox)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GbTab *tab = NULL;

  g_return_if_fail (GB_IS_TAB_STACK (stack));

  model = gtk_combo_box_get_model (combobox);

  if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
      gtk_tree_model_get (model, &iter, 0, &tab, -1);

      if (GB_IS_TAB (tab))
        {
          GtkWidget *controls;

          gtk_stack_set_visible_child (stack->priv->stack, GTK_WIDGET (tab));
          gtk_widget_set_sensitive (GTK_WIDGET (stack->priv->close), TRUE);

          if ((controls = gb_tab_get_controls (tab)))
            gtk_stack_set_visible_child (stack->priv->controls, controls);
        }
      else
        {
          gtk_widget_set_sensitive (GTK_WIDGET (stack->priv->close), FALSE);
        }

      g_clear_object (&tab);
    }
}

GbTab *
gb_tab_stack_get_active (GbTabStack *stack)
{
  g_return_val_if_fail (GB_IS_TAB_STACK (stack), NULL);

  return GB_TAB (gtk_stack_get_visible_child (stack->priv->stack));
}

static void
gb_tab_stack_tab_closed (GbTabStack *stack,
                         GbTab      *tab)
{
  g_return_if_fail (GB_IS_TAB_STACK (stack));
  g_return_if_fail (GB_IS_TAB (tab));

  gb_tab_stack_remove_tab (stack, tab);
}

static void
gb_tab_stack_add_tab (GbTabStack *stack,
                      GbTab      *tab)
{
  GtkTreeIter iter;
  GtkWidget *controls;

  g_return_if_fail (GB_IS_TAB_STACK (stack));
  g_return_if_fail (GB_IS_TAB (tab));

  gtk_list_store_append (stack->priv->store, &iter);
  g_object_freeze_notify (G_OBJECT (stack->priv->stack));
  gtk_list_store_set (stack->priv->store, &iter, 0, tab, -1);
  gtk_container_add (GTK_CONTAINER (stack->priv->stack), GTK_WIDGET (tab));
  if ((controls = gb_tab_get_controls (tab)))
    gtk_container_add (GTK_CONTAINER (stack->priv->controls), controls);
  g_object_thaw_notify (G_OBJECT (stack->priv->stack));
  gtk_combo_box_set_active_iter (stack->priv->combo, &iter);

  /* TODO: need to disconnect on (re)move */
  g_signal_connect_object (tab,
                           "close",
                           G_CALLBACK (gb_tab_stack_tab_closed),
                           stack,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (tab,
                           "notify::title",
                           G_CALLBACK (gb_tab_stack_queue_draw),
                           stack,
                           G_CONNECT_SWAPPED);

  gtk_widget_show (GTK_WIDGET (stack->priv->header_box));

  g_signal_emit (stack, gSignals [CHANGED], 0);
}

static void
gb_tab_stack_add (GtkContainer *container,
                  GtkWidget    *widget)
{
  GbTabStack *stack = (GbTabStack *)container;

  g_return_if_fail (GB_IS_TAB_STACK (stack));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (GB_IS_TAB (widget))
    gb_tab_stack_add_tab (stack, GB_TAB (widget));
  else
    GTK_CONTAINER_CLASS (gb_tab_stack_parent_class)->add (container, widget);
}

static void
gb_tab_stack_combobox_text_func (GtkCellLayout   *cell_layout,
                                 GtkCellRenderer *cell,
                                 GtkTreeModel    *tree_model,
                                 GtkTreeIter     *iter,
                                 gpointer         data)
{
  const gchar *title = NULL;
  GbTab *tab = NULL;

  gtk_tree_model_get (tree_model, iter, 0, &tab, -1);

  if (GB_IS_TAB (tab))
    title = gb_tab_get_title (tab);
  if (!title)
    title = _("untitled");

  if (gb_tab_get_dirty (tab))
    {
      gchar *str;

      str = g_strdup_printf ("%s •", title);
      g_object_set (cell, "text", str, NULL);
      g_free (str);
    }
  else
    g_object_set (cell, "text", title, NULL);

  g_clear_object (&tab);
}

static void
gb_tab_stack_grab_focus (GtkWidget *widget)
{
  GbTabStack *stack = (GbTabStack *)widget;
  GtkWidget *child;

  g_return_if_fail (GB_IS_TAB_STACK (stack));

  child = gtk_stack_get_visible_child (stack->priv->stack);

  if (child)
    gtk_widget_grab_focus (child);
}

static GbTabGrid *
get_grid (GbTabStack *stack)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (stack);

  while (widget && !GB_IS_TAB_GRID (widget))
    widget = gtk_widget_get_parent (widget);

  return (GbTabGrid *)widget;
}

static void
gb_tab_stack_do_close_tab (GbTabStack *stack,
                           GdkEvent   *event,
                           GtkButton  *button)
{
  GbTabGrid *grid;
  GbTab *tab;

  g_return_if_fail (GB_IS_TAB_STACK (stack));

  grid = get_grid (stack);
  tab = gb_tab_stack_get_active (stack);

  if (grid && tab)
    gb_tab_stack_remove_tab (stack, tab);
}

GtkTreeModel *
gb_tab_stack_get_model (GbTabStack *stack)
{
  g_return_val_if_fail (GB_IS_TAB_STACK (stack), NULL);

  return gtk_combo_box_get_model (stack->priv->combo);
}

void
gb_tab_stack_set_model (GbTabStack   *stack,
                        GtkTreeModel *model)
{
  g_return_if_fail (GB_IS_TAB_STACK (stack));

  gtk_combo_box_set_model (stack->priv->combo, model);
  g_object_notify_by_pspec (G_OBJECT (stack), gParamSpecs [PROP_MODEL]);
}

static void
gb_tab_stack_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_tab_stack_parent_class)->finalize (object);
}

static void
gb_tab_stack_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GbTabStack *stack = GB_TAB_STACK(object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, gb_tab_stack_get_model (stack));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gb_tab_stack_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GbTabStack *stack = GB_TAB_STACK(object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gb_tab_stack_set_model (stack, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gb_tab_stack_class_init (GbTabStackClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_tab_stack_finalize;
  object_class->get_property = gb_tab_stack_get_property;
  object_class->set_property = gb_tab_stack_set_property;

  container_class->add = gb_tab_stack_add;

  widget_class->grab_focus = gb_tab_stack_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-tab-stack.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GbTabStack, controls);
  gtk_widget_class_bind_template_child_private (widget_class, GbTabStack, close);
  gtk_widget_class_bind_template_child_private (widget_class, GbTabStack, combo);
  gtk_widget_class_bind_template_child_private (widget_class, GbTabStack, header_box);
  gtk_widget_class_bind_template_child_private (widget_class, GbTabStack, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GbTabStack, stack_menu);
  gtk_widget_class_bind_template_child_private (widget_class, GbTabStack, store);

  gParamSpecs [PROP_MODEL] =
    g_param_spec_object ("model",
                         _("Model"),
                         _("The model containing the buffers."),
                         GTK_TYPE_TREE_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MODEL,
                                   gParamSpecs [PROP_MODEL]);

  gSignals [CHANGED] = g_signal_new ("changed",
                                     GB_TYPE_TAB_STACK,
                                     G_SIGNAL_RUN_FIRST,
                                     G_STRUCT_OFFSET (GbTabStackClass, changed),
                                     NULL,
                                     NULL,
                                     g_cclosure_marshal_generic,
                                     G_TYPE_NONE,
                                     0);

  g_type_ensure (GB_TYPE_TAB);
}

static void
gb_tab_stack_init (GbTabStack *stack)
{
  GtkCellLayout *layout;
  GtkCellRenderer *cell;
  GApplication *app;
  GMenu *menu;

  stack->priv = gb_tab_stack_get_instance_private (stack);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (stack),
                                  GTK_ORIENTATION_VERTICAL);

  gtk_widget_init_template (GTK_WIDGET (stack));

  g_signal_connect_object (stack->priv->combo,
                           "changed",
                           G_CALLBACK (gb_tab_stack_combobox_changed),
                           stack,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack->priv->close,
                           "clicked",
                           G_CALLBACK (gb_tab_stack_do_close_tab),
                           stack,
                           G_CONNECT_SWAPPED);

  layout = GTK_CELL_LAYOUT (stack->priv->combo);
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (layout, cell, TRUE);
  gtk_cell_layout_set_cell_data_func (layout, cell,
                                      gb_tab_stack_combobox_text_func,
                                      NULL, NULL);
  gtk_cell_renderer_text_set_fixed_height_from_font (
      GTK_CELL_RENDERER_TEXT (cell), 1);

  app = g_application_get_default ();
  menu = gtk_application_get_menu_by_id (GTK_APPLICATION (app), "stack-menu");
  gtk_menu_button_set_menu_model (stack->priv->stack_menu, G_MENU_MODEL (menu));
}
