/*
 * gedit-menu-extension.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Ignacio Casal Quinteiro
 *
 * gb is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gb is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gb. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ide-menu-extension.h"

#include <string.h>

static guint last_merge_id = 0;

struct _IdeMenuExtension
{
	GObject parent_instance;

	GMenu *menu;
	guint merge_id;
	gboolean dispose_has_run;
};

enum
{
	PROP_0,
	PROP_MENU
};

G_DEFINE_TYPE (IdeMenuExtension, ide_menu_extension, G_TYPE_OBJECT)

static void
ide_menu_extension_dispose (GObject *object)
{
	IdeMenuExtension *menu = IDE_MENU_EXTENSION (object);

	if (!menu->dispose_has_run)
	{
		ide_menu_extension_remove_items (menu);
		menu->dispose_has_run = TRUE;
	}

	g_clear_object (&menu->menu);

	G_OBJECT_CLASS (ide_menu_extension_parent_class)->dispose (object);
}

static void
ide_menu_extension_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	IdeMenuExtension *menu = IDE_MENU_EXTENSION (object);

	switch (prop_id)
	{
		case PROP_MENU:
			g_value_set_object (value, menu->menu);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ide_menu_extension_set_property (GObject     *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	IdeMenuExtension *menu = IDE_MENU_EXTENSION (object);

	switch (prop_id)
	{
		case PROP_MENU:
			menu->menu = g_value_dup_object (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ide_menu_extension_class_init (IdeMenuExtensionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ide_menu_extension_dispose;
	object_class->get_property = ide_menu_extension_get_property;
	object_class->set_property = ide_menu_extension_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_MENU,
	                                 g_param_spec_object ("menu",
	                                                      "Menu",
	                                                      "The main menu",
	                                                      G_TYPE_MENU,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));
}

static void
ide_menu_extension_init (IdeMenuExtension *menu)
{
	menu->merge_id = ++last_merge_id;
}

IdeMenuExtension *
ide_menu_extension_new (GMenu *menu)
{
	return g_object_new (IDE_TYPE_MENU_EXTENSION, "menu", menu, NULL);
}

IdeMenuExtension *
ide_menu_extension_new_for_section (GMenu       *menu,
                                   const gchar *section)
{
	guint n_items;
	guint i;

	n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu));

	for (i = 0; i < n_items; i++)
	{
		g_autoptr(GMenuAttributeIter) iter = NULL;

		iter = g_menu_model_iterate_item_attributes (G_MENU_MODEL (menu), i);

                while (g_menu_attribute_iter_next (iter))
		{
			g_autoptr(GVariant) variant = NULL;
			const gchar *name;
			const gchar *key;

			name = g_menu_attribute_iter_get_name (iter);
			if (g_strcmp0 (name, "id") != 0)
				continue;

			variant = g_menu_attribute_iter_get_value (iter);
			if (!g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
				continue;

			key = g_variant_get_string (variant, NULL);
			if (g_strcmp0 (key, section) == 0)
			{
				GMenuModel *section_menu;

				section_menu = g_menu_model_get_item_link (G_MENU_MODEL (menu), i, G_MENU_LINK_SECTION);

				if (!G_IS_MENU (section_menu))
					continue;

				return g_object_new (IDE_TYPE_MENU_EXTENSION, "menu", section_menu, NULL);
			}
		}
	}

	g_warning ("Failed to locate section \"%s\". "
	           "Ensure you have set the <attribute name=\"id\"> element.",
	           section);

	return NULL;
}

void
ide_menu_extension_append_menu_item (IdeMenuExtension *menu,
                                    GMenuItem       *item)
{
	g_return_if_fail (IDE_IS_MENU_EXTENSION (menu));
	g_return_if_fail (G_IS_MENU_ITEM (item));

	if (menu->menu != NULL)
	{
		g_menu_item_set_attribute (item, "gb-merge-id", "u", menu->merge_id);
		g_menu_append_item (menu->menu, item);
	}
}

void
ide_menu_extension_prepend_menu_item (IdeMenuExtension *menu,
                                     GMenuItem       *item)
{
	g_return_if_fail (IDE_IS_MENU_EXTENSION (menu));
	g_return_if_fail (G_IS_MENU_ITEM (item));

	if (menu->menu != NULL)
	{
		g_menu_item_set_attribute (item, "gb-merge-id", "u", menu->merge_id);
		g_menu_prepend_item (menu->menu, item);
	}
}

void
ide_menu_extension_remove_items (IdeMenuExtension *menu)
{
	gint i, n_items;

	g_return_if_fail (IDE_IS_MENU_EXTENSION (menu));

	n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu->menu));
	i = 0;
	while (i < n_items)
	{
		guint id = 0;

		if (g_menu_model_get_item_attribute (G_MENU_MODEL (menu->menu),
		                                     i, "gb-merge-id", "u", &id) &&
		    id == menu->merge_id)
		{
			g_menu_remove (menu->menu, i);
			n_items--;
		}
		else
		{
			i++;
		}
	}
}

/* ex:set ts=8 noet: */
