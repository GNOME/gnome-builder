/* ide-source-view-mode.c
 *
 * Copyright 2015 Alexander Larsson <alexl@redhat.com>
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-source-view-mode"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-core.h>
#include <string.h>

#include "ide-source-view.h"
#include "ide-source-view-mode.h"

struct _IdeSourceViewMode
{
  GtkWidget              parent_instance;

  GtkWidget             *view;
  char                  *name;
  char                  *display_name;
  gchar                 *default_mode;
  IdeSourceViewModeType  type;
};

G_DEFINE_TYPE (IdeSourceViewMode, ide_source_view_mode, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
get_param (IdeSourceViewMode *self,
           const gchar       *param,
           GValue            *value)
{
  GtkStyleContext *context;

  g_assert (IDE_IS_SOURCE_VIEW_MODE (self));
  g_assert (param != NULL);
  g_assert (value != NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_get_style_property (context, param, value);
}

static gboolean
get_boolean_param (IdeSourceViewMode *self,
                   const gchar       *param)
{
  GValue value = { 0 };
  gboolean ret;

  g_value_init (&value, G_TYPE_BOOLEAN);
  get_param (self, param, &value);
  ret = g_value_get_boolean (&value);
  g_value_unset (&value);

  return ret;
}

static gchar *
get_string_param (IdeSourceViewMode *self,
                  const gchar       *param)
{
  GValue value = { 0 };
  gchar *ret;

  g_value_init (&value, G_TYPE_STRING);
  get_param (self, param, &value);
  ret = g_value_dup_string (&value);
  g_value_unset (&value);

  return ret;
}

const gchar *
ide_source_view_mode_get_default_mode (IdeSourceViewMode *self)
{
  /*
   * instead of switching back to "default" mode, use this mode instead
   * if no other mode is specified.
   */
  return self->default_mode;
}

const gchar *
ide_source_view_mode_get_display_name (IdeSourceViewMode *self)
{
  return self->display_name;
}

gboolean
ide_source_view_mode_get_repeat_insert_with_count (IdeSourceViewMode *self)
{
  /*
   * If count is 10, and you type -, you will get ----------
   */
  return get_boolean_param (self, "repeat-insert-with-count");
}

gboolean
ide_source_view_mode_get_suppress_unbound (IdeSourceViewMode *self)
{
  /*
   * unknown keypresses are swallowed. you probably want to use this
   * with a transient mode.
   */
  return get_boolean_param (self, "suppress-unbound");
}

gboolean
ide_source_view_mode_get_block_cursor (IdeSourceViewMode *self)
{
  /*
   * fakes a block cursor by using overwrite mode in textview.
   * you probably want to use this with "suppress-unbound".
   */
  return get_boolean_param (self, "block-cursor");
}
gboolean
ide_source_view_mode_get_keep_mark_on_char (IdeSourceViewMode *self)
{
  /* forces the source view to not let the cursor reach the end of the
   * line (basically an iter over \n). this is useful for emulating vim
   */
  return get_boolean_param (self, "keep-mark-on-char");
}

static void
ide_source_view_mode_destroy (GtkWidget *widget)
{
  IdeSourceViewMode *self = IDE_SOURCE_VIEW_MODE (widget);

  g_clear_object (&self->view);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->default_mode, g_free);
  g_clear_pointer (&self->display_name, g_free);
  self->type = 0;

  GTK_WIDGET_CLASS (ide_source_view_mode_parent_class)->destroy (widget);
}

static void
proxy_closure_marshal (GClosure     *closure,
                       GValue       *return_value,
                       guint         n_param_values,
                       const GValue *param_values,
                       gpointer      invocation_hint,
                       gpointer      marshal_data)
{
  g_autoptr(IdeSourceViewMode) self = NULL;
  GValue *param_copy;

  g_assert (closure != NULL);
  g_assert (param_values != NULL);

  /*
   * To be absolutely sure about reference counting with GValue and other
   * ownership mishaps, this proxy makes a full copy of all parameters. It is
   * certainly slower, but we are only activated from keybindings and the user
   * can only type so fast...
   */

  self = g_value_dup_object (&param_values[0]);
  g_assert (IDE_IS_SOURCE_VIEW_MODE (self));

  if (self->view == NULL)
    {
      g_warning ("Cannot proxy signal after destroy has been called");
      return;
    }

  /* Just spill to the stack, we only have a small number of params */
  param_copy = g_alloca (sizeof (GValue) * n_param_values);
  memset (param_copy, 0, sizeof (GValue) * n_param_values);

  /* Swap the first object into the IdeSourceView */
  g_value_init (&param_copy[0], G_OBJECT_TYPE (self->view));
  g_value_set_object (&param_copy[0], self->view);

  /* Copy the rest of the parameters across */
  for (guint i = 1; i < n_param_values; i++)
    {
      g_value_init (&param_copy[i], G_VALUE_TYPE (&param_values[i]));
      g_value_copy (&param_values[i], &param_copy[i]);
    }

  /* Emit the signal on the source view */
  g_signal_emitv (param_copy, GPOINTER_TO_UINT (closure->data), 0, return_value);

  /* Cleanup, dropping our references. */
  for (guint i = 0; i < n_param_values; i++)
    g_value_unset (&param_copy[i]);
}

static void
proxy_all_action_signals (GType type)
{
  g_autofree guint *signals = NULL;
  GSignalQuery query;
  guint n_signals = 0;

  g_assert (g_type_is_a (type, G_TYPE_OBJECT));

  signals = g_signal_list_ids (type, &n_signals);

  for (guint i = 0; i < n_signals; i++)
    {
      g_signal_query (signals[i], &query);

      /* We don't support detailed signals */
      if (query.signal_flags & G_SIGNAL_DETAILED)
        continue;

      /* Only proxy keybinding action signals */
      if (query.signal_flags & G_SIGNAL_ACTION)
        {
          GClosure *proxy;

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
          /* Proxy ownership is stolen when sinking the closure */
        }
    }
}

const gchar *
ide_source_view_mode_get_name (IdeSourceViewMode *mode)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW_MODE (mode), NULL);

  return mode->name;
}

static void
ide_source_view_mode_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  IdeSourceViewMode *mode = IDE_SOURCE_VIEW_MODE(object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ide_source_view_mode_get_name (mode));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_source_view_mode_class_init (IdeSourceViewModeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set, *parent_binding_set;
  GType type;

  object_class->get_property = ide_source_view_mode_get_property;

  widget_class->destroy = ide_source_view_mode_destroy;

  gtk_widget_class_set_css_name (widget_class, "idesourceviewmode");

  properties [PROP_NAME] =
    g_param_spec_string ("name",
                          "Name",
                          "The name of the mode.",
                          NULL,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_install_style_property (GTK_WIDGET_CLASS (klass),
                                           g_param_spec_boolean ("suppress-unbound",
                                                                 "Supress Unbound",
                                                                 "Suppress Unbound Keypresses",
                                                                 FALSE,
                                                                 (G_PARAM_READABLE |
                                                                  G_PARAM_STATIC_STRINGS)));

  gtk_widget_class_install_style_property (GTK_WIDGET_CLASS (klass),
                                           g_param_spec_boolean ("block-cursor",
                                                                 "Block Cursor",
                                                                 "Use fake block cursor by "
                                                                  "using overwrite mode.",
                                                                 FALSE,
                                                                 (G_PARAM_READABLE |
                                                                  G_PARAM_STATIC_STRINGS)));

  gtk_widget_class_install_style_property (GTK_WIDGET_CLASS (klass),
                                           g_param_spec_boolean ("keep-mark-on-char",
                                                                 "Keep Mark on Char",
                                                                 "Don't allow the cursor to "
                                                                  "move to line end.",
                                                                 FALSE,
                                                                 (G_PARAM_READABLE |
                                                                  G_PARAM_STATIC_STRINGS)));

  gtk_widget_class_install_style_property (GTK_WIDGET_CLASS (klass),
                                           g_param_spec_string ("display-name",
                                                                "Display Name",
                                                                "Display name for mode",
                                                                NULL,
                                                                (G_PARAM_READABLE |
                                                                 G_PARAM_STATIC_STRINGS)));

  gtk_widget_class_install_style_property (GTK_WIDGET_CLASS (klass),
                                           g_param_spec_string ("default-mode",
                                                                "Default Mode",
                                                                "Suggest a followup default mode",
                                                                NULL,
                                                                (G_PARAM_READABLE |
                                                                 G_PARAM_STATIC_STRINGS)));

  gtk_widget_class_install_style_property (GTK_WIDGET_CLASS (klass),
                                           g_param_spec_boolean ("repeat-insert-with-count",
                                                                 "Repeat Insert with Count",
                                                                 "Use the current count to "
                                                                  "repeat the insertion.",
                                                                 FALSE,
                                                                 (G_PARAM_READABLE |
                                                                  G_PARAM_STATIC_STRINGS)));

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

static gboolean
toplevel_is_offscreen (GdkWindow *window)
{
  /*
   * FIXME: This function is a workaround for a segfault in gdk_window_beep()
   *        with offscreen windows.
   *
   *        https://bugzilla.gnome.org/show_bug.cgi?id=748341
   */
  for (;
       window != NULL;
       window = gdk_window_get_parent (window))
    {
      GdkWindowType type;

      type = gdk_window_get_window_type (window);

      if (type == GDK_WINDOW_OFFSCREEN)
        return TRUE;
    }

  return FALSE;
}

static gboolean
can_suppress (const GdkEventKey *event)
{
  /*
   * This is rather tricky because we don't know what can be activated
   * in the bubble up phase of event delivery. Looking at ->string isn't
   * very safe when input methods are in play. So we just hard code some
   * things we know about common keybindings.
   *
   * If you are wondering why you're getting beeps in the editor while
   * activating some keybinding you've added, you found the right spot!
   */
  if ((event->state & GDK_MODIFIER_MASK) != 0)
    return FALSE;

  switch (event->keyval)
    {
    case GDK_KEY_F1: case GDK_KEY_F2: case GDK_KEY_F3: case GDK_KEY_F4:
    case GDK_KEY_F5: case GDK_KEY_F6: case GDK_KEY_F7: case GDK_KEY_F8:
    case GDK_KEY_F9: case GDK_KEY_F10: case GDK_KEY_F11: case GDK_KEY_F12:
      return FALSE;

    default:
      return TRUE;
    }
}

gboolean
_ide_source_view_mode_do_event (IdeSourceViewMode *mode,
                                GdkEventKey       *event,
                                gboolean          *remove)
{
  GtkStyleContext *context;
  gboolean suppress_unbound;
  gboolean handled;

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW_MODE (mode), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (remove, FALSE);

  context = gtk_widget_get_style_context (GTK_WIDGET (mode));

  suppress_unbound = ide_source_view_mode_get_suppress_unbound (mode);

  g_object_ref (context);
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, mode->name);
  handled = gtk_bindings_activate_event (G_OBJECT (mode), event);
  gtk_style_context_restore (context);
  g_object_unref (context);

  *remove = FALSE;
  switch (mode->type)
    {
    case IDE_SOURCE_VIEW_MODE_TYPE_TRANSIENT:
      if (handled)
        {
          *remove = TRUE;
        }
      else
        {
          if (!is_modifier_key (event))
            {
              if (!toplevel_is_offscreen (event->window))
                gtk_widget_error_bell (mode->view);
              handled = TRUE;
              *remove = TRUE;
            }
        }
      break;

    case IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT:
      {
        /* Don't block possible accelerators, but suppress others. */
        if (!handled && suppress_unbound && can_suppress (event))
          {
            if (!is_modifier_key (event) && !toplevel_is_offscreen (event->window))
              gdk_window_beep (event->window);

            /* cancel any inflight macros */
            g_signal_emit_by_name (mode->view, "end-macro");

            handled = TRUE;
          }
      }
      break;

    case IDE_SOURCE_VIEW_MODE_TYPE_MODAL:
      handled = TRUE;
      break;

    default:
      g_assert_not_reached ();
    }

  return handled;
}

IdeSourceViewMode *
_ide_source_view_mode_new (GtkWidget             *view,
                           const char            *name,
                           IdeSourceViewModeType  type)
{
  IdeSourceViewMode *mode;

  mode = g_object_new (IDE_TYPE_SOURCE_VIEW_MODE, NULL);

  mode->view = g_object_ref (view);
  mode->name = g_strdup (name);
  mode->type = type;

  if (mode->name != NULL)
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (GTK_WIDGET (mode));
      gtk_style_context_add_class (context, mode->name);
    }

  mode->default_mode = get_string_param (mode, "default-mode");
  mode->display_name = get_string_param (mode, "display-name");

  IDE_TRACE_MSG ("suppress_unbound = %d", ide_source_view_mode_get_suppress_unbound (mode));
  IDE_TRACE_MSG ("block_cursor = %d", ide_source_view_mode_get_block_cursor (mode));
  IDE_TRACE_MSG ("type = %d", (int)mode->type);
  IDE_TRACE_MSG ("default_mode = %s", mode->default_mode ?: "(null)");
  IDE_TRACE_MSG ("display_name = %s", mode->display_name ?: "(null)");

  return g_object_ref_sink (mode);
}

IdeSourceViewModeType
ide_source_view_mode_get_mode_type (IdeSourceViewMode *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW_MODE (self), 0);
  return self->type;
}

void
ide_source_view_mode_set_has_indenter (IdeSourceViewMode *self,
                                       gboolean           has_indenter)
{
  GtkStyleContext *style_context;

  g_assert (IDE_IS_SOURCE_VIEW_MODE (self));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (has_indenter)
    gtk_style_context_add_class (style_context, "has-indenter");
  else
    gtk_style_context_remove_class (style_context, "has-indenter");
}

void
ide_source_view_mode_set_has_selection (IdeSourceViewMode *self,
                                        gboolean           has_selection)
{
  GtkStyleContext *style_context;

  g_assert (IDE_IS_SOURCE_VIEW_MODE (self));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));

  if (has_selection)
    gtk_style_context_add_class (style_context, "has-selection");
  else
    gtk_style_context_remove_class (style_context, "has-selection");
}
