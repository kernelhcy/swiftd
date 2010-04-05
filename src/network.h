#ifndef __SWIFTD__NETWORK_H
#define __SWIFTD_NETWORK_H

#include "base.h"

int network_init(server *srv);
void network_close(server *srv);

int network_register_fdevent(server *srv);

#endif
