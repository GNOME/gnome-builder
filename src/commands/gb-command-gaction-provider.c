/* gb-command-gaction-provider.c
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

#define G_LOG_DOMAIN "gaction-commands"

#include <glib/gi18n.h>

#include "gb-command-gaction-provider.h"
#include "gb-tab.h"

struct _GbCommandGactionProviderPrivate
{
  GbWorkbench *workbench;
  GtkWidget   *focus;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandGactionProvider,
                            gb_command_gaction_provider,
                            GB_TYPE_COMMAND_PROVIDER)

enum {
  PROP_0,
  PROP_WORKBENCH,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbCommandProvider *
gb_command_gaction_provider_new (GbWorkbench *workbench)
{
  return g_object_new (GB_TYPE_COMMAND_GACTION_PROVIDER,
                       "workbench", workbench,
                       NULL);
}

GbWorkbench *
gb_command_gaction_provider_get_workbench (GbCommandGactionProvider *provider)
{
  g_return_val_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider), NULL);

  return provider->priv->workbench;
}

static void
gb_command_gaction_provider_update_focus (GbCommandGactionProvider *provider,
                                          GtkWidget                *focus)
{
  GbCommandGactionProviderPrivate *priv;

  g_return_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider));
  g_return_if_fail (!focus || GTK_IS_WIDGET (focus));

  priv = provider->priv;

  if (priv->focus)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->focus),
                                    (gpointer *)&priv->focus);
      priv->focus = NULL;
    }

  if (focus)
    {
      priv->focus = focus;
      g_object_add_weak_pointer (G_OBJECT (priv->focus),
                                 (gpointer *)&priv->focus);
    }
}

static void
on_workbench_set_focus (GbCommandGactionProvider *provider,
                        GtkWidget                *widget,
                        GbWorkbench              *workbench)
{
  GtkWidget *parent = widget;

  g_return_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  /*
   * Try to locate the nearest GbTab in the widget hierarchy starting from
   * the new focus widget. If this widget is not a decendent of a tab, we
   * will just ignore things.
   */
  while (!GB_IS_TAB (parent))
    {
      parent = gtk_widget_get_parent (parent);
      if (!parent)
        break;
    }

  if (GB_IS_TAB (parent))
    gb_command_gaction_provider_update_focus (provider, parent);
}

static void
gb_command_gaction_provider_connect (GbCommandGactionProvider *provider,
                                     GbWorkbench              *workbench)
{
  g_return_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  g_signal_connect_object (workbench,
                           "set-focus",
                           G_CALLBACK (on_workbench_set_focus),
                           provider,
                           G_CONNECT_SWAPPED);
}

static void
gb_command_gaction_provider_disconnect (GbCommandGactionProvider *provider,
                                        GbWorkbench              *workbench)
{
  g_return_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  g_signal_handlers_disconnect_by_func (workbench,
                                        G_CALLBACK (on_workbench_set_focus),
                                        provider);
}

void
gb_command_gaction_provider_set_workbench (GbCommandGactionProvider *provider,
                                           GbWorkbench              *workbench)
{
  GbCommandGactionProviderPrivate *priv;

  g_return_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (provider));
  g_return_if_fail (!workbench || GB_IS_WORKBENCH (workbench));

  priv = provider->priv;

  if (workbench != priv->workbench)
    {
      if (priv->workbench)
        {
          gb_command_gaction_provider_disconnect (provider, workbench);
          g_object_remove_weak_pointer (G_OBJECT (priv->workbench),
                                        (gpointer *)&priv->workbench);
          priv->workbench = NULL;
        }

      if (workbench)
        {
          gb_command_gaction_provider_connect (provider, workbench);
          priv->workbench = workbench;
          g_object_add_weak_pointer (G_OBJECT (workbench),
                                     (gpointer *)&priv->workbench);
        }

      g_object_notify_by_pspec (G_OBJECT (provider),
                                gParamSpecs [PROP_WORKBENCH]);
    }
}

static GAction *
gb_command_gaction_provider_lookup (GbCommandProvider  *provider,
                                    const gchar        *command_text,
                                    GVariant          **parameters)
{
  GbCommandGactionProvider *self = (GbCommandGactionProvider *)provider;
  GtkWidget *widget;
  GAction *action = NULL;
  gchar **parts;
  gchar *tmp;
  gchar *name = NULL;

  g_return_val_if_fail (GB_IS_COMMAND_GACTION_PROVIDER (self), NULL);

  if (!self->priv->focus)
    return NULL;

  widget = self->priv->focus;

  /* Determine the command name */
  tmp = g_strdelimit (g_strdup (command_text), "(", ' ');
  parts = g_strsplit (tmp, " ", 2);
  name = g_strdup (parts [0]);
  g_free (tmp);
  g_strfreev (parts);

  /* Parse the parameters if provided */
  command_text += strlen (name);
  for (; *command_text; command_text = g_utf8_next_char (command_text))
    {
      gunichar ch;

      ch = g_utf8_get_char (command_text);
      if (g_unichar_isspace (ch))
        continue;
      break;
    }

  if (*command_text)
    {
      *parameters = g_variant_parse (NULL, command_text, NULL, NULL, NULL);
      if (!*parameters)
        goto cleanup;
    }

  /*
   * TODO: We are missing some API in Gtk+ to be able to resolve an action
   *       for a particular name. It exists, but is currently only private
   *       API. So we'll hold off a bit on this until we can use that.
   *       Alternatively, we can just walk up the chain to the
   *       ApplicationWindow which is a GActionMap.
   */

  while (widget)
    {
      if (G_IS_ACTION_MAP (widget))
        {
          action = g_action_map_lookup_action (G_ACTION_MAP (widget), name);
          if (action)
            break;
        }

      widget = gtk_widget_get_parent (widget);
    }

  if (!action && *parameters)
    {
      g_variant_unref (*parameters);
      *parameters = NULL;
    }

cleanup:
  g_free (name);

  return action;
}

static void
gb_command_gaction_provider_dispose (GObject *object)
{
  GbCommandGactionProvider *provider = (GbCommandGactionProvider *)object;

  gb_command_gaction_provider_update_focus (provider, NULL);
  gb_command_gaction_provider_set_workbench (provider, NULL);

  G_OBJECT_CLASS (gb_command_gaction_provider_parent_class)->dispose (object);
}

static void
gb_command_gaction_provider_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GbCommandGactionProvider *self = GB_COMMAND_GACTION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_WORKBENCH:
      g_value_set_object (value,
                          gb_command_gaction_provider_get_workbench (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_gaction_provider_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GbCommandGactionProvider *self = GB_COMMAND_GACTION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_WORKBENCH:
      gb_command_gaction_provider_set_workbench (self,
                                                 g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_gaction_provider_class_init (GbCommandGactionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbCommandProviderClass *provider_class = GB_COMMAND_PROVIDER_CLASS (klass);

  object_class->dispose = gb_command_gaction_provider_dispose;
  object_class->get_property = gb_command_gaction_provider_get_property;
  object_class->set_property = gb_command_gaction_provider_set_property;

  provider_class->lookup = gb_command_gaction_provider_lookup;

  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("The workbench containing the actions."),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_WORKBENCH,
                                   gParamSpecs [PROP_WORKBENCH]);
}

static void
gb_command_gaction_provider_init (GbCommandGactionProvider *self)
{
  self->priv = gb_command_gaction_provider_get_instance_private (self);
}
