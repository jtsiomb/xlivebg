
#define PROPLIST	\
	"proplist {\n" \
	"    prop {\n" \
	"        id = \"light_angle\"\n" \
	"        desc = \"Angle the directional light is facing on the YZ plane\"\n" \
	"        type = \"number\"\n" \
	"        range = [0.0, 360.0]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"chaos\"\n" \
	"        desc = \"How noisy the wave shape is\"\n" \
	"        type = \"number\"\n" \
	"        range = [0.01, 2.0]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"detail\"\n" \
	"        desc = \"How detailed the wave shape is\"\n" \
	"        type = \"number\"\n" \
	"        range = [0.1, 50]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"speed\"\n" \
	"        desc = \"How fast the wave moves\"\n" \
	"        type = \"number\"\n" \
	"        range = [0.1, 50.0]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"scale\"\n" \
	"        desc = \"How big the difference between the wave peaks and dips is\"\n" \
	"        type = \"number\"\n" \
	"        range = [0.1, 2.0]\n" \
	"    }\n" \
	"    prop {\n" \
	"        id = \"wave_color\"\n" \
	"        desc = \"wave color\"\n" \
	"        type = \"color\"\n" \
	"    }\n" \
	"}\n"

#define CHAOS_DEFAULT 1.36f
#define DETAIL_DEFAULT 2.2f
#define SPEED_DEFAULT 0.9f
#define SCALE_DEFAULT 0.39f
#define WAVE_COLOR_DEFAULT_INIT { 0.2f, 0.5372549f, 0.7529411f }
static float WAVE_COLOR_DEFAULT[] = WAVE_COLOR_DEFAULT_INIT;
#define LIGHT_ANGLE_DEFAULT 140.0f
#define SECONDARY_CHAOS_DEFAULT 1.17f
#define SECONDARY_DETAIL_DEFAULT 0.9f
#define SECONDARY_SPEED_DEFAULT 1.3f
#define SECONDARY_SCALE_DEFAULT 0.84f