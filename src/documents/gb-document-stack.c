/* gb-document-stack.c
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

#define G_LOG_DOMAIN "document-stack"

#include <glib/gi18n.h>

#include "gb-document-menu-button.h"
#include "gb-document-stack.h"
#include "gb-glib.h"

struct _GbDocumentStackPrivate
{
  /* Objects ownen by GbDocumentStack */
  GbDocumentManager    *document_manager;

  /* Weak references */
  GbDocumentView       *active_view;

  /* GtkWidgets owned by GtkWidgetClass template */
  GbDocumentMenuButton *document_button;
  GtkStack             *controls;
  GtkButton            *close;
  GtkStack             *stack;
  GtkMenuButton        *stack_menu;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDocumentStack, gb_document_stack, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_ACTIVE_VIEW,
  PROP_DOCUMENT_MANAGER,
  LAST_PROP
};

enum {
  CREATE_VIEW,
  EMPTY,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GtkWidget *
gb_document_stack_new (void)
{
  return g_object_new (GB_TYPE_DOCUMENT_STACK, NULL);
}

static void
gb_document_stack_remove_view (GbDocumentStack *stack,
                               GbDocumentView  *view)
{
  GbDocument *document = NULL;
  GtkWidget *visible_child;
  GtkWidget *controls;
  gboolean visible;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));
  g_return_if_fail (GB_IS_DOCUMENT_VIEW (view));

  /* Release our weak pointer */
  if (view == stack->priv->active_view)
    gb_clear_weak_pointer (&stack->priv->active_view);

  /* Notify the document view it is being closed */
  gb_document_view_close (view);

  /* Remove the document view and its controls */
  controls = gb_document_view_get_controls (view);
  if (controls)
    gtk_container_remove (GTK_CONTAINER (stack->priv->controls), controls);
  gtk_container_remove (GTK_CONTAINER (stack->priv->stack), GTK_WIDGET (view));

  /*
   * Set the visible child to the first document view. We probably want to
   * be more intelligent about this in the future. visible_child may be
   * NULL, which will clear the title string in the menu button.
   */
  visible_child = gtk_stack_get_visible_child (stack->priv->stack);
  if (visible_child)
    document = gb_document_view_get_document (GB_DOCUMENT_VIEW (visible_child));
  gb_document_menu_button_select_document (stack->priv->document_button,
                                           document);

  /* Only show close and stack menu if we have children */
  visible = (visible_child != NULL);
  gtk_widget_set_visible (GTK_WIDGET (stack->priv->close), visible);
  gtk_widget_set_visible (GTK_WIDGET (stack->priv->stack_menu), visible);

  if (!visible_child)
    g_signal_emit (stack, gSignals [EMPTY], 0);
}

/**
 * gb_document_stack_get_document_manager:
 *
 * Fetches the document manager for the stack.
 *
 * Returns: (transfer none): A #GbDocumentManager
 */
GbDocumentManager *
gb_document_stack_get_document_manager (GbDocumentStack *stack)
{
  g_return_val_if_fail (GB_IS_DOCUMENT_STACK (stack), NULL);

  return stack->priv->document_manager;
}

/**
 * gb_document_stack_set_document_manager:
 * @document_manager: A #GbDocumentManager.
 *
 * Sets the document manager for the stack. All existing views will be removed
 * if the #GbDocumentManager is different than the current document manager.
 */
void
gb_document_stack_set_document_manager (GbDocumentStack   *stack,
                                        GbDocumentManager *document_manager)
{
  GbDocumentStackPrivate *priv;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));
  g_return_if_fail (!document_manager ||
                    GB_IS_DOCUMENT_MANAGER (document_manager));

  priv = stack->priv;

  if (document_manager != priv->document_manager)
    {
      if (priv->document_manager)
        {
          GList *list;
          GList *iter;

          /* Release our document manager */
          g_clear_object (&priv->document_manager);
          gb_document_menu_button_set_document_manager (priv->document_button,
                                                        NULL);

          /* Remove any views lingering in here */
          list = gtk_container_get_children (GTK_CONTAINER (priv->stack));
          for (iter = list; iter; iter = iter->next)
            gb_document_stack_remove_view (stack, iter->data);
          g_list_free (list);
        }

      if (document_manager)
        {
          priv->document_manager = g_object_ref (document_manager);
          gb_document_menu_button_set_document_manager (priv->document_button,
                                                        document_manager);
        }

      g_object_notify_by_pspec (G_OBJECT (stack),
                                gParamSpecs [PROP_DOCUMENT_MANAGER]);
    }
}

/**
 * gb_document_stack_get_active_view:
 *
 * Fetches the active view for the document stack.
 *
 * Returns: (transfer none): A #GbDocumentView or %NULL.
 */
GbDocumentView *
gb_document_stack_get_active_view (GbDocumentStack *stack)
{
  GtkWidget *child;

  g_return_val_if_fail (GB_IS_DOCUMENT_STACK (stack), NULL);

  child = gtk_stack_get_visible_child (stack->priv->stack);
  if (GB_IS_DOCUMENT_VIEW (child))
    return GB_DOCUMENT_VIEW (child);

  return NULL;
}

void
gb_document_stack_set_active_view (GbDocumentStack *stack,
                                   GbDocumentView  *active_view)
{
  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));
  g_return_if_fail (!active_view || GB_IS_DOCUMENT_VIEW (active_view));

  if (active_view != stack->priv->active_view)
    {
      gb_clear_weak_pointer (&stack->priv->active_view);
      gb_set_weak_pointer (active_view, &stack->priv->active_view);

      if (active_view)
        {
          GtkWidget *controls;

          gtk_stack_set_visible_child (stack->priv->stack,
                                       GTK_WIDGET (active_view));

          controls = gb_document_view_get_controls (active_view);

          if (controls)
            gtk_stack_set_visible_child (stack->priv->controls, controls);
        }

      g_object_notify_by_pspec (G_OBJECT (stack),
                                gParamSpecs [PROP_ACTIVE_VIEW]);
    }
}

/**
 * gb_document_stack_find_with_document:
 *
 * Finds the first #GbDocumentView containing @document.
 *
 * Returns: (transfer none): A #GbDocumentView or %NULL.
 */
GtkWidget *
gb_document_stack_find_with_document (GbDocumentStack *stack,
                                      GbDocument      *document)
{
  GbDocumentView *ret = NULL;
  GList *list;
  GList *iter;

  g_return_val_if_fail (GB_IS_DOCUMENT_STACK (stack), NULL);
  g_return_val_if_fail (GB_IS_DOCUMENT (document), NULL);

  list = gtk_container_get_children (GTK_CONTAINER (stack->priv->stack));

  for (iter = list; iter; iter = iter->next)
    {
      GbDocument *value;

      value = gb_document_view_get_document (GB_DOCUMENT_VIEW (iter->data));
      if (!value)
        continue;

      if (document == value)
        {
          ret = GB_DOCUMENT_VIEW (iter->data);
          break;
        }
    }

  g_list_free (list);

  return GTK_WIDGET (ret);
}

/**
 * gb_document_stack_find_with_type:
 *
 * Finds the first #GbDocumentView of type @type_id. The view may be a subclass
 * of type #GType provided.
 *
 * Returns: (transfer none): A #GbDocumentView or %NULL.
 */
GtkWidget *
gb_document_stack_find_with_type (GbDocumentStack *stack,
                                  GType            type_id)
{
  GbDocumentView *ret = NULL;
  GList *list;
  GList *iter;

  g_return_val_if_fail (GB_IS_DOCUMENT_STACK (stack), NULL);
  g_return_val_if_fail (g_type_is_a (type_id, GB_TYPE_DOCUMENT_VIEW), NULL);

  list = gtk_container_get_children (GTK_CONTAINER (stack->priv->stack));

  for (iter = list; iter; iter = iter->next)
    {
      GbDocument *value;

      value = gb_document_view_get_document (GB_DOCUMENT_VIEW (iter->data));
      if (!value)
        continue;

      if (g_type_is_a (G_TYPE_FROM_INSTANCE (value), type_id))
        {
          ret = GB_DOCUMENT_VIEW (iter->data);
          break;
        }
    }

  g_list_free (list);

  return GTK_WIDGET (ret);
}

void
gb_document_stack_focus_document (GbDocumentStack *stack,
                                  GbDocument      *document)
{
  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  gb_document_menu_button_select_document (stack->priv->document_button,
                                           document);
}

static void
gb_document_stack_document_selected (GbDocumentStack      *stack,
                                     GbDocument           *document,
                                     GbDocumentMenuButton *button)
{
  GtkWidget *view;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));
  g_return_if_fail (GB_IS_DOCUMENT (document));
  g_return_if_fail (GB_IS_DOCUMENT_MENU_BUTTON (button));

  view = gb_document_stack_find_with_document (stack, document);

  if (!view)
    {
      GtkWidget *controls;

      view = gb_document_create_view (document);

      if (!view)
        {
          g_warning ("Failed to create view");
          return;
        }

      gtk_container_add (GTK_CONTAINER (stack->priv->stack), view);
      controls = gb_document_view_get_controls (GB_DOCUMENT_VIEW (view));
      if (controls)
        gtk_container_add (GTK_CONTAINER (stack->priv->controls), controls);
    }

  gtk_widget_set_visible (GTK_WIDGET (stack->priv->close), TRUE);
  gtk_widget_set_visible (GTK_WIDGET (stack->priv->stack_menu), TRUE);
  gb_document_stack_set_active_view (stack, GB_DOCUMENT_VIEW (view));
  gtk_widget_grab_focus (view);

  return;
}

static void
gb_document_stack_close (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  GbDocumentStack *stack = user_data;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  if (stack->priv->active_view)
    gb_document_stack_remove_view (stack, stack->priv->active_view);
}

static void
gb_document_stack_grab_focus (GtkWidget *widget)
{
  GbDocumentStack *stack = (GbDocumentStack *)widget;
  GbDocumentView *active_view;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  active_view = gb_document_stack_get_active_view (stack);
  if (active_view)
    gtk_widget_grab_focus (GTK_WIDGET (active_view));
}

static void
gb_document_stack_constructed (GObject *object)
{
  GbDocumentStack *stack = (GbDocumentStack *)object;
  GApplication *app;

  G_OBJECT_CLASS (gb_document_stack_parent_class)->constructed (object);

  app = g_application_get_default ();

  if (GTK_IS_APPLICATION (app))
    {
      GMenu *menu;

      menu = gtk_application_get_menu_by_id (GTK_APPLICATION (app),
                                             "gb-document-stack-menu");

      if (menu)
        gtk_menu_button_set_menu_model (stack->priv->stack_menu,
                                        G_MENU_MODEL (menu));
    }

  g_signal_connect_object (stack->priv->document_button,
                           "document-selected",
                           G_CALLBACK (gb_document_stack_document_selected),
                           stack,
                           G_CONNECT_SWAPPED);
}

static void
gb_document_stack_move_document_left (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
  GbDocumentStack *stack = user_data;
  GbDocumentView *view;
  GbDocument *document;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  view = gb_document_stack_get_active_view (stack);
  if (!view)
    return;

  document = gb_document_view_get_document (view);
  if (!document)
    return;

  g_signal_emit (stack, gSignals [CREATE_VIEW], 0, document,
                 GB_DOCUMENT_SPLIT_LEFT);

  gb_document_stack_remove_view (stack, view);
}

static void
gb_document_stack_move_document_right (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  GbDocumentStack *stack = user_data;
  GbDocumentView *view;
  GbDocument *document;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  view = gb_document_stack_get_active_view (stack);
  if (!view)
    return;

  document = gb_document_view_get_document (view);
  if (!document)
    return;

  g_signal_emit (stack, gSignals [CREATE_VIEW], 0, document,
                 GB_DOCUMENT_SPLIT_RIGHT);

  gb_document_stack_remove_view (stack, view);
}

static void
gb_document_stack_split_document_left (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  GbDocumentStack *stack = user_data;
  GbDocumentView *view;
  GbDocument *document;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  view = gb_document_stack_get_active_view (stack);
  if (!view)
    return;

  document = gb_document_view_get_document (view);
  if (!document)
    return;

  g_signal_emit (stack, gSignals [CREATE_VIEW], 0, document,
                 GB_DOCUMENT_SPLIT_LEFT);
}

static void
gb_document_stack_split_document_right (GSimpleAction *action,
                                        GVariant      *parameter,
                                        gpointer       user_data)
{
  GbDocumentStack *stack = user_data;
  GbDocumentView *view;
  GbDocument *document;

  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  view = gb_document_stack_get_active_view (stack);
  if (!view)
    return;

  document = gb_document_view_get_document (view);
  if (!document)
    return;

  g_signal_emit (stack, gSignals [CREATE_VIEW], 0, document,
                 GB_DOCUMENT_SPLIT_RIGHT);
}

static void
gb_document_stack_finalize (GObject *object)
{
  GbDocumentStackPrivate *priv = GB_DOCUMENT_STACK (object)->priv;

  gb_clear_weak_pointer (&priv->active_view);
  g_clear_object (&priv->document_manager);

  G_OBJECT_CLASS (gb_document_stack_parent_class)->finalize (object);
}

static void
gb_document_stack_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbDocumentStack *self = GB_DOCUMENT_STACK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_object (value, gb_document_stack_get_active_view (self));
      break;

    case PROP_DOCUMENT_MANAGER:
      g_value_set_object (value, gb_document_stack_get_document_manager (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_document_stack_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbDocumentStack *self = GB_DOCUMENT_STACK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      gb_document_stack_set_active_view (self, g_value_get_object (value));
      break;

    case PROP_DOCUMENT_MANAGER:
      gb_document_stack_set_document_manager (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_document_stack_class_init (GbDocumentStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_document_stack_constructed;
  object_class->finalize = gb_document_stack_finalize;
  object_class->get_property = gb_document_stack_get_property;
  object_class->set_property = gb_document_stack_set_property;

  widget_class->grab_focus = gb_document_stack_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-document-stack.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GbDocumentStack, close);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GbDocumentStack, stack);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GbDocumentStack, stack_menu);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GbDocumentStack, controls);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GbDocumentStack, document_button);

  gParamSpecs [PROP_ACTIVE_VIEW] =
    g_param_spec_object ("active-view",
                         _("Active View"),
                         _("The active view within the stack."),
                         GB_TYPE_DOCUMENT_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACTIVE_VIEW,
                                   gParamSpecs [PROP_ACTIVE_VIEW]);

  gParamSpecs [PROP_DOCUMENT_MANAGER] =
    g_param_spec_object ("document-manager",
                         _("Document Manager"),
                         _("The document manager for the stack."),
                         GB_TYPE_DOCUMENT_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT_MANAGER,
                                   gParamSpecs [PROP_DOCUMENT_MANAGER]);

  gSignals [CREATE_VIEW] =
    g_signal_new ("create-view",
                  GB_TYPE_DOCUMENT_STACK,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbDocumentStackClass, create_view),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  GB_TYPE_DOCUMENT,
                  GB_TYPE_DOCUMENT_SPLIT);

  /**
   * GbDocumentStack::empty:
   *
   * This signal is emitted when the last document view has been closed.
   * The parent grid may want to destroy the grid when this occurs.
   */
  gSignals [EMPTY] =
    g_signal_new ("empty",
                  GB_TYPE_DOCUMENT_STACK,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbDocumentStackClass, empty),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  0);

  g_type_ensure (GB_TYPE_DOCUMENT_MENU_BUTTON);
}

static void
gb_document_stack_init (GbDocumentStack *self)
{
  const GActionEntry entries[] = {
    { "move-document-left", gb_document_stack_move_document_left },
    { "move-document-right", gb_document_stack_move_document_right },
    { "split-document-left", gb_document_stack_split_document_left },
    { "split-document-right", gb_document_stack_split_document_right },
    { "close", gb_document_stack_close },
  };
  GSimpleActionGroup *actions;

  self->priv = gb_document_stack_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries,
                                   G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "stack",
                                  G_ACTION_GROUP (actions));
  g_clear_object (&actions);
}
