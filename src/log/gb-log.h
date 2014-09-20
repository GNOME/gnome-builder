/* gb-log.h
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
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

#ifndef GB_LOG_H
#define GB_LOG_H

#include <glib.h>

G_BEGIN_DECLS

#ifndef G_LOG_LEVEL_TRACE
#define G_LOG_LEVEL_TRACE (1 << G_LOG_LEVEL_USER_SHIFT)
#endif

#ifdef GB_ENABLE_TRACE
#define TRACE_MSG(fmt, ...)                                            \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, "TRACE: %s():%d: "fmt,       \
         G_STRFUNC, __LINE__, ##__VA_ARGS__)
#define TRACE                                                          \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, "TRACE: %s():%d",            \
         G_STRFUNC, __LINE__)
#define TODO(_msg)                                                     \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, " TODO: %s():%d: %s",        \
         G_STRFUNC, __LINE__, _msg)
#define ENTRY                                                          \
   g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, "ENTRY: %s():%d",            \
         G_STRFUNC, __LINE__)
#define EXIT                                                           \
   G_STMT_START {                                                      \
      g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, " EXIT: %s():%d",         \
            G_STRFUNC, __LINE__);                                      \
      return;                                                          \
   } G_STMT_END
#define GOTO(_l)                                                       \
   G_STMT_START {                                                      \
      g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, " GOTO: %s():%d ("#_l")", \
            G_STRFUNC, __LINE__);                                      \
      goto _l;                                                         \
   } G_STMT_END
#define RETURN(_r)                                                     \
   G_STMT_START {                                                      \
      g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE, " EXIT: %s():%d ",        \
            G_STRFUNC, __LINE__);                                      \
      return _r;                                                       \
   } G_STMT_END
#else
#define TODO(_msg)
#define TRACE
#define TRACE_MSG(fmt, ...)
#define ENTRY
#define GOTO(_l)   goto _l
#define EXIT       return
#define RETURN(_r) return _r
#endif

void gb_log_init     (gboolean     stdout_,
                      const gchar *filename);
void gb_log_shutdown (void);

G_END_DECLS

#endif /* GB_LOG_H */
