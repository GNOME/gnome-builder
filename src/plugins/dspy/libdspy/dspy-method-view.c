/* dspy-method-view.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#define G_LOG_DOMAIN "dspy-method-view"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "dspy-method-view.h"

typedef struct
{
  DspyMethodInvocation *invocation;
  DzlBindingGroup      *bindings;
  GCancellable         *cancellable;
  GArray               *durations;

  GtkLabel             *label_interface;
  GtkLabel             *label_object_path;
  GtkLabel             *label_method;
  GtkLabel             *label_avg;
  GtkLabel             *label_min;
  GtkLabel             *label_max;
  GtkButton            *button;
  GtkButton            *copy_button;
  GtkTextBuffer        *buffer_params;
  GtkTextBuffer        *buffer_reply;
  GtkTextView          *textview_params;

  guint                 busy : 1;
} DspyMethodViewPrivate;

typedef struct
{
  DspyMethodView *self;
  GTimer         *timer;
} Execute;

G_DEFINE_TYPE_WITH_PRIVATE (DspyMethodView, dspy_method_view, DZL_TYPE_BIN)

enum {
  PROP_0,
  PROP_INVOCATION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
execute_free (Execute *state)
{
  if (state != NULL)
    {
      g_clear_pointer (&state->timer, g_timer_destroy);
      g_clear_object (&state->self);
      g_slice_free (Execute, state);
    }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Execute, execute_free)

/**
 * dspy_method_view_new:
 *
 * Create a new #DspyMethodView.
 *
 * Returns: (transfer full): a newly created #DspyMethodView
 */
GtkWidget *
dspy_method_view_new (void)
{
  return g_object_new (DSPY_TYPE_METHOD_VIEW, NULL);
}

static void
update_timings (DspyMethodView *self)
{
  DspyMethodViewPrivate *priv = dspy_method_view_get_instance_private (self);
  g_autofree gchar *mean_str = NULL;
  g_autofree gchar *min_str = NULL;
  g_autofree gchar *max_str = NULL;
  gdouble min = G_MAXDOUBLE;
  gdouble max = -G_MAXDOUBLE;
  gdouble total = 0;
  gdouble mean = 0;

  g_assert (DSPY_IS_METHOD_VIEW (self));
  g_assert (priv->durations != NULL);

  if (priv->durations->len == 0)
    {
      gtk_label_set_label (priv->label_avg, NULL);
      gtk_label_set_label (priv->label_min, NULL);
      gtk_label_set_label (priv->label_max, NULL);
      return;
    }

  for (guint i = 0; i < priv->durations->len; i++)
    {
      gdouble val = g_array_index (priv->durations, gdouble, i);

      total += val;
      min = MIN (min, val);
      max = MAX (max, val);
    }

  mean = total / (gdouble)priv->durations->len;

  mean_str = g_strdup_printf ("%0.4lf", mean);
  min_str = g_strdup_printf ("%0.4lf", min);
  max_str = g_strdup_printf ("%0.4lf", max);

  gtk_label_set_label (priv->label_avg, mean_str);
  gtk_label_set_label (priv->label_min, min_str);
  gtk_label_set_label (priv->label_max, max_str);
}

static gboolean
variant_to_string_transform (GBinding     *binding,
                             const GValue *from_value,
                             GValue       *to_value,
                             gpointer      user_data)
{
  GVariant *v = g_value_get_variant (from_value);
  if (v != NULL)
    g_value_take_string (to_value, g_variant_print (v, FALSE));
  else
    g_value_set_string (to_value, "");
  return TRUE;
}

static void
dspy_method_view_execute_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  DspyMethodInvocation *invocation = (DspyMethodInvocation *)object;
  g_autoptr(Execute) state = user_data;
  DspyMethodViewPrivate *priv;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  gdouble elapsed;

  g_assert (DSPY_IS_METHOD_INVOCATION (invocation));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (state->timer != NULL);
  g_assert (DSPY_IS_METHOD_VIEW (state->self));

  priv = dspy_method_view_get_instance_private (state->self);
  priv->busy = FALSE;

  g_timer_stop (state->timer);
  elapsed = g_timer_elapsed (state->timer, NULL);
  g_array_append_val (priv->durations, elapsed);

  if (!(reply = dspy_method_invocation_execute_finish (invocation, result, &error)))
    {
      if (priv->invocation == invocation)
        gtk_text_buffer_set_text (priv->buffer_reply, error->message, -1);
    }
  else
    {
      if (priv->invocation == invocation)
        {
          g_autofree gchar *replystr = g_variant_print (reply, TRUE);
          gtk_text_buffer_set_text (priv->buffer_reply, replystr, -1);
        }
    }

  update_timings (state->self);

  gtk_button_set_label (priv->button, _("Execute"));
}

static GVariant *
get_variant_for_text_buffer (GtkTextBuffer       *buffer,
                             const GVariantType  *type,
                             GError             **error)
{
  g_autofree gchar *text = NULL;
  GtkTextIter begin, end;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  text = g_strstrip (gtk_text_buffer_get_text (buffer, &begin, &end, TRUE));

  if (text[0] != '(')
    {
      g_autofree gchar *tmp = text;
      text = g_strdup_printf ("(%s,)", tmp);
      gtk_text_buffer_set_text (buffer, text, -1);
    }

  return g_variant_parse (type, text, NULL, NULL, error);
}

static void
dspy_method_view_button_clicked_cb (DspyMethodView *self,
                                    GtkButton      *button)
{
  DspyMethodViewPrivate *priv = dspy_method_view_get_instance_private (self);
  g_autoptr(GVariant) params = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *signature;
  Execute *state;

  g_assert (DSPY_IS_METHOD_VIEW (self));
  g_assert (GTK_IS_BUTTON (button));

  /* Always cancel anything in flight */
  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);

  if (priv->busy)
    return;

  if (priv->invocation == NULL)
    return;

  g_assert (priv->busy == FALSE);
  g_assert (priv->cancellable == NULL);

  signature = dspy_method_invocation_get_signature (priv->invocation);

  if (!signature || !signature[0])
    signature = NULL;

  if (!(params = get_variant_for_text_buffer (priv->buffer_params,
                                              (const GVariantType *)signature,
                                              &error)))
    {
      gtk_text_buffer_set_text (priv->buffer_reply, error->message, -1);
      return;
    }

  dspy_method_invocation_set_parameters (priv->invocation, params);

  priv->busy = TRUE;
  priv->cancellable = g_cancellable_new ();

  gtk_text_buffer_set_text (priv->buffer_reply, "", -1);

  state = g_slice_new0 (Execute);
  state->self = g_object_ref (self);
  state->timer = g_timer_new ();

  dspy_method_invocation_execute_async (priv->invocation,
                                        priv->cancellable,
                                        dspy_method_view_execute_cb,
                                        state);

  gtk_button_set_label (priv->button, _("Cancel"));
}

static void
dspy_method_view_invoke_method (GtkWidget *widget,
                                gpointer   user_data)
{
  DspyMethodView *self = user_data;
  DspyMethodViewPrivate *priv = dspy_method_view_get_instance_private (self);

  g_assert (DSPY_IS_METHOD_VIEW (self));

  gtk_widget_activate (GTK_WIDGET (priv->button));
}

static void
copy_button_clicked_cb (DspyMethodView *self,
                        GtkButton      *button)
{
  DspyMethodViewPrivate *priv = dspy_method_view_get_instance_private (self);
  g_autofree gchar *text = NULL;
  GtkClipboard *clipboard;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (DSPY_IS_METHOD_VIEW (self));
  g_assert (GTK_IS_BUTTON (button));

  if (!gtk_text_buffer_get_selection_bounds (priv->buffer_reply, &begin, &end))
    gtk_text_buffer_get_bounds (priv->buffer_reply, &begin, &end);

  text = gtk_text_iter_get_slice (&begin, &end);
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self), GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, text, -1);
}

static void
dspy_method_view_finalize (GObject *object)
{
  DspyMethodView *self = (DspyMethodView *)object;
  DspyMethodViewPrivate *priv = dspy_method_view_get_instance_private (self);

  dzl_binding_group_set_source (priv->bindings, NULL);

  g_clear_object (&priv->invocation);
  g_clear_object (&priv->bindings);
  g_clear_object (&priv->cancellable);
  g_clear_pointer (&priv->durations, g_array_unref);

  G_OBJECT_CLASS (dspy_method_view_parent_class)->finalize (object);
}

static void
dspy_method_view_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  DspyMethodView *self = DSPY_METHOD_VIEW (object);

  switch (prop_id)
    {
    case PROP_INVOCATION:
      g_value_set_object (value, dspy_method_view_get_invocation (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_method_view_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  DspyMethodView *self = DSPY_METHOD_VIEW (object);

  switch (prop_id)
    {
    case PROP_INVOCATION:
      dspy_method_view_set_invocation (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
dspy_method_view_class_init (DspyMethodViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = dspy_method_view_finalize;
  object_class->get_property = dspy_method_view_get_property;
  object_class->set_property = dspy_method_view_set_property;

  properties [PROP_INVOCATION] =
    g_param_spec_object ("invocation",
                         "Invocation",
                         "The method invocation to view",
                         DSPY_TYPE_METHOD_INVOCATION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/dspy/dspy-method-view.ui");
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, buffer_params);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, buffer_reply);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, button);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, copy_button);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, label_avg);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, label_interface);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, label_max);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, label_method);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, label_min);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, label_object_path);
  gtk_widget_class_bind_template_child_private (widget_class, DspyMethodView, textview_params);
}

static void
dspy_method_view_init (DspyMethodView *self)
{
  DspyMethodViewPrivate *priv = dspy_method_view_get_instance_private (self);
  DzlShortcutController *controller;

  gtk_widget_init_template (GTK_WIDGET (self));

  priv->durations = g_array_new (FALSE, FALSE, sizeof (gdouble));

  priv->bindings = dzl_binding_group_new ();
  dzl_binding_group_bind (priv->bindings, "interface", priv->label_interface, "label", 0);
  dzl_binding_group_bind (priv->bindings, "method", priv->label_method, "label", 0);
  dzl_binding_group_bind (priv->bindings, "object-path", priv->label_object_path, "label", 0);
  dzl_binding_group_bind_full (priv->bindings, "parameters", priv->buffer_params, "text", 0,
                               variant_to_string_transform, NULL, NULL, NULL);

  g_signal_connect_object (priv->button,
                           "clicked",
                           G_CALLBACK (dspy_method_view_button_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->copy_button,
                           "clicked",
                           G_CALLBACK (copy_button_clicked_cb),
                           self,
                           G_CONNECT_SWAPPED);

  controller = dzl_shortcut_controller_find (GTK_WIDGET (priv->textview_params));

  dzl_shortcut_controller_add_command_callback (controller,
                                                "org.gnome.dspy.invoke-method",
                                                "<Primary>Return",
                                                DZL_SHORTCUT_PHASE_DISPATCH,
                                                dspy_method_view_invoke_method,
                                                self,
                                                NULL);
}

void
dspy_method_view_set_invocation (DspyMethodView       *self,
                                 DspyMethodInvocation *invocation)
{
  DspyMethodViewPrivate *priv = dspy_method_view_get_instance_private (self);

  g_return_if_fail (DSPY_IS_METHOD_VIEW (self));
  g_return_if_fail (!invocation || DSPY_IS_METHOD_INVOCATION (invocation));

  if (g_set_object (&priv->invocation, invocation))
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);

      dzl_binding_group_set_source (priv->bindings, invocation);
      gtk_text_buffer_set_text (priv->buffer_reply, "", -1);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INVOCATION]);
    }
}

/**
 * dspy_method_view_get_invocation:
 *
 * Returns: (transfer none) (nullable): a #DspyMethodInvocation or %NULL
 */
DspyMethodInvocation *
dspy_method_view_get_invocation (DspyMethodView *self)
{
  DspyMethodViewPrivate *priv = dspy_method_view_get_instance_private (self);

  g_return_val_if_fail (DSPY_IS_METHOD_VIEW (self), NULL);

  return priv->invocation;
}
