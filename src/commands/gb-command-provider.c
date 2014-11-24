/* gb-command-provider.c
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

#include <glib/gi18n.h>

#include "gb-command-provider.h"

struct _GbCommandProviderPrivate
{
  GbWorkbench *workbench;
  GbTab       *active_tab;
  gint         priority;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandProvider, gb_command_provider,
                            G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ACTIVE_TAB,
  PROP_PRIORITY,
  PROP_WORKBENCH,
  LAST_PROP
};

enum {
  LOOKUP,
  COMPLETE,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

GbCommandProvider *
gb_command_provider_new (GbWorkbench *workbench)
{
  return g_object_new (GB_TYPE_COMMAND_PROVIDER,
                       "workbench", workbench,
                       NULL);
}

/**
 * gb_command_provider_get_active_tab:
 *
 * Returns the "active-tab" property. The active-tab is the last tab that
 * was focused in the workbench.
 *
 * Returns: (transfer none): A #GbTab or %NULL.
 */
GbTab *
gb_command_provider_get_active_tab (GbCommandProvider *provider)
{
  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), NULL);

  return provider->priv->active_tab;
}

static void
gb_command_provider_set_active_tab (GbCommandProvider *provider,
                                    GbTab             *tab)
{
  GbCommandProviderPrivate *priv;

  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (!tab || GB_IS_TAB (tab));

  priv = provider->priv;

  if (priv->active_tab)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->active_tab),
                                    (gpointer *)&priv->active_tab);
      priv->active_tab = NULL;
    }

  if (tab)
    {
      priv->active_tab = tab;
      g_object_add_weak_pointer (G_OBJECT (priv->active_tab),
                                 (gpointer *)&priv->active_tab);
    }

  g_object_notify_by_pspec (G_OBJECT (provider),
                            gParamSpecs [PROP_ACTIVE_TAB]);
}

static void
on_workbench_set_focus (GbCommandProvider *provider,
                        GtkWidget         *widget,
                        GbWorkbench       *workbench)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  /* walk the hierarchy to find a tab */
  if (widget)
    while (!GB_IS_TAB (widget))
      if (!(widget = gtk_widget_get_parent (widget)))
        break;

  if (GB_IS_TAB (widget))
    gb_command_provider_set_active_tab (provider, GB_TAB (widget));
}

static void
gb_command_provider_connect (GbCommandProvider *provider,
                             GbWorkbench       *workbench)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  g_signal_connect_object (workbench,
                           "set-focus",
                           G_CALLBACK (on_workbench_set_focus),
                           provider,
                           G_CONNECT_SWAPPED);
}

static void
gb_command_provider_disconnect (GbCommandProvider *provider,
                                GbWorkbench       *workbench)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (GB_IS_WORKBENCH (workbench));

  g_signal_handlers_disconnect_by_func (workbench,
                                        G_CALLBACK (on_workbench_set_focus),
                                        provider);
}

GbWorkbench *
gb_command_provider_get_workbench (GbCommandProvider *provider)
{
  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), NULL);

  return provider->priv->workbench;
}

static void
gb_command_provider_set_workbench (GbCommandProvider *provider,
                                   GbWorkbench       *workbench)
{
  GbCommandProviderPrivate *priv;

  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (!workbench || GB_IS_WORKBENCH (workbench));

  priv = provider->priv;

  if (priv->workbench != workbench)
    {
      if (priv->workbench)
        {
          gb_command_provider_disconnect (provider, workbench);
          g_object_remove_weak_pointer (G_OBJECT (priv->workbench),
                                        (gpointer *)&priv->workbench);
          priv->workbench = NULL;
        }
    
      if (workbench)
        {
          priv->workbench = workbench;
          g_object_add_weak_pointer (G_OBJECT (priv->workbench),
                                     (gpointer *)&priv->workbench);
          gb_command_provider_connect (provider, workbench);
        }
    
      g_object_notify_by_pspec (G_OBJECT (provider),
                                gParamSpecs [PROP_WORKBENCH]);
  }
}

gint
gb_command_provider_get_priority (GbCommandProvider *provider)
{
  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), 0);

  return provider->priv->priority;
}

void
gb_command_provider_set_priority (GbCommandProvider *provider,
                                  gint               priority)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));

  if (provider->priv->priority != priority)
    {
      provider->priv->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (provider),
                                gParamSpecs [PROP_PRIORITY]);
    }
}

/**
 * gb_command_provider_lookup:
 * @provider: (in): The #GbCommandProvider
 * @command_text: (in): Command text to be parsed
 *
 *
 * Returns: (transfer full): A #GbCommand if successful; otherwise %NULL.
 */
GbCommand *
gb_command_provider_lookup (GbCommandProvider *provider,
                            const gchar       *command_text)
{
  GbCommand *ret = NULL;

  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), NULL);
  g_return_val_if_fail (command_text, NULL);

  g_signal_emit (provider, gSignals [LOOKUP], 0, command_text, &ret);

  return ret;
}

/**
 * gb_command_provider_complete:
 * @provider: (in): The #GbCommandProvider
 * @completions: (in): A #GPtrArray where completed strings can be added
 * @command_text: (in): Initial command text to be completed
 *
 */
void
gb_command_provider_complete (GbCommandProvider *provider,
                              GPtrArray         *completions,
                              const gchar       *initial_command_text)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (completions);
  g_return_if_fail (initial_command_text);

  g_signal_emit (provider, gSignals [COMPLETE], 0, completions, initial_command_text);
}


static void
gb_command_provider_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbCommandProvider *self = GB_COMMAND_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_TAB:
      g_value_set_object (value, gb_command_provider_get_active_tab (self));
      break;

    case PROP_PRIORITY:
      g_value_set_int (value, gb_command_provider_get_priority (self));
      break;

    case PROP_WORKBENCH:
      g_value_set_object (value, gb_command_provider_get_workbench (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_provider_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbCommandProvider *self = GB_COMMAND_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      gb_command_provider_set_priority (self, g_value_get_int (value));
      break;

    case PROP_WORKBENCH:
      gb_command_provider_set_workbench (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_command_provider_class_init (GbCommandProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gb_command_provider_get_property;
  object_class->set_property = gb_command_provider_set_property;

  gParamSpecs [PROP_ACTIVE_TAB] =
    g_param_spec_object ("active-tab",
                         _("Active Tab"),
                         _("The last focused GbTab widget."),
                         GB_TYPE_TAB,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACTIVE_TAB,
                                   gParamSpecs [PROP_ACTIVE_TAB]);

  /**
   * GbCommandProvider:priority:
   *
   * The priority property denotes the order in which providers should be
   * queried. During the lookup process, providers are queried in order of
   * priority to parse the command text and resolve a GAction and optional
   * parameters.
   *
   * Lower priorities will be queried first.
   *
   * A negative priority is allowed;
   */
  gParamSpecs [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      _("Priority"),
                      _("The priority of the command provider."),
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PRIORITY,
                                   gParamSpecs [PROP_PRIORITY]);

  /**
   * GbCommandProvider:workbench:
   * 
   * The "workbench" property is the top-level window containing our project
   * and the workbench to work on it. It keeps track of the last focused tab
   * for convenience by action providers.
   */
  gParamSpecs [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         _("Workbench"),
                         _("The target workbench."),
                         GB_TYPE_WORKBENCH,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_WORKBENCH,
                                   gParamSpecs [PROP_WORKBENCH]);

  /**
   * GbCommandProvider::lookup:
   * @command_text: (in): the command line text to be processed.
   * @parameter: (out): a location to store any parsed parameters.
   *
   * This signal is emitted when a request to parse the command text is
   * received. Only the first handler will respond to the action. The
   * callee should return a GAction if successful, otherwise %NULL.
   * 
   * If successful, the callee can set @parameter, to specify the
   * parameters that should be passed to the resulting action.
   */
  gSignals [LOOKUP] =
    g_signal_new ("lookup",
                  GB_TYPE_COMMAND_PROVIDER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbCommandProviderClass, lookup),
                  g_signal_accumulator_first_wins,
                  NULL,
                  NULL,
                  GB_TYPE_COMMAND,
                  1,
                  G_TYPE_STRING);

  /**
   * GbCommandProvider::complete:
   * @completions: (in): A #GPtrArray where completed strings can be added
   * @initial_command_text: (in): the command line text to be processed.
   *
   * This signal is emitted when a request to complete a command text is
   * received. All providers should all all possible completions, matching
   * the initial test.
   */
  gSignals [COMPLETE] =
    g_signal_new ("complete",
                  GB_TYPE_COMMAND_PROVIDER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbCommandProviderClass, complete),
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_PTR_ARRAY,
                  G_TYPE_STRING);
}

static void
gb_command_provider_init (GbCommandProvider *self)
{
  self->priv = gb_command_provider_get_instance_private (self);
}
