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

#define G_LOG_DOMAIN "gb-search-display"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-search-display.h"
#include "gb-search-display-group.h"

struct _GbSearchDisplay
{
  GtkBox                parent_instance;

  IdeSearchContext     *context;
  GPtrArray            *providers;
  GtkSizeGroup         *size_group;
  GbSearchDisplayGroup *last_group;
};

typedef struct
{
  IdeSearchProvider    *provider;
  GbSearchDisplayGroup *group;
} ProviderEntry;

G_DEFINE_TYPE (GbSearchDisplay, gb_search_display, GTK_TYPE_BOX)

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

  IDE_ENTRY;

  IDE_TRACE_MSG ("releasing %p", data);

  ide_clear_weak_pointer (&entry->group);
  g_clear_object (&entry->provider);
  g_free (entry);

  IDE_EXIT;
}

static gint
provider_entry_sort (gconstpointer ptra,
                     gconstpointer ptrb)
{
  ProviderEntry **entrya = (ProviderEntry **)ptra;
  ProviderEntry **entryb = (ProviderEntry **)ptrb;
  gint a;
  gint b;

  a = ide_search_provider_get_priority ((IDE_SEARCH_PROVIDER ((*entrya)->provider)));
  b = ide_search_provider_get_priority ((IDE_SEARCH_PROVIDER ((*entryb)->provider)));

  return a - b;
}

GtkWidget *
gb_search_display_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_DISPLAY, NULL);
}

static void
gb_search_display_real_result_activated (GbSearchDisplay *self,
                                         IdeSearchResult *result)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

}

static void
gb_search_display_result_activated (GbSearchDisplay      *self,
                                    GtkWidget            *widget,
                                    IdeSearchResult      *result,
                                    GbSearchDisplayGroup *group)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (group));

  g_signal_emit (self, gSignals [RESULT_ACTIVATED], 0, result);
}

static void
gb_search_display_result_selected (GbSearchDisplay      *self,
                                   IdeSearchResult      *result,
                                   GbSearchDisplayGroup *group)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (!result || IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (group));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);
      if ((ptr->group != NULL) && (ptr->group != group))
        gb_search_display_group_unselect (ptr->group);
    }
}

static gboolean
gb_search_display_keynav_failed (GbSearchDisplay      *self,
                                 GtkDirectionType      dir,
                                 GbSearchDisplayGroup *group)
{
  GList *list;
  GList *iter;
  gint position = -1;

  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY (self), FALSE);
  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (group), FALSE);

  gtk_container_child_get (GTK_CONTAINER (self), GTK_WIDGET (group),
                           "position", &position,
                           NULL);

  if (dir == GTK_DIR_DOWN)
    {
      list = gtk_container_get_children (GTK_CONTAINER (self));
      iter = g_list_nth (list, position + 1);
      if (iter && (iter->data != self->last_group))
        {
          gb_search_display_group_unselect (group);
          gb_search_display_group_focus_first (iter->data);
          return TRUE;
        }
    }
  else if (dir == GTK_DIR_UP && position > 0)
    {
      list = gtk_container_get_children (GTK_CONTAINER (self));
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
gb_search_display_activate (GbSearchDisplay *self)
{
  IdeSearchResult *result = NULL;
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));

  for (i = 0; !result && i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);
      if (ptr->group != NULL)
        result = gb_search_display_group_get_first (ptr->group);
    }

  if (result)
    g_signal_emit (self, gSignals [RESULT_ACTIVATED], 0, result);
}

static void
gb_search_display_add_provider (GbSearchDisplay   *self,
                                IdeSearchProvider *provider)
{
  ProviderEntry *entry;
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));

  /*
   * Make sure we don't add an item twice. Probably can assert here, but
   * warning will do for now.
   */
  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

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
  entry = g_new0 (ProviderEntry, 1);
  entry->provider = g_object_ref (provider);
  entry->group = g_object_new (GB_TYPE_SEARCH_DISPLAY_GROUP,
                               "size-group", self->size_group,
                               "provider", provider,
                               "visible", FALSE,
                               NULL);
  g_object_add_weak_pointer (G_OBJECT (entry->group), (gpointer *)&entry->group);
  g_signal_connect_object (entry->group,
                           "result-activated",
                           G_CALLBACK (gb_search_display_result_activated),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (entry->group,
                           "result-selected",
                           G_CALLBACK (gb_search_display_result_selected),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (entry->group,
                           "keynav-failed",
                           G_CALLBACK (gb_search_display_keynav_failed),
                           self,
                           G_CONNECT_SWAPPED);
  g_ptr_array_add (self->providers, entry);
  g_ptr_array_sort (self->providers, provider_entry_sort);

  /*
   * Find the location of the entry and use the index to pack the display
   * group widget.
   */
  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ptr->provider == provider)
        {
          gtk_container_add_with_properties (GTK_CONTAINER (self),
                                             GTK_WIDGET (entry->group),
                                             "position", i,
                                             NULL);
          break;
        }
    }
}

static void
gb_search_display_remove_provider (GbSearchDisplay   *self,
                                   IdeSearchProvider *provider)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ptr->provider == provider)
        {
          GbSearchDisplayGroup *group = ptr->group;

          if (group)
            gtk_container_remove (GTK_CONTAINER (self), GTK_WIDGET (group));
          g_ptr_array_remove_index (self->providers, i);
          return;
        }
    }

  g_warning (_("The provider could not be found."));
}

static void
gb_search_display_result_added (GbSearchDisplay   *self,
                                IdeSearchProvider *provider,
                                IdeSearchResult   *result,
                                IdeSearchContext  *context)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ptr->provider == provider)
        {
          if (ptr->group != NULL)
            {
              gb_search_display_group_add_result (ptr->group, result);
              gtk_widget_show (GTK_WIDGET (ptr->group));
            }
          break;
        }
    }
}

static void
gb_search_display_result_removed (GbSearchDisplay   *self,
                                  IdeSearchProvider *provider,
                                  IdeSearchResult   *result,
                                  IdeSearchContext  *context)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ptr->provider == provider)
        {
          if (ptr->group != NULL)
            gb_search_display_group_remove_result (ptr->group, result);
          break;
        }
    }
}

static void
gb_search_display_count_set (GbSearchDisplay   *self,
                             IdeSearchProvider *provider,
                             guint64            count,
                             IdeSearchContext  *context)
{
  guint i;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ptr->provider == provider)
        {
          if (ptr->group != NULL)
            gb_search_display_group_set_count (ptr->group, count);
          break;
        }
    }
}

static void
gb_search_display_connect_context (GbSearchDisplay  *self,
                                   IdeSearchContext *context)
{
  const GList *providers;
  const GList *iter;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));

  providers = ide_search_context_get_providers (context);

  for (iter = providers; iter; iter = iter->next)
    gb_search_display_add_provider (self, iter->data);

  g_signal_connect_object (context,
                           "result-added",
                           G_CALLBACK (gb_search_display_result_added),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (context,
                           "result-removed",
                           G_CALLBACK (gb_search_display_result_removed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (context,
                           "count-set",
                           G_CALLBACK (gb_search_display_count_set),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_search_display_disconnect_context (GbSearchDisplay  *self,
                                      IdeSearchContext *context)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));

  g_signal_handlers_disconnect_by_func (context,
                                        G_CALLBACK (gb_search_display_result_added),
                                        self);

  while (self->providers->len)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers,
                               self->providers->len - 1);
      gb_search_display_remove_provider (self, ptr->provider);
    }
}

IdeSearchContext *
gb_search_display_get_context (GbSearchDisplay *self)
{
  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY (self), NULL);

  return self->context;
}

void
gb_search_display_set_context (GbSearchDisplay  *self,
                               IdeSearchContext *context)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));
  g_return_if_fail (!context || IDE_IS_SEARCH_CONTEXT (context));

  if (self->context != context)
    {
      if (self->context)
        {
          gb_search_display_disconnect_context (self, self->context);
          g_clear_object (&self->context);
        }

      if (context)
        {
          self->context = g_object_ref (context);
          gb_search_display_connect_context (self, self->context);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_CONTEXT]);
    }
}

static void
gb_search_display_grab_focus (GtkWidget *widget)
{
  GbSearchDisplay *self = (GbSearchDisplay *)widget;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY (self));

  if (self->providers->len)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, 0);
      gtk_widget_child_focus (GTK_WIDGET (ptr->group), GTK_DIR_DOWN);
    }
}

static void
gb_search_display_dispose (GObject *object)
{
  GbSearchDisplay *self = (GbSearchDisplay *)object;

  g_clear_pointer (&self->providers, g_ptr_array_unref);
  g_clear_object (&self->context);
  g_clear_object (&self->size_group);

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

  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The active search context."),
                         IDE_TYPE_SEARCH_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gSignals [RESULT_ACTIVATED] =
    g_signal_new_class_handler ("result-activated",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (gb_search_display_real_result_activated),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                1,
                                IDE_TYPE_SEARCH_RESULT);
}

static void
gb_search_display_init (GbSearchDisplay *self)
{
  self->providers = g_ptr_array_new_with_free_func (provider_entry_destroy);

  self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);

  self->last_group = g_object_new (GB_TYPE_SEARCH_DISPLAY_GROUP,
                                         "size-group", self->size_group,
                                         "visible", TRUE,
                                         "vexpand", TRUE,
                                         NULL);
  gtk_container_add (GTK_CONTAINER (self),
                     GTK_WIDGET (self->last_group));
}
