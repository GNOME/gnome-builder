/* ide-key-mode.c
 *
 * Copyright (C) 2015 Alexander Larsson <alexl@redhat.com>
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

#define G_LOG_DOMAIN "ide-source-view-mode"

#include <glib/gi18n.h>

#include "ide-source-view-mode.h"

typedef struct
{
  GtkWidget             *view;
  char                  *name;
  IdeSourceViewModeType  type;
} IdeSourceViewModePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSourceViewMode, ide_source_view_mode, GTK_TYPE_WIDGET)

static void
ide_source_view_mode_finalize (GObject *object)
{
  IdeSourceViewMode *self = (IdeSourceViewMode *)object;
  IdeSourceViewModePrivate *priv = ide_source_view_mode_get_instance_private (self);

  g_clear_object (&priv->view);
  g_free (priv->name);

  G_OBJECT_CLASS (ide_source_view_mode_parent_class)->finalize (object);
}

static void
proxy_closure_marshal (GClosure     *closure,
                       GValue       *return_value,
                       guint         n_param_values,
                       const GValue *param_values,
                       gpointer      invocation_hint,
                       gpointer      marshal_data)
{
  IdeSourceViewMode *mode;
  IdeSourceViewModePrivate *priv;
  GValue *param_copy;

  mode = IDE_SOURCE_VIEW_MODE (g_value_get_object (&param_values[0]));
  priv = ide_source_view_mode_get_instance_private (mode);

  param_copy = g_memdup (param_values, sizeof (GValue) * n_param_values);

  param_copy[0].data[0].v_pointer = priv->view;
  g_signal_emitv (param_copy,
                  GPOINTER_TO_INT (closure->data),
                  0,
                  return_value);
  g_free (param_copy);
}

static void
proxy_all_action_signals (GType type)
{
  GClosure *proxy;
  guint *signals;
  guint n_signals, i;
  GSignalQuery query;

  signals = g_signal_list_ids (type, &n_signals);
  for (i = 0; i < n_signals; i++)
    {
      g_signal_query (signals[i], &query);

      if (query.signal_flags & G_SIGNAL_ACTION)
        {
          proxy = g_closure_new_simple (sizeof (GClosure), GINT_TO_POINTER (query.signal_id));
          g_closure_set_meta_marshal (proxy, NULL, proxy_closure_marshal);
          g_signal_newv (query.signal_name,
                         IDE_TYPE_SOURCE_VIEW_MODE,
                         G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                         proxy,
                         NULL, NULL, NULL,
                         query.return_type,
                         query.n_params,
                         (GType *)query.param_types);
        }
    }
}

static void
ide_source_view_mode_class_init (IdeSourceViewModeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set, *parent_binding_set;
  GType type;

  object_class->finalize = ide_source_view_mode_finalize;

  /* Proxy all action signals from source view */
  type = IDE_TYPE_SOURCE_VIEW;
  while (type != G_TYPE_INVALID && type != GTK_TYPE_WIDGET)
    {
      proxy_all_action_signals (type);
      type = g_type_parent (type);
    }

  /* Add unbind all entries from parent classes (which is
     really just the GtkWidget ones) so that we *only* add
     stuff via modes. Any default ones are handled in the
     normal fallback paths after mode elements are done. */
  binding_set = gtk_binding_set_by_class (klass);

  type = g_type_parent (IDE_TYPE_SOURCE_VIEW_MODE);
  while (type)
    {
      parent_binding_set = gtk_binding_set_find (g_type_name (type));
      type = g_type_parent (type);

      if (parent_binding_set)
        {
          GtkBindingEntry *entry = parent_binding_set->entries;

          while (entry != NULL)
            {
              gtk_binding_entry_skip (binding_set, entry->keyval, entry->modifiers);
              entry = entry->set_next;
            }
        }
    }
}

static void
ide_source_view_mode_init (IdeSourceViewMode *mode)
{
}

static gboolean
is_modifier_key (GdkEventKey *event)
{
  static const guint modifier_keyvals[] = {
    GDK_KEY_Shift_L, GDK_KEY_Shift_R, GDK_KEY_Shift_Lock,
    GDK_KEY_Caps_Lock, GDK_KEY_ISO_Lock, GDK_KEY_Control_L,
    GDK_KEY_Control_R, GDK_KEY_Meta_L, GDK_KEY_Meta_R,
    GDK_KEY_Alt_L, GDK_KEY_Alt_R, GDK_KEY_Super_L, GDK_KEY_Super_R,
    GDK_KEY_Hyper_L, GDK_KEY_Hyper_R, GDK_KEY_ISO_Level3_Shift,
    GDK_KEY_ISO_Next_Group, GDK_KEY_ISO_Prev_Group,
    GDK_KEY_ISO_First_Group, GDK_KEY_ISO_Last_Group,
    GDK_KEY_Mode_switch, GDK_KEY_Num_Lock, GDK_KEY_Multi_key,
    GDK_KEY_Scroll_Lock,
    0
  };
  const guint *ac_val;

  ac_val = modifier_keyvals;
  while (*ac_val)
    {
      if (event->keyval == *ac_val++)
        return TRUE;
    }

  return FALSE;
}

gboolean
_ide_source_view_mode_do_event (IdeSourceViewMode *mode,
                                GdkEventKey       *event,
                                gboolean          *remove)
{
  IdeSourceViewModePrivate *priv = ide_source_view_mode_get_instance_private (mode);
  GtkStyleContext *context;
  gboolean handled;

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW_MODE (mode), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (remove, FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (mode));

  gtk_style_context_save (context);

  gtk_style_context_add_class (context, priv->name);

  handled = gtk_bindings_activate_event (G_OBJECT (mode), event);

  gtk_style_context_restore (context);

  *remove = FALSE;
  switch (priv->type)
    {
    default:
    case IDE_SOURCE_VIEW_MODE_TYPE_TRANSIENT:
      if (handled)
        {
          *remove = TRUE;
        }
      else
        {
          if (!is_modifier_key (event))
            {
              gtk_widget_error_bell (priv->view);
              handled = TRUE;
              *remove = TRUE;
            }
        }
      break;

    case IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT:
      break;

    case IDE_SOURCE_VIEW_MODE_TYPE_MODAL:
      handled = TRUE;
      break;
    }

  return handled;
}

IdeSourceViewMode *
_ide_source_view_mode_new (GtkWidget             *view,
                           const char            *name,
                           IdeSourceViewModeType  type)
{
  IdeSourceViewModePrivate *priv;
  IdeSourceViewMode *mode;

  mode = g_object_new (IDE_TYPE_SOURCE_VIEW_MODE, NULL);
  priv = ide_source_view_mode_get_instance_private (mode);

  priv->view = g_object_ref (view);
  priv->name = g_strdup (name);
  priv->type = type;

  return g_object_ref_sink (mode);
}
