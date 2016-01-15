/* ide-omni-search-display.c
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

#define G_LOG_DOMAIN "ide-omni-search-display"

#include <glib/gi18n.h>

#include "ide-omni-search-group.h"
#include "ide-omni-search-display.h"

struct _IdeOmniSearchDisplay
{
  GtkBox               parent_instance;

  IdeSearchContext    *context;
  GPtrArray           *providers;

  guint                do_autoselect : 1;
};

typedef struct
{
  IdeSearchProvider   *provider;
  IdeOmniSearchGroup  *group;
} ProviderEntry;

G_DEFINE_TYPE (IdeOmniSearchDisplay, ide_omni_search_display, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_CONTEXT,
  LAST_PROP
};

enum {
  ACTIVATE,
  RESULT_ACTIVATED,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint       signals [LAST_SIGNAL];

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
ide_omni_search_display_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_SEARCH_DISPLAY, NULL);
}

static void
ide_omni_search_display_real_result_activated (IdeOmniSearchDisplay *self,
                                               IdeSearchResult      *result)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

}

static void
ide_omni_search_display_result_activated (IdeOmniSearchDisplay *self,
                                          GtkWidget            *widget,
                                          IdeSearchResult      *result,
                                          IdeOmniSearchGroup   *group)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (group));

  g_signal_emit (self, signals [RESULT_ACTIVATED], 0, result);
}

static void
ide_omni_search_display_result_selected (IdeOmniSearchDisplay *self,
                                         IdeSearchResult      *result,
                                         IdeOmniSearchGroup   *group)
{
  guint i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
  g_return_if_fail (!result || IDE_IS_SEARCH_RESULT (result));
  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (group));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);
      if ((ptr->group != NULL) && (ptr->group != group))
        ide_omni_search_group_unselect (ptr->group);
    }
}

void
ide_omni_search_display_move_next_result (IdeOmniSearchDisplay *self)
{
  gint i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));

  self->do_autoselect = FALSE;

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr = g_ptr_array_index (self->providers, i);

      if (ide_omni_search_group_has_selection (ptr->group))
        {
          while (ptr && !ide_omni_search_group_move_next (ptr->group))
            {
              ide_omni_search_group_unselect (ptr->group);

              if (i < (self->providers->len - 1))
                ptr = g_ptr_array_index (self->providers, ++i);
              else
                ptr = NULL;
            }

          if (ptr == NULL)
            break;

          return;
        }
    }

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr = g_ptr_array_index (self->providers, i);

      if (ide_omni_search_group_move_next (ptr->group))
        break;
    }
}

void
ide_omni_search_display_move_previous_result (IdeOmniSearchDisplay *self)
{
  gint i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));

  self->do_autoselect = FALSE;

  for (i = self->providers->len - 1; i >= 0; i--)
    {
      ProviderEntry *ptr = g_ptr_array_index (self->providers, i);

      if (ide_omni_search_group_has_selection (ptr->group))
        {
          while (ptr && !ide_omni_search_group_move_previous (ptr->group))
            {
              ide_omni_search_group_unselect (ptr->group);
              if (i > 0)
                ptr = g_ptr_array_index (self->providers, --i);
              else
                ptr = NULL;
            }

          if (ptr == NULL)
            break;

          return;
        }
    }

  for (i = self->providers->len - 1; i >= 0; i--)
    {
      ProviderEntry *ptr = g_ptr_array_index (self->providers, i);

      if (ide_omni_search_group_move_previous (ptr->group))
        return;
    }
}

static void
ide_omni_search_display_activate (IdeOmniSearchDisplay *self)
{
  gsize i;

  g_assert (IDE_IS_OMNI_SEARCH_DISPLAY (self));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ptr->group != NULL)
        {
          if (ide_omni_search_group_activate (ptr->group))
            break;
        }
    }
}

static void
ide_omni_search_display_add_provider (IdeOmniSearchDisplay *self,
                                      IdeSearchProvider    *provider)
{
  ProviderEntry *entry;
  guint i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
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
  entry->group = g_object_new (IDE_TYPE_OMNI_SEARCH_GROUP,
                               "provider", provider,
                               "visible", FALSE,
                               NULL);
  g_object_add_weak_pointer (G_OBJECT (entry->group), (gpointer *)&entry->group);
  g_signal_connect_object (entry->group,
                           "result-activated",
                           G_CALLBACK (ide_omni_search_display_result_activated),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (entry->group,
                           "result-selected",
                           G_CALLBACK (ide_omni_search_display_result_selected),
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
ide_omni_search_display_remove_provider (IdeOmniSearchDisplay *self,
                                         IdeSearchProvider    *provider)
{
  guint i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ptr->provider == provider)
        {
          IdeOmniSearchGroup *group = ptr->group;

          if (group)
            gtk_container_remove (GTK_CONTAINER (self), GTK_WIDGET (group));
          g_ptr_array_remove_index (self->providers, i);
          return;
        }
    }

  g_warning (_("The provider could not be found."));
}

static void
ide_omni_search_display_result_added (IdeOmniSearchDisplay *self,
                                      IdeSearchProvider    *provider,
                                      IdeSearchResult      *result,
                                      IdeSearchContext     *context)
{
  guint i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
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
              ide_omni_search_group_add_result (ptr->group, result);
              gtk_widget_show (GTK_WIDGET (ptr->group));

              /*
               * If this is the first group and we are still auto-selecting
               * the first row, we might need to update the selection.
               */
              if ((i == 0) && self->do_autoselect)
                ide_omni_search_group_select_first (ptr->group);
            }
          break;
        }
    }
}

static void
ide_omni_search_display_result_removed (IdeOmniSearchDisplay *self,
                                        IdeSearchProvider    *provider,
                                        IdeSearchResult      *result,
                                        IdeSearchContext     *context)
{
  guint i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
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
            ide_omni_search_group_remove_result (ptr->group, result);
          break;
        }
    }
}

static void
ide_omni_search_display_count_set (IdeOmniSearchDisplay *self,
                                   IdeSearchProvider    *provider,
                                   guint64               count,
                                   IdeSearchContext     *context)
{
#if 0
  guint i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ptr->provider == provider)
        {
          if (ptr->group != NULL)
            ide_omni_search_group_set_count (ptr->group, count);
          break;
        }
    }
#endif
}

static void
ide_omni_search_display_connect_context (IdeOmniSearchDisplay *self,
                                         IdeSearchContext     *context)
{
  const GList *providers;
  const GList *iter;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));

  self->do_autoselect = TRUE;

  providers = ide_search_context_get_providers (context);

  for (iter = providers; iter; iter = iter->next)
    ide_omni_search_display_add_provider (self, iter->data);

  g_signal_connect_object (context,
                           "result-added",
                           G_CALLBACK (ide_omni_search_display_result_added),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (context,
                           "result-removed",
                           G_CALLBACK (ide_omni_search_display_result_removed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (context,
                           "count-set",
                           G_CALLBACK (ide_omni_search_display_count_set),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_omni_search_display_disconnect_context (IdeOmniSearchDisplay *self,
                                            IdeSearchContext     *context)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));

  g_signal_handlers_disconnect_by_func (context,
                                        G_CALLBACK (ide_omni_search_display_result_added),
                                        self);

  while (self->providers->len)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers,
                               self->providers->len - 1);
      ide_omni_search_display_remove_provider (self, ptr->provider);
    }
}

IdeSearchContext *
ide_omni_search_display_get_context (IdeOmniSearchDisplay *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self), NULL);

  return self->context;
}

void
ide_omni_search_display_set_context (IdeOmniSearchDisplay *self,
                                     IdeSearchContext     *context)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));
  g_return_if_fail (!context || IDE_IS_SEARCH_CONTEXT (context));

  if (self->context != context)
    {
      if (self->context)
        {
          ide_omni_search_display_disconnect_context (self, self->context);
          g_clear_object (&self->context);
        }

      if (context)
        {
          self->context = g_object_ref (context);
          ide_omni_search_display_connect_context (self, self->context);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
    }
}

static void
ide_omni_search_display_grab_focus (GtkWidget *widget)
{
  IdeOmniSearchDisplay *self = (IdeOmniSearchDisplay *)widget;
  gsize i;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self));

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *ptr;

      ptr = g_ptr_array_index (self->providers, i);

      if (ide_omni_search_group_get_first (ptr->group))
        {
          gtk_widget_child_focus (GTK_WIDGET (ptr->group), GTK_DIR_DOWN);
          break;
        }
    }
}

static void
ide_omni_search_display_dispose (GObject *object)
{
  IdeOmniSearchDisplay *self = (IdeOmniSearchDisplay *)object;

  g_clear_pointer (&self->providers, g_ptr_array_unref);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (ide_omni_search_display_parent_class)->dispose (object);
}

static void
ide_omni_search_display_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeOmniSearchDisplay *self = IDE_OMNI_SEARCH_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_omni_search_display_get_context (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_search_display_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeOmniSearchDisplay *self = IDE_OMNI_SEARCH_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_omni_search_display_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_search_display_class_init (IdeOmniSearchDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = ide_omni_search_display_grab_focus;

  object_class->dispose = ide_omni_search_display_dispose;
  object_class->get_property = ide_omni_search_display_get_property;
  object_class->set_property = ide_omni_search_display_set_property;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The active search context.",
                         IDE_TYPE_SEARCH_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [ACTIVATE] = widget_class->activate_signal =
    g_signal_new_class_handler ("activate",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_omni_search_display_activate),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [RESULT_ACTIVATED] =
    g_signal_new_class_handler ("result-activated",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_omni_search_display_real_result_activated),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                1,
                                IDE_TYPE_SEARCH_RESULT);

  gtk_widget_class_set_css_name (widget_class, "omnisearchdisplay");
}

static void
ide_omni_search_display_init (IdeOmniSearchDisplay *self)
{
  self->providers = g_ptr_array_new_with_free_func (provider_entry_destroy);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
}

guint64
ide_omni_search_display_get_count (IdeOmniSearchDisplay *self)
{
  guint64 count = 0;
  gint i;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_DISPLAY (self), 0);

  for (i = 0; i < self->providers->len; i++)
    {
      ProviderEntry *provider = g_ptr_array_index (self->providers, i);
      count += ide_omni_search_group_get_count (provider->group);
    }

  return count;
}
