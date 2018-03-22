/* gbp-devhelp-layout-stack-addin.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-devhelp-layout-stack-addin"

#include "gbp-devhelp-layout-stack-addin.h"
#include "gbp-devhelp-menu-button.h"
#include "gbp-devhelp-view.h"

struct _GbpDevhelpLayoutStackAddin
{
  GObject               parent_instance;
  IdeLayoutStack       *stack;
  GbpDevhelpMenuButton *button;
};

static void
gbp_devhelp_layout_stack_addin_search (GSimpleAction *action,
                                       GVariant      *variant,
                                       gpointer       user_data)
{
  GbpDevhelpLayoutStackAddin *self = user_data;
  const gchar *keyword;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_DEVHELP_LAYOUT_STACK_ADDIN (self));
  g_assert (self->stack != NULL);
  g_assert (IDE_IS_LAYOUT_STACK (self->stack));
  g_assert (variant != NULL);
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  if (self->button != NULL)
    {
      keyword = g_variant_get_string (variant, NULL);
      gbp_devhelp_menu_button_search (self->button, keyword);
    }
}

static void
gbp_devhelp_layout_stack_addin_new_view (GSimpleAction *action,
                                         GVariant      *variant,
                                         gpointer       user_data)
{
  GbpDevhelpLayoutStackAddin *self = user_data;
  GbpDevhelpView *view;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_DEVHELP_LAYOUT_STACK_ADDIN (self));
  g_assert (self->stack != NULL);
  g_assert (IDE_IS_LAYOUT_STACK (self->stack));

  view = g_object_new (GBP_TYPE_DEVHELP_VIEW,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (self->stack), GTK_WIDGET (view));
}

static void
gbp_devhelp_layout_stack_addin_navigate_to (GSimpleAction *action,
                                            GVariant      *variant,
                                            gpointer       user_data)
{
  GbpDevhelpLayoutStackAddin *self = user_data;
  IdeLayoutView *view;
  const gchar *uri;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_DEVHELP_LAYOUT_STACK_ADDIN (self));
  g_assert (self->stack != NULL);
  g_assert (IDE_IS_LAYOUT_STACK (self->stack));
  g_assert (variant != NULL);
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING));

  uri = g_variant_get_string (variant, NULL);
  view = ide_layout_stack_get_visible_child (self->stack);

  if (GBP_IS_DEVHELP_VIEW (view))
    gbp_devhelp_view_set_uri (GBP_DEVHELP_VIEW (view), uri);
}

static GActionEntry actions[] = {
  { "new-view", gbp_devhelp_layout_stack_addin_new_view },
  { "search", gbp_devhelp_layout_stack_addin_search, "s" },
  { "navigate-to", gbp_devhelp_layout_stack_addin_navigate_to, "s" },
};

static void
gbp_devhelp_layout_stack_addin_load (IdeLayoutStackAddin *addin,
                                     IdeLayoutStack      *stack)
{
  GbpDevhelpLayoutStackAddin *self = (GbpDevhelpLayoutStackAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (GBP_IS_DEVHELP_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  self->stack = stack;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (stack),
                                  "devhelp",
                                  G_ACTION_GROUP (group));
}

static void
gbp_devhelp_layout_stack_addin_unload (IdeLayoutStackAddin *addin,
                                       IdeLayoutStack      *stack)
{
  GbpDevhelpLayoutStackAddin *self = (GbpDevhelpLayoutStackAddin *)addin;

  g_assert (GBP_IS_DEVHELP_LAYOUT_STACK_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  self->stack = NULL;

  gtk_widget_insert_action_group (GTK_WIDGET (stack), "devhelp", NULL);

  if (self->button != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->button));
}

static void
gbp_devhelp_layout_stack_addin_set_view (IdeLayoutStackAddin *addin,
                                         IdeLayoutView       *view)
{
  GbpDevhelpLayoutStackAddin *self = (GbpDevhelpLayoutStackAddin *)addin;
  gboolean visible = FALSE;

  g_assert (GBP_IS_DEVHELP_LAYOUT_STACK_ADDIN (self));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));
  g_assert (self->stack != NULL);
  g_assert (IDE_IS_LAYOUT_STACK (self->stack));

  /*
   * We don't setup self->button until we get our first devhelp
   * view. This helps reduce startup overhead as well as lower
   * memory footprint until it is necessary.
   */

  if (GBP_IS_DEVHELP_VIEW (view))
    {
      if (self->button == NULL)
        {
          GtkWidget *titlebar;

          titlebar = ide_layout_stack_get_titlebar (self->stack);

          self->button = g_object_new (GBP_TYPE_DEVHELP_MENU_BUTTON,
                                       "hexpand", TRUE,
                                       NULL);
          g_signal_connect (self->button,
                            "destroy",
                            G_CALLBACK (gtk_widget_destroyed),
                            &self->button);
          ide_layout_stack_header_add_custom_title (IDE_LAYOUT_STACK_HEADER (titlebar),
                                                    GTK_WIDGET (self->button),
                                                    100);
        }

      visible = TRUE;
    }

  if (self->button != NULL)
    gtk_widget_set_visible (GTK_WIDGET (self->button), visible);
}

static void
layout_stack_addin_iface_init (IdeLayoutStackAddinInterface *iface)
{
  iface->load = gbp_devhelp_layout_stack_addin_load;
  iface->unload = gbp_devhelp_layout_stack_addin_unload;
  iface->set_view = gbp_devhelp_layout_stack_addin_set_view;
}

G_DEFINE_TYPE_WITH_CODE (GbpDevhelpLayoutStackAddin,
                         gbp_devhelp_layout_stack_addin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_LAYOUT_STACK_ADDIN,
                                                layout_stack_addin_iface_init))

static void
gbp_devhelp_layout_stack_addin_class_init (GbpDevhelpLayoutStackAddinClass *klass)
{
}

static void
gbp_devhelp_layout_stack_addin_init (GbpDevhelpLayoutStackAddin *self)
{
}
