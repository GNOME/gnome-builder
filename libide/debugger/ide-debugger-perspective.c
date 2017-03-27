/* ide-debugger-perspective.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-perspective"

#include <egg-signal-group.h>
#include <glib/gi18n.h>

#include "debugger/ide-debugger.h"
#include "debugger/ide-debugger-perspective.h"
#include "util/ide-pango.h"

struct _IdeDebuggerPerspective
{
  IdeLayout       parent_instance;

  IdeDebugger    *debugger;
  EggSignalGroup *debugger_signals;
  GSettings      *terminal_settings;
  GtkCssProvider *log_css;

  GtkTextBuffer  *log_buffer;
  GtkTextView    *log_text_view;
};

enum {
  PROP_0,
  PROP_DEBUGGER,
  N_PROPS
};

static gchar *
ide_debugger_perspective_get_title (IdePerspective *perspective)
{
  return g_strdup (_("Debugger"));
}

static gchar *
ide_debugger_perspective_get_id (IdePerspective *perspective)
{
  return g_strdup ("debugger");
}

static gchar *
ide_debugger_perspective_get_icon_name (IdePerspective *perspective)
{
  return g_strdup ("builder-debugger-symbolic");
}

static gchar *
ide_debugger_perspective_get_accelerator (IdePerspective *perspective)
{
  return g_strdup ("<Alt>2");
}

static void
perspective_iface_init (IdePerspectiveInterface *iface)
{
  iface->get_accelerator = ide_debugger_perspective_get_accelerator;
  iface->get_icon_name = ide_debugger_perspective_get_icon_name;
  iface->get_id = ide_debugger_perspective_get_id;
  iface->get_title = ide_debugger_perspective_get_title;
}

G_DEFINE_TYPE_EXTENDED (IdeDebuggerPerspective, ide_debugger_perspective, IDE_TYPE_LAYOUT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PERSPECTIVE, perspective_iface_init))

static GParamSpec *properties [N_PROPS];

static void
on_debugger_log (IdeDebuggerPerspective *self,
                 const gchar            *message,
                 IdeDebugger            *debugger)
{
  GtkTextIter iter;

  g_assert (IDE_IS_DEBUGGER_PERSPECTIVE (self));
  g_assert (IDE_IS_DEBUGGER (debugger));

  gtk_text_buffer_get_end_iter (self->log_buffer, &iter);
  gtk_text_buffer_insert (self->log_buffer, &iter, message, -1);
  gtk_text_buffer_select_range (self->log_buffer, &iter, &iter);
  gtk_text_view_scroll_to_iter (self->log_text_view, &iter, 0.0, FALSE, 1.0, 1.0);
}

static void
ide_debugger_perspective_set_debugger (IdeDebuggerPerspective *self,
                                       IdeDebugger            *debugger)
{
  g_return_if_fail (IDE_IS_DEBUGGER_PERSPECTIVE (self));
  g_return_if_fail (!debugger || IDE_IS_DEBUGGER (debugger));

  if (g_set_object (&self->debugger, debugger))
    {
      egg_signal_group_set_target (self->debugger_signals, debugger);
      gtk_text_buffer_set_text (self->log_buffer, "", 0);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEBUGGER]);
    }
}

static void
log_panel_changed_font_name (IdeDebuggerPerspective *self,
                             const gchar            *key,
                             GSettings              *settings)
{
  gchar *font_name;
  PangoFontDescription *font_desc;

  g_assert (IDE_IS_DEBUGGER_PERSPECTIVE (self));
  g_assert (g_strcmp0 (key, "font-name") == 0);
  g_assert (G_IS_SETTINGS (settings));

  font_name = g_settings_get_string (settings, key);
  font_desc = pango_font_description_from_string (font_name);

  if (font_desc != NULL)
    {
      gchar *fragment;
      gchar *css;

      fragment = ide_pango_font_description_to_css (font_desc);
      css = g_strdup_printf ("textview { %s }", fragment);

      gtk_css_provider_load_from_data (self->log_css, css, -1, NULL);

      pango_font_description_free (font_desc);
      g_free (fragment);
      g_free (css);
    }

  g_free (font_name);
}

static void
ide_debugger_perspective_finalize (GObject *object)
{
  IdeDebuggerPerspective *self = (IdeDebuggerPerspective *)object;

  g_clear_object (&self->debugger);
  g_clear_object (&self->debugger_signals);
  g_clear_object (&self->terminal_settings);
  g_clear_object (&self->log_css);

  G_OBJECT_CLASS (ide_debugger_perspective_parent_class)->finalize (object);
}

static void
ide_debugger_perspective_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeDebuggerPerspective *self = IDE_DEBUGGER_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      g_value_set_object (value, self->debugger);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_perspective_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeDebuggerPerspective *self = IDE_DEBUGGER_PERSPECTIVE (object);

  switch (prop_id)
    {
    case PROP_DEBUGGER:
      ide_debugger_perspective_set_debugger (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_perspective_class_init (IdeDebuggerPerspectiveClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_debugger_perspective_finalize;
  object_class->get_property = ide_debugger_perspective_get_property;
  object_class->set_property = ide_debugger_perspective_set_property;

  properties [PROP_DEBUGGER] =
    g_param_spec_object ("debugger",
                         "Debugger",
                         "The current debugger instance",
                         IDE_TYPE_DEBUGGER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-debugger-perspective.ui");

  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerPerspective, log_text_view);
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerPerspective, log_buffer);
}

static void
ide_debugger_perspective_init (IdeDebuggerPerspective *self)
{
  GtkStyleContext *context;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->debugger_signals = egg_signal_group_new (IDE_TYPE_DEBUGGER);

  egg_signal_group_connect_object (self->debugger_signals,
                                   "log",
                                   G_CALLBACK (on_debugger_log),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->log_css = gtk_css_provider_new ();
  context = gtk_widget_get_style_context (GTK_WIDGET (self->log_text_view));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (self->log_css),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  self->terminal_settings = g_settings_new ("org.gnome.builder.terminal");
  g_signal_connect_object (self->terminal_settings,
                           "changed::font-name",
                           G_CALLBACK (log_panel_changed_font_name),
                           self,
                           G_CONNECT_SWAPPED);
  log_panel_changed_font_name (self, "font-name", self->terminal_settings);
}
