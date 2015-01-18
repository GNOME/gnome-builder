/* gb-search-display.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include "gb-search-display.h"
#include "gb-search-display-group.h"
#include "gb-search-provider.h"
#include "gb-search-result.h"

struct _GbSearchDisplayPrivate
{
  GbSearchContext      *context;
  GArray               *providers;
  GtkSizeGroup         *size_group;
  GbSearchDisplayGroup *last_group;
};

typedef struct
{
  GbSearchProvider     *provider;
  GbSearchDisplayGroup *group;
} ProviderEntry;

G_DEFINE_TYPE_WITH_PRIVATE (GbSearchDisplay, gb_search_display, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_CONTEXT,
  LAST_PROP
};

enum {
  RESULT_ACTIVATED,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void
provider_entry_destroy (gpointer data)
{
  ProviderEntry *entry = data;

  g_clear_object (&entry->provider);
}

static gint
provider_entry_sort (gconstpointer ptra,
                     gconstpointer ptrb)
{
  const ProviderEntry *entrya = ptra;
  const ProviderEntry *entryb = ptrb;
  gint a;
  gint b;

  a = gb_search_provider_get_priority ((GB_SEARCH_PROVIDER (entrya->provider)));
  b = gb_search_provider_get_priority ((GB_SEARCH_PROVIDER (entryb->provider)));

  return a - b;
}

GtkWidget *
gb_search_display_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_DISPLAY, NULL);
}

static void
gb_search_display_real_result_activated (GbSearchDisplay *display,
                                         GbSearchResult  *result)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_RESULT (result));

  gb_search_result_activate (result);
}

static void
gb_search_display_result_activated (GbSearchDisplay      *display,
                                    GbSearchResult       *result,
                                    GbSearchDisplayGroup *group)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (!result || GB_IS_SEARCH_RESULT (result));
  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (group));

  g_signal_emit (display, gSignals [RESULT_ACTIVATED], 0, result);
}

static void
gb_search_display_result_selected (GbSearchDisplay      *display,
                                   GbSearchResult       *result,
                                   GbSearchDisplayGroup *group)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (!result || GB_IS_SEARCH_RESULT (result));
  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (group));

  for (i = 0; i < display->priv->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (display->priv->providers, ProviderEntry, i);
      if (ptr->group != group)
        gb_search_display_group_unselect (ptr->group);
    }
}

static gboolean
gb_search_display_keynav_failed (GbSearchDisplay      *display,
                                 GtkDirectionType      dir,
                                 GbSearchDisplayGroup *group)
{
  GList *list;
  GList *iter;
  gint position = -1;

  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY (display), FALSE);
  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (group), FALSE);

  gtk_container_child_get (GTK_CONTAINER (display), GTK_WIDGET (group),
                           "position", &position,
                           NULL);

  if (dir == GTK_DIR_DOWN)
    {
      list = gtk_container_get_children (GTK_CONTAINER (display));
      iter = g_list_nth (list, position + 1);
      if (iter && (iter->data != display->priv->last_group))
        {
          gb_search_display_group_unselect (group);
          gb_search_display_group_focus_first (iter->data);
          return TRUE;
        }
    }
  else if (dir == GTK_DIR_UP && position > 0)
    {
      list = gtk_container_get_children (GTK_CONTAINER (display));
      iter = g_list_nth (list, position - 1);
      if (iter)
        {
          gb_search_display_group_unselect (group);
          gb_search_display_group_focus_last (iter->data);
          return TRUE;
        }
    }

  return FALSE;
}

void
gb_search_display_activate (GbSearchDisplay *display)
{
  g_warning ("TODO: implement display_activate()");
}

static void
gb_search_display_add_provider (GbSearchDisplay  *display,
                                GbSearchProvider *provider)
{
  ProviderEntry entry = { 0 };
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  /*
   * Make sure we don't add an item twice. Probably can assert here, but
   * warning will do for now.
   */
  for (i = 0; i < display->priv->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (display->priv->providers, ProviderEntry, i);

      if (ptr->provider == provider)
        {
          g_warning (_("Cannot add provider more than once."));
          return;
        }
    }

  /*
   * Add the entry to our array and sort the array to determine our target
   * widget packing position.
   */
  entry.provider = g_object_ref (provider);
  entry.group = g_object_new (GB_TYPE_SEARCH_DISPLAY_GROUP,
                              "size-group", display->priv->size_group,
                              "provider", provider,
                              "visible", FALSE,
                              NULL);
  g_signal_connect_object (entry.group,
                           "result-activated",
                           G_CALLBACK (gb_search_display_result_activated),
                           display,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (entry.group,
                           "result-selected",
                           G_CALLBACK (gb_search_display_result_selected),
                           display,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (entry.group,
                           "keynav-failed",
                           G_CALLBACK (gb_search_display_keynav_failed),
                           display,
                           G_CONNECT_SWAPPED);
  g_array_append_val (display->priv->providers, entry);
  g_array_sort (display->priv->providers, provider_entry_sort);

  /*
   * Find the location of the entry and use the index to pack the display
   * group widget.
   */
  for (i = 0; i < display->priv->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (display->priv->providers, ProviderEntry, i);

      if (ptr->provider == provider)
        {
          gtk_container_add_with_properties (GTK_CONTAINER (display),
                                             GTK_WIDGET (entry.group),
                                             "position", i,
                                             NULL);
          break;
        }
    }
}

static void
gb_search_display_remove_provider (GbSearchDisplay  *display,
                                   GbSearchProvider *provider)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  for (i = 0; i < display->priv->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (display->priv->providers, ProviderEntry, i);

      if (ptr->provider == provider)
        {
          gtk_container_remove (GTK_CONTAINER (display),
                                GTK_WIDGET (ptr->group));
          g_array_remove_index (display->priv->providers, i);
          return;
        }
    }

  g_warning (_("The provider could not be found."));
}

static void
gb_search_display_result_added (GbSearchDisplay  *display,
                                GbSearchProvider *provider,
                                GbSearchResult   *result,
                                GbSearchContext  *context)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (GB_IS_SEARCH_RESULT (result));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  for (i = 0; i < display->priv->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (display->priv->providers, ProviderEntry, i);

      if (ptr->provider == provider)
        {
          gb_search_display_group_add_result (ptr->group, result);
          gtk_widget_show (GTK_WIDGET (ptr->group));
          break;
        }
    }
}

static void
gb_search_display_result_removed (GbSearchDisplay  *display,
                                  GbSearchProvider *provider,
                                  GbSearchResult   *result,
                                  GbSearchContext  *context)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (GB_IS_SEARCH_RESULT (result));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  for (i = 0; i < display->priv->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (display->priv->providers, ProviderEntry, i);

      if (ptr->provider == provider)
        {
          gb_search_display_group_remove_result (ptr->group, result);
          break;
        }
    }
}

static void
gb_search_display_count_set (GbSearchDisplay  *display,
                             GbSearchProvider *provider,
                             guint64           count,
                             GbSearchContext  *context)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  for (i = 0; i < display->priv->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (display->priv->providers, ProviderEntry, i);

      if (ptr->provider == provider)
        {
          gb_search_display_group_set_count (ptr->group, count);
          break;
        }
    }
}

static void
gb_search_display_connect_context (GbSearchDisplay *display,
                                   GbSearchContext *context)
{
  const GList *providers;
  const GList *iter;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  providers = gb_search_context_get_providers (context);

  for (iter = providers; iter; iter = iter->next)
    gb_search_display_add_provider (display, iter->data);

  g_signal_connect_object (context,
                           "result-added",
                           G_CALLBACK (gb_search_display_result_added),
                           display,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (context,
                           "result-removed",
                           G_CALLBACK (gb_search_display_result_removed),
                           display,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (context,
                           "count-set",
                           G_CALLBACK (gb_search_display_count_set),
                           display,
                           G_CONNECT_SWAPPED);
}

static void
gb_search_display_disconnect_context (GbSearchDisplay *display,
                                      GbSearchContext *context)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));

  while (display->priv->providers->len)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (display->priv->providers, ProviderEntry,
                            display->priv->providers->len - 1);
      gb_search_display_remove_provider (display, ptr->provider);
    }

  g_signal_handlers_disconnect_by_func (context,
                                        G_CALLBACK (gb_search_display_result_added),
                                        display);
}

GbSearchContext *
gb_search_display_get_context (GbSearchDisplay *display)
{
  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY (display), NULL);

  return display->priv->context;
}

void
gb_search_display_set_context (GbSearchDisplay *display,
                               GbSearchContext *context)
{
  GbSearchDisplayPrivate *priv;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (display));
  g_return_if_fail (!context || GB_IS_SEARCH_CONTEXT (context));

  priv = display->priv;

  if (priv->context != context)
    {
      if (priv->context)
        {
          gb_search_display_disconnect_context (display, priv->context);
          g_clear_object (&display->priv->context);
        }

      if (context)
        {
          priv->context = g_object_ref (context);
          gb_search_display_connect_context (display, priv->context);
        }

      g_object_notify_by_pspec (G_OBJECT (display), gParamSpecs [PROP_CONTEXT]);
    }
}

static void
gb_search_display_grab_focus (GtkWidget *widget)
{
  GbSearchDisplay *self = (GbSearchDisplay *)widget;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));

  if (self->priv->providers->len)
    {
      ProviderEntry *ptr;

      ptr = &g_array_index (self->priv->providers, ProviderEntry, 0);
      gtk_widget_child_focus (GTK_WIDGET (ptr->group), GTK_DIR_DOWN);
    }
}

static void
gb_search_display_dispose (GObject *object)
{
  GbSearchDisplayPrivate *priv = GB_SEARCH_DISPLAY (object)->priv;

  g_clear_pointer (&priv->providers, g_array_unref);
  g_clear_object (&priv->context);
  g_clear_object (&priv->size_group);

  G_OBJECT_CLASS (gb_search_display_parent_class)->dispose (object);
}

static void
gb_search_display_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbSearchDisplay *self = GB_SEARCH_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, gb_search_display_get_context (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_display_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbSearchDisplay *self = GB_SEARCH_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      gb_search_display_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_display_class_init (GbSearchDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = gb_search_display_grab_focus;

  object_class->dispose = gb_search_display_dispose;
  object_class->get_property = gb_search_display_get_property;
  object_class->set_property = gb_search_display_set_property;

  klass->result_activated = gb_search_display_real_result_activated;

  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The active search context."),
                         GB_TYPE_SEARCH_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONTEXT,
                                   gParamSpecs [PROP_CONTEXT]);

  gSignals [RESULT_ACTIVATED] =
    g_signal_new ("result-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSearchDisplayClass, result_activated),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_SEARCH_RESULT);
}

static void
gb_search_display_init (GbSearchDisplay *self)
{
  self->priv = gb_search_display_get_instance_private (self);

  self->priv->providers = g_array_new (FALSE, FALSE, sizeof (ProviderEntry));
  g_array_set_clear_func (self->priv->providers,
                          (GDestroyNotify)provider_entry_destroy);

  self->priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);

  self->priv->last_group = g_object_new (GB_TYPE_SEARCH_DISPLAY_GROUP,
                                         "size-group", self->priv->size_group,
                                         "visible", TRUE,
                                         "vexpand", TRUE,
                                         NULL);
  gtk_container_add (GTK_CONTAINER (self),
                     GTK_WIDGET (self->priv->last_group));
}
