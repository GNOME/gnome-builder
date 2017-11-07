/* gb-command-provider.c
 *
 * Copyright © 2014 Christian Hergert <christian@hergert.me>
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

typedef struct
{
  IdeWorkbench  *workbench;
  IdeLayoutView *active_view;
  gint           priority;
} GbCommandProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GbCommandProvider, gb_command_provider, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ACTIVE_VIEW,
  PROP_PRIORITY,
  PROP_WORKBENCH,
  LAST_PROP
};

enum {
  LOOKUP,
  COMPLETE,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

GbCommandProvider *
gb_command_provider_new (IdeWorkbench *workbench)
{
  return g_object_new (GB_TYPE_COMMAND_PROVIDER,
                       "workbench", workbench,
                       NULL);
}

/**
 * gb_command_provider_get_active_view:
 *
 * Returns the "active-tab" property. The active-tab is the last tab that
 * was focused in the workbench.
 *
 * Returns: (transfer none): an #IdeLayoutView or %NULL.
 */
IdeLayoutView *
gb_command_provider_get_active_view (GbCommandProvider *provider)
{
  GbCommandProviderPrivate *priv = gb_command_provider_get_instance_private (provider);

  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), NULL);

  return priv->active_view;
}

static void
gb_command_provider_set_active_view (GbCommandProvider *provider,
                                     IdeLayoutView     *tab)
{
  GbCommandProviderPrivate *priv = gb_command_provider_get_instance_private (provider);

  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (!tab || IDE_IS_LAYOUT_VIEW (tab));

  if (priv->active_view)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->active_view),
                                    (gpointer *)&priv->active_view);
      priv->active_view = NULL;
    }

  if (tab)
    {
      priv->active_view = tab;
      g_object_add_weak_pointer (G_OBJECT (priv->active_view),
                                 (gpointer *)&priv->active_view);
    }

  g_object_notify_by_pspec (G_OBJECT (provider),
                            properties [PROP_ACTIVE_VIEW]);
}

static void
on_workbench_set_focus (GbCommandProvider *provider,
                        GtkWidget         *widget,
                        IdeWorkbench      *workbench)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  /* walk the hierarchy to find a tab */
  if (widget)
    while (!IDE_IS_LAYOUT_VIEW (widget))
      if (!(widget = gtk_widget_get_parent (widget)))
        break;

  if (IDE_IS_LAYOUT_VIEW (widget))
    gb_command_provider_set_active_view (provider,
                                         IDE_LAYOUT_VIEW (widget));
}

static void
gb_command_provider_connect (GbCommandProvider *provider,
                             IdeWorkbench      *workbench)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  g_signal_connect_object (workbench,
                           "set-focus",
                           G_CALLBACK (on_workbench_set_focus),
                           provider,
                           G_CONNECT_SWAPPED);
}

static void
gb_command_provider_disconnect (GbCommandProvider *provider,
                                IdeWorkbench      *workbench)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (IDE_IS_WORKBENCH (workbench));

  g_signal_handlers_disconnect_by_func (workbench,
                                        G_CALLBACK (on_workbench_set_focus),
                                        provider);
}

IdeWorkbench *
gb_command_provider_get_workbench (GbCommandProvider *provider)
{
  GbCommandProviderPrivate *priv = gb_command_provider_get_instance_private (provider);

  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), NULL);

  return priv->workbench;
}

static void
gb_command_provider_set_workbench (GbCommandProvider *provider,
                                   IdeWorkbench      *workbench)
{
  GbCommandProviderPrivate *priv = gb_command_provider_get_instance_private (provider);

  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (!workbench || IDE_IS_WORKBENCH (workbench));

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
                                properties [PROP_WORKBENCH]);
  }
}

gint
gb_command_provider_get_priority (GbCommandProvider *provider)
{
  GbCommandProviderPrivate *priv = gb_command_provider_get_instance_private (provider);

  g_return_val_if_fail (GB_IS_COMMAND_PROVIDER (provider), 0);

  return priv->priority;
}

void
gb_command_provider_set_priority (GbCommandProvider *provider,
                                  gint               priority)
{
  GbCommandProviderPrivate *priv = gb_command_provider_get_instance_private (provider);

  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));

  if (priv->priority != priority)
    {
      priv->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (provider),
                                properties [PROP_PRIORITY]);
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

  g_signal_emit (provider, signals [LOOKUP], 0, command_text, &ret);

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

  g_signal_emit (provider, signals [COMPLETE], 0, completions, initial_command_text);
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
    case PROP_ACTIVE_VIEW:
      g_value_set_object (value, gb_command_provider_get_active_view (self));
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

  properties [PROP_ACTIVE_VIEW] =
    g_param_spec_object ("active-tab",
                         "Active View",
                         "The last focused IdeLayoutView widget.",
                         IDE_TYPE_LAYOUT_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

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
  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The priority of the command provider.",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GbCommandProvider:workbench:
   *
   * The "workbench" property is the top-level window containing our project
   * and the workbench to work on it. It keeps track of the last focused tab
   * for convenience by action providers.
   */
  properties [PROP_WORKBENCH] =
    g_param_spec_object ("workbench",
                         "Workbench",
                         "The target workbench.",
                         IDE_TYPE_WORKBENCH,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

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
  signals [LOOKUP] =
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
  signals [COMPLETE] =
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
}
