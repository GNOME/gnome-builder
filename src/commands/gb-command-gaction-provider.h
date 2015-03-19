/* gb-command-gaction-provider.h
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

#ifndef GB_COMMAND_GACTION_PROVIDER_H
#define GB_COMMAND_GACTION_PROVIDER_H

#include "gb-command-provider.h"

G_BEGIN_DECLS

#define GB_TYPE_COMMAND_GACTION_PROVIDER            (gb_command_gaction_provider_get_type())
#define GB_COMMAND_GACTION_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_GACTION_PROVIDER, GbCommandGactionProvider))
#define GB_COMMAND_GACTION_PROVIDER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_GACTION_PROVIDER, GbCommandGactionProvider const))
#define GB_COMMAND_GACTION_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_COMMAND_GACTION_PROVIDER, GbCommandGactionProviderClass))
#define GB_IS_COMMAND_GACTION_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_COMMAND_GACTION_PROVIDER))
#define GB_IS_COMMAND_GACTION_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_COMMAND_GACTION_PROVIDER))
#define GB_COMMAND_GACTION_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_COMMAND_GACTION_PROVIDER, GbCommandGactionProviderClass))

typedef struct _GbCommandGactionProvider        GbCommandGactionProvider;
typedef struct _GbCommandGactionProviderClass   GbCommandGactionProviderClass;
typedef struct _GbCommandGactionProviderPrivate GbCommandGactionProviderPrivate;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GbCommandGactionProvider, g_object_unref)

struct _GbCommandGactionProvider
{
  GbCommandProvider parent;

  /*< private >*/
  GbCommandGactionProviderPrivate *priv;
};

struct _GbCommandGactionProviderClass
{
  GbCommandProviderClass parent;
};

GType              gb_command_gaction_provider_get_type (void);
GbCommandProvider *gb_command_gaction_provider_new      (GbWorkbench *workbench);

G_END_DECLS

#endif /* GB_COMMAND_GACTION_PROVIDER_H */
