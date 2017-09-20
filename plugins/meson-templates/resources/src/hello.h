{{include "license.h"}}

#ifndef {{NAME}}_H
#define {{NAME}}_H

#include <glib.h>

G_BEGIN_DECLS

#define {{NAME}}_INSIDE
# include "{{prefix}}-version.h"
#undef {{NAME}}_INSIDE

G_END_DECLS

#endif /* {{NAME}}_H */
