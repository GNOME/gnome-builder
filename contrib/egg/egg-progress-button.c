/*
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define G_LOG_DOMAIN "egg-progress-button"

#include "egg-progress-button.h"

typedef struct
{
  GtkButton       parent_instance;
  guint           progress;
  guint           show_progress : 1;
  GtkCssProvider *css_provider;
} EggProgressButtonPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EggProgressButton, egg_progress_button, GTK_TYPE_BUTTON)

enum {
  PROP_0,
  PROP_PROGRESS,
  PROP_SHOW_PROGRESS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

guint
egg_progress_button_get_progress (EggProgressButton *self)
{
  EggProgressButtonPrivate *priv = egg_progress_button_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_PROGRESS_BUTTON (self), 0);

  return priv->progress;
}

gboolean
egg_progress_button_get_show_progress (EggProgressButton *self)
{
  EggProgressButtonPrivate *priv = egg_progress_button_get_instance_private (self);

  g_return_val_if_fail (EGG_IS_PROGRESS_BUTTON (self), FALSE);

  return priv->show_progress;
}

void
egg_progress_button_set_progress (EggProgressButton *button,
                                  guint              percentage)
{
  EggProgressButtonPrivate *priv = egg_progress_button_get_instance_private (button);
  g_autofree gchar *css = NULL;

  g_return_if_fail (EGG_IS_PROGRESS_BUTTON (button));

  priv->progress = percentage = MIN (percentage, 100);

  if (percentage == 0)
    css = g_strdup (".install-progress { background-size: 0; }");
  else if (percentage == 100)
    css = g_strdup (".install-progress { background-size: 100%; }");
  else
    css = g_strdup_printf (".install-progress { background-size: %u%%; }", percentage);

  gtk_css_provider_load_from_data (priv->css_provider, css, -1, NULL);
}

void
egg_progress_button_set_show_progress (EggProgressButton *button,
                                       gboolean           show_progress)
{
  EggProgressButtonPrivate *priv = egg_progress_button_get_instance_private (button);
  GtkStyleContext *context;

  g_return_if_fail (EGG_IS_PROGRESS_BUTTON (button));

  priv->show_progress = !!show_progress;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));

  if (show_progress)
    gtk_style_context_add_class (context, "install-progress");
  else
    gtk_style_context_remove_class (context, "install-progress");
}

static void
egg_progress_button_dispose (GObject *object)
{
  EggProgressButton *button = EGG_PROGRESS_BUTTON (object);
  EggProgressButtonPrivate *priv = egg_progress_button_get_instance_private (button);

  g_clear_object (&priv->css_provider);

  G_OBJECT_CLASS (egg_progress_button_parent_class)->dispose (object);
}

static void
egg_progress_button_init (EggProgressButton *button)
{
  EggProgressButtonPrivate *priv = egg_progress_button_get_instance_private (button);

  priv->css_provider = gtk_css_provider_new ();

  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (button)),
                                  GTK_STYLE_PROVIDER (priv->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
egg_progress_button_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EggProgressButton *self = EGG_PROGRESS_BUTTON (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      g_value_set_uint (value, egg_progress_button_get_progress (self));
      break;

    case PROP_SHOW_PROGRESS:
      g_value_set_boolean (value, egg_progress_button_get_show_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_progress_button_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EggProgressButton *self = EGG_PROGRESS_BUTTON (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      egg_progress_button_set_progress (self, g_value_get_uint (value));
      break;

    case PROP_SHOW_PROGRESS:
      egg_progress_button_set_show_progress (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_progress_button_class_init (EggProgressButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = egg_progress_button_dispose;
  object_class->get_property = egg_progress_button_get_property;
  object_class->set_property = egg_progress_button_set_property;

  properties [PROP_PROGRESS] =
    g_param_spec_uint ("progress", NULL, NULL, 0, 100, 0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_PROGRESS] =
    g_param_spec_boolean ("show-progress", NULL,  NULL, FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

GtkWidget *
egg_progress_button_new (void)
{
  return g_object_new (EGG_TYPE_PROGRESS_BUTTON, NULL);
}
