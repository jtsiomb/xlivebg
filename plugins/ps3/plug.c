#include "xlivebg.h"
#include "props.h"
#include "ps3.h"

static struct xlivebg_plugin plugin = {
	"ps3",
	"PS3 xmb wave",
	PROPLIST,
	XLIVEBG_30FPS,
	init, deinit,
	start, stop,
	draw,
	prop,
	0, 0
};


int register_plugin(void) {
	return xlivebg_register_plugin(&plugin);
}
