/* gb-command-manager.h
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

#ifndef GB_COMMAND_MANAGER_H
#define GB_COMMAND_MANAGER_H

#include <gio/gio.h>

#include "gb-command.h"
#include "gb-command-provider.h"

G_BEGIN_DECLS

#define GB_TYPE_COMMAND_MANAGER            (gb_command_manager_get_type())
#define GB_COMMAND_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_MANAGER, GbCommandManager))
#define GB_COMMAND_MANAGER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_COMMAND_MANAGER, GbCommandManager const))
#define GB_COMMAND_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_COMMAND_MANAGER, GbCommandManagerClass))
#define GB_IS_COMMAND_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_COMMAND_MANAGER))
#define GB_IS_COMMAND_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_COMMAND_MANAGER))
#define GB_COMMAND_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_COMMAND_MANAGER, GbCommandManagerClass))

typedef struct _GbCommandManager        GbCommandManager;
typedef struct _GbCommandManagerClass   GbCommandManagerClass;
typedef struct _GbCommandManagerPrivate GbCommandManagerPrivate;

struct _GbCommandManager
{
  GObject parent;

  /*< private >*/
  GbCommandManagerPrivate *priv;
};

struct _GbCommandManagerClass
{
  GObjectClass parent;
};

GType      gb_command_manager_get_type     (void) G_GNUC_CONST;
GbCommand *gb_command_manager_lookup       (GbCommandManager  *manager,
                                            const gchar       *command_text);
gchar **   gb_command_manager_complete     (GbCommandManager  *manager,
                                            const gchar       *initial_command_text);
void       gb_command_manager_add_provider (GbCommandManager  *manager,
                                            GbCommandProvider *provider);

G_END_DECLS

#endif /* GB_COMMAND_MANAGER_H */
