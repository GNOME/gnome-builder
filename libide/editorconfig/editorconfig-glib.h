/*
 * Authors: Christian Hergert <christian@hergert.me>
 *
 * The author or authors of this code dedicate any and all copyright interest
 * in this code to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and successors. We
 * intend this dedication to be an overt act of relinquishment in perpetuity of
 * all present and future rights to this code under copyright law.
 */

#ifndef EDITORCONFIG_GLIB_H
#define EDITORCONFIG_GLIB_H

#include <gio/gio.h>

GHashTable *editorconfig_glib_read (GFile         *file,
                                    GCancellable  *cancellable,
                                    GError       **error);

#endif /* EDITORCONFIG_GLIB_H */
