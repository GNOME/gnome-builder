/* ide-tweaks-font.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tweaks-font"

#include "config.h"

#include <glib/gi18n.h>

#include <adwaita.h>

#include "ide-tweaks.h"
#include "ide-tweaks-font.h"

struct _IdeTweaksFont
{
  IdeTweaksWidget parent_instance;
  IdeTweaksBinding *enabled_binding;
  char *title;
  char *subtitle;
};

enum {
  PROP_0,
  PROP_ENABLED_BINDING,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksFont, ide_tweaks_font, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_font_dialog_response_cb (GtkFontChooserDialog *dialog,
                                    int                   response_id,
                                    IdeTweaksBinding     *binding)
{
  g_autofree char *font_name = NULL;

  g_assert (GTK_IS_FONT_CHOOSER_DIALOG (dialog));
  g_assert (IDE_IS_TWEAKS_BINDING (binding));

  font_name = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));

  if (response_id == GTK_RESPONSE_OK)
    ide_tweaks_binding_set_string (binding, font_name);

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
ide_tweaks_font_button_clicked_cb (GtkWidget        *widget,
                                   IdeTweaksBinding *binding)
{
  g_autofree char *font_name = NULL;
  GtkWidget *dialog;
  GtkRoot *root;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_BINDING (binding));

  font_name = ide_tweaks_binding_dup_string (binding);
  root = gtk_widget_get_root (widget);
  dialog = gtk_font_chooser_dialog_new (_("Select Font"), GTK_WINDOW (root));

  gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), font_name);

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (ide_tweaks_font_dialog_response_cb),
                           binding,
                           0);

  gtk_window_present (GTK_WINDOW (dialog));
}

static GtkWidget *
ide_tweaks_font_create_for_item (IdeTweaksWidget *instance,
                                 IdeTweaksItem   *widget)
{
  IdeTweaksFont *self = (IdeTweaksFont *)widget;
  IdeTweaksBinding *binding;
  GtkWidget *row;
  GtkWidget *arrow;
  GtkWidget *expander;
  GtkWidget *toggle;

  g_assert (IDE_IS_TWEAKS_FONT (self));

  if (!(binding = ide_tweaks_widget_get_binding (IDE_TWEAKS_WIDGET (self))))
    return NULL;

  if (self->enabled_binding == NULL)
    {
      GtkButton *button;

      button = g_object_new (GTK_TYPE_BUTTON,
                             "css-classes", IDE_STRV_INIT ("flat"),
                             "valign", GTK_ALIGN_CENTER,
                             "can-shrink", TRUE,
                             NULL);
      row = g_object_new (ADW_TYPE_ACTION_ROW,
                          "title", self->title,
                          "subtitle", self->subtitle,
                          "activatable-widget", button,
                          NULL);
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), GTK_WIDGET (button));

      g_signal_connect_object (button,
                               "clicked",
                               G_CALLBACK (ide_tweaks_font_button_clicked_cb),
                               binding,
                               0);

      ide_tweaks_binding_bind (binding, button, "label");

      return row;
    }

  expander = g_object_new (ADW_TYPE_EXPANDER_ROW,
                           "expanded", FALSE,
                           "title", self->title,
                           "subtitle", self->subtitle,
                           NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      NULL);
  ide_tweaks_binding_bind (binding, row, "title");

  toggle = g_object_new (GTK_TYPE_SWITCH,
                         "valign", GTK_ALIGN_CENTER,
                         NULL);
  g_object_bind_property (expander, "expanded",
                          toggle, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  adw_expander_row_add_suffix (ADW_EXPANDER_ROW (expander), toggle);

  ide_tweaks_binding_bind (self->enabled_binding, expander, "expanded");

  arrow = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "pan-end-symbolic",
                        NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), GTK_WIDGET (arrow));
  adw_expander_row_add_row (ADW_EXPANDER_ROW (expander), row);

  g_signal_connect_object (row,
                           "activated",
                           G_CALLBACK (ide_tweaks_font_button_clicked_cb),
                           binding,
                           0);

  return GTK_WIDGET (expander);
}

static void
ide_tweaks_font_dispose (GObject *object)
{
  IdeTweaksFont *self = (IdeTweaksFont *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_clear_object (&self->enabled_binding);

  G_OBJECT_CLASS (ide_tweaks_font_parent_class)->dispose (object);
}

static void
ide_tweaks_font_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeTweaksFont *self = IDE_TWEAKS_FONT (object);

  switch (prop_id)
    {
    case PROP_ENABLED_BINDING:
      g_value_set_object (value, self->enabled_binding);
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_tweaks_font_get_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_font_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_font_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeTweaksFont *self = IDE_TWEAKS_FONT (object);

  switch (prop_id)
    {
    case PROP_ENABLED_BINDING:
      g_set_object (&self->enabled_binding, g_value_get_object (value));
      break;

    case PROP_SUBTITLE:
      ide_tweaks_font_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_tweaks_font_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_font_class_init (IdeTweaksFontClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_font_dispose;
  object_class->get_property = ide_tweaks_font_get_property;
  object_class->set_property = ide_tweaks_font_set_property;

  widget_class->create_for_item = ide_tweaks_font_create_for_item;

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ENABLED_BINDING] =
    g_param_spec_object ("enabled-binding", NULL, NULL,
                         IDE_TYPE_TWEAKS_BINDING,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_font_init (IdeTweaksFont *self)
{
}

const char *
ide_tweaks_font_get_subtitle (IdeTweaksFont *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_FONT (self), NULL);

  return self->subtitle;
}

const char *
ide_tweaks_font_get_title (IdeTweaksFont *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_FONT (self), NULL);

  return self->title;
}

void
ide_tweaks_font_set_subtitle (IdeTweaksFont *self,
                              const char    *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_FONT (self));

  if (g_set_str (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

void
ide_tweaks_font_set_title (IdeTweaksFont *self,
                           const char    *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_FONT (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
