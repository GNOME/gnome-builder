/* ide-completion-view.c
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

#define G_LOG_DOMAIN "ide-completion-view"

#include "config.h"

#include "ide-completion.h"
#include "ide-completion-context.h"
#include "ide-completion-list-box.h"
#include "ide-completion-private.h"
#include "ide-completion-proposal.h"
#include "ide-completion-provider.h"
#include "ide-completion-view.h"

struct _IdeCompletionView
{
  DzlBin                parent_instance;
  IdeCompletionContext *context;
  IdeCompletionListBox *list_box;
  GtkLabel             *details;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PROPOSAL,
  N_PROPS
};

enum {
  ACTIVATE,
  MOVE_CURSOR,
  REPOSITION,
  N_SIGNALS
};

G_DEFINE_TYPE (IdeCompletionView, ide_completion_view, DZL_TYPE_BIN)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_completion_view_real_activate (IdeCompletionView *self)
{
  g_autoptr(IdeCompletionProvider) provider = NULL;
  g_autoptr(IdeCompletionProposal) proposal = NULL;
  IdeCompletion *completion;

  g_assert (IDE_IS_COMPLETION_VIEW (self));

  if (self->context == NULL ||
      !gtk_widget_get_visible (GTK_WIDGET (self)) ||
      !(completion = ide_completion_context_get_completion (self->context)) ||
      !ide_completion_list_box_get_selected (self->list_box, &provider, &proposal))
    return;

  _ide_completion_activate (completion, self->context, provider, proposal);
}

static void
ide_completion_view_real_move_cursor (IdeCompletionView *self,
                                      GtkMovementStep    step,
                                      gint               direction)
{
  g_assert (IDE_IS_COMPLETION_VIEW (self));

  if (!gtk_widget_get_visible (GTK_WIDGET (self)))
    return;

  ide_completion_list_box_move_cursor (self->list_box, step, direction);
}

static void
on_notify_proposal_cb (IdeCompletionView    *self,
                       GParamSpec           *pspec,
                       IdeCompletionListBox *list_box)
{
  g_autoptr(IdeCompletionProposal) proposal = NULL;
  g_autoptr(IdeCompletionProvider) provider = NULL;
  g_autofree gchar *comment = NULL;

  g_assert (IDE_IS_COMPLETION_VIEW (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_COMPLETION_LIST_BOX (list_box));

  if (ide_completion_list_box_get_selected (list_box, &provider, &proposal))
    comment = ide_completion_provider_get_comment (provider, proposal);

  gtk_label_set_label (self->details, comment);
  gtk_widget_set_visible (GTK_WIDGET (self->details), comment && *comment);
}

static void
ide_completion_view_notify_proposal_cb (IdeCompletionListBox *list_box,
                                        GParamSpec           *pspec,
                                        IdeCompletionView    *view)
{
  g_assert (IDE_IS_COMPLETION_LIST_BOX (list_box));
  g_assert (IDE_IS_COMPLETION_VIEW (view));

  g_object_notify_by_pspec (G_OBJECT (view), properties [PROP_PROPOSAL]);
}

static void
ide_completion_view_reposition_cb (IdeCompletionListBox *list_box,
                                   IdeCompletionView    *view)
{
  g_assert (IDE_IS_COMPLETION_VIEW (view));
  g_assert (IDE_IS_COMPLETION_LIST_BOX (list_box));

  g_signal_emit (view, signals [REPOSITION], 0);
}

static void
ide_completion_view_finalize (GObject *object)
{
  IdeCompletionView *self = (IdeCompletionView *)object;

  g_clear_object (&self->context);

  G_OBJECT_CLASS (ide_completion_view_parent_class)->finalize (object);
}

static void
ide_completion_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeCompletionView *self = IDE_COMPLETION_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_completion_view_get_context (self));
      break;

    case PROP_PROPOSAL:
      g_value_take_object (value, ide_completion_list_box_get_proposal (self->list_box));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeCompletionView *self = IDE_COMPLETION_VIEW (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_completion_view_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_view_class_init (IdeCompletionViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set = gtk_binding_set_by_class (klass);

  object_class->finalize = ide_completion_view_finalize;
  object_class->get_property = ide_completion_view_get_property;
  object_class->set_property = ide_completion_view_set_property;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The context to display in the view",
                         IDE_TYPE_COMPLETION_CONTEXT,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties [PROP_PROPOSAL] =
    g_param_spec_object ("proposal",
                         "Proposal",
                         "The selected proposal",
                         IDE_TYPE_COMPLETION_PROPOSAL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeCompletionOverlay::move-cursor:
   * @self: an #IdeCompletionOverlay
   * @direction: the amount to move and in what direction
   *
   * Make @direction positive to move forward, negative to move backwards
   *
   * Since: 3.32
   */
  signals [MOVE_CURSOR] =
    g_signal_new_class_handler ("move-cursor",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_completion_view_real_move_cursor),
                                NULL, NULL, NULL,
                                G_TYPE_NONE, 2, GTK_TYPE_MOVEMENT_STEP, G_TYPE_INT);

  /**
   * IdeCompletionOverlay::activate:
   * @self: an #IdeCompletionOverlay
   *
   * Activates the selected item in the completion window.
   *
   * Since: 3.32
   */
  signals [ACTIVATE] =
    g_signal_new_class_handler ("activate",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_completion_view_real_activate),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [ACTIVATE],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);

  /**
   * IdeCompletionView::reposition:
   *
   * Signal used to request the the container reposition itself due
   * to changes in the underlying list.
   *
   * Since: 3.32
   */
  signals [REPOSITION] =
    g_signal_new_class_handler ("reposition",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [REPOSITION],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);

  widget_class->activate_signal = signals [ACTIVATE];

  gtk_widget_class_set_css_name (widget_class, "completionview");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-sourceview/ui/ide-completion-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionView, details);
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionView, list_box);
  gtk_widget_class_bind_template_callback (widget_class, on_notify_proposal_cb);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, 0, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_DISPLAY_LINES,
                                G_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, 0, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_DISPLAY_LINES,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Down, 0, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_PAGES,
                                G_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Page_Down, 0, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_PAGES,
                                G_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Up, 0, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_PAGES,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Page_Up, 0, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_PAGES,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Home, GDK_CONTROL_MASK, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, -1);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_End, GDK_CONTROL_MASK, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, 1);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Up, GDK_CONTROL_MASK, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_PAGES,
                                G_TYPE_INT, -5);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Down, GDK_CONTROL_MASK, "move-cursor", 2,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_PAGES,
                                G_TYPE_INT, 5);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0, "activate", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, 0, "activate", 0);

  g_type_ensure (IDE_TYPE_COMPLETION_LIST_BOX);
}

static void
ide_completion_view_init (IdeCompletionView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->list_box,
                    "notify::proposal",
                    G_CALLBACK (ide_completion_view_notify_proposal_cb),
                    self);
  g_signal_connect (self->list_box,
                    "reposition",
                    G_CALLBACK (ide_completion_view_reposition_cb),
                    self);
}

/**
 * ide_completion_view_get_context:
 * @self: a #IdeCompletionView
 *
 * Gets the #IdeCompletionView:context property.
 *
 * Returns: (transfer none) (nullable): an #IdeCompletionContext or %NULL
 *
 * Since: 3.32
 */
IdeCompletionContext *
ide_completion_view_get_context (IdeCompletionView *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_VIEW (self), NULL);

  return self->context;
}

/**
 * ide_completion_view_set_context:
 * @self: a #IdeCompletionView
 *
 * Sets the #IdeCompletionContext to be visualized.
 *
 * Since: 3.32
 */
void
ide_completion_view_set_context (IdeCompletionView    *self,
                                 IdeCompletionContext *context)
{
  g_return_if_fail (IDE_IS_COMPLETION_VIEW (self));
  g_return_if_fail (!context || IDE_IS_COMPLETION_CONTEXT (context));

  if (g_set_object (&self->context, context))
    {
      ide_completion_list_box_set_context (self->list_box, context);
      gtk_widget_queue_resize (GTK_WIDGET (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
    }
}

void
_ide_completion_view_set_n_rows (IdeCompletionView *self,
                                 guint              n_rows)
{
  g_return_if_fail (IDE_IS_COMPLETION_VIEW (self));
  g_return_if_fail (n_rows > 0);
  g_return_if_fail (n_rows <= 32);

  ide_completion_list_box_set_n_rows (self->list_box, n_rows);
}

gint
_ide_completion_view_get_x_offset (IdeCompletionView *self)
{
  IdeCompletionListBoxRow *first;

  g_return_val_if_fail (IDE_IS_COMPLETION_VIEW (self), 0);

  if ((first = _ide_completion_list_box_get_first (self->list_box)))
    return _ide_completion_list_box_row_get_x_offset (first, GTK_WIDGET (self));

  return 0;
}

gboolean
_ide_completion_view_handle_key_press (IdeCompletionView *self,
                                       const GdkEventKey *event)
{
  GtkBindingSet *binding_set;
  GtkTextView *view;

  g_return_val_if_fail (IDE_IS_COMPLETION_VIEW (self), GDK_EVENT_PROPAGATE);
  g_return_val_if_fail (event != NULL, GDK_EVENT_PROPAGATE);

  /*
   * If we have a snippet active, we don't want to activate with tab since
   * that could advance the snippet (and should take precedence).
   */
  if (self->context != NULL &&
      event->keyval == GDK_KEY_Tab &&
      (view = ide_completion_context_get_view (self->context)) &&
      ide_source_view_has_snippet (IDE_SOURCE_VIEW (view)))
    return FALSE;

  /* The key-press might cause the proposal to activate as well as insert some
   * extra data. For example, a C completion provider might convert '.' to '->'
   * after inserting the completion.
   */
  if (_ide_completion_list_box_key_activates (self->list_box, event))
    {
      gtk_widget_activate (GTK_WIDGET (self));
      return GDK_EVENT_STOP;
    }

  binding_set = gtk_binding_set_by_class (G_OBJECT_GET_CLASS (self));

  return gtk_binding_set_activate (binding_set, event->keyval, event->state, G_OBJECT (self));
}

void
_ide_completion_view_move_cursor (IdeCompletionView *self,
                                  GtkMovementStep    step,
                                  gint               count)
{
  g_return_if_fail (IDE_IS_COMPLETION_VIEW (self));

  g_signal_emit (self, signals [MOVE_CURSOR], 0, step, count);
}

void
_ide_completion_view_set_font_desc (IdeCompletionView          *self,
                                    const PangoFontDescription *font_desc)
{
  g_assert (IDE_IS_COMPLETION_VIEW (self));

  _ide_completion_list_box_set_font_desc (self->list_box, font_desc);
}
