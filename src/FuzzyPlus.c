/* Fuzzy text watchface that shows more detail when shaken */
#include <pebble.h>
#include <autoconfig.h>

enum
{
    FONT_HUGE,
    FONT_LARGE,
    FONT_MEDIUM,
    FONT_SMALL,
    FONT_DETAIL,
    FONT_TINY,
    FONT_COUNT
};

static uint32_t font_resources[FONT_COUNT] = {
    RESOURCE_ID_FONT_UBUNTU_48,
    RESOURCE_ID_FONT_UBUNTU_38,
    RESOURCE_ID_FONT_UBUNTU_28,
    RESOURCE_ID_FONT_UBUNTU_20,
    RESOURCE_ID_FONT_UBUNTU_16,
    RESOURCE_ID_FONT_UBUNTU_10,
};

/* struct to define a text layer */
struct TextBox
{
    int16_t             x, y, w, h;
    int                 font_index;
    GTextAlignment      alignment;
};

/* enum to provide indexes for text layers */
enum
{
    BOX_TOP_DETAIL,
    BOX_LINE1,
    BOX_LINE2,
    BOX_LINE3,
    BOX_BIG_HOUR,
    BOX_BOTTOM_DETAIL,
    BOX_TIME,
    BOX_AM_PM,
    BOX_COUNT
};

/* TextBox objects to define the text layers */
static struct TextBox boxes[] =
{
    {   0,   0, 144, 28, FONT_SMALL,    GTextAlignmentRight },  /* TOP_DETAIL */
    {   0,  20, 144, 48, FONT_LARGE,    GTextAlignmentLeft },   /* LINE1 */
    {   0,  54, 144, 48, FONT_LARGE,    GTextAlignmentCenter }, /* LINE2 */
    {   0,  92, 144, 48, FONT_LARGE,    GTextAlignmentRight },  /* LINE3 */
    {   0,  40, 144, 60, FONT_HUGE,     GTextAlignmentCenter }, /* BIG_HOUR */
    {   0, 148,  90, 20, FONT_DETAIL,   GTextAlignmentLeft },   /* BOTTOM_DETAIL */
    {  72, 148,  54, 20, FONT_DETAIL,   GTextAlignmentRight },  /* TIME */
    { 128, 154,  16, 14, FONT_TINY,     GTextAlignmentLeft },   /* AM_PM */
};

/* How many minutes around each quarter that doesn't need qualification */
#define MINUTE_TOLERANCE 3

static Window* s_main_window;
static GFont fonts[FONT_COUNT];
static struct TextLayer* layers[BOX_COUNT];
static bool detail_visible = false;

/* ============================== Testing Data ============================== */
/* Data to test layout/behavior.  Set TESTING to true and edit testing_values,
 * then a tap will move one step through the testing_values data */
#define TESTING false

static const int testing_hour = 0;
static const int testing_minutes[] = {
    00,
    MINUTE_TOLERANCE,
    MINUTE_TOLERANCE + 1,
    15 - MINUTE_TOLERANCE - 1,
    15,
    15 + MINUTE_TOLERANCE + 1,
    30 - MINUTE_TOLERANCE - 1,
    30,
    30 + MINUTE_TOLERANCE + 1,
    45 - MINUTE_TOLERANCE - 1,
    45,
    45 + MINUTE_TOLERANCE + 1,
    60 - MINUTE_TOLERANCE - 1,
};

static const int testing_value_count = sizeof(testing_minutes) / sizeof(int);
static int testing_index = 0;

/* ========================================================================== */
const char*
hour_to_string(int hour)
{
    static const char* hour_strings[] =
        {
            "twelve",
            "one",
            "two",
            "three",
            "four",
            "five",
            "six",
            "seven",
            "eight",
            "nine",
            "ten",
            "eleven",
        };

    return hour_strings[hour % 12];
}

/* ========================================================================== */
static void
show_time(struct tm* tick_time)
{
    /* clear out boxes */
    for (int i = 0; i < BOX_COUNT; ++i)
        text_layer_set_text(layers[i], "");

    /* work out which quarter, and what our offset from that is */
    int quarter = (tick_time->tm_min + 7) / 15;
    int offset = tick_time->tm_min - (quarter * 15);
    const char* hour_string = hour_to_string(
        quarter > 2 ? tick_time->tm_hour + 1 : tick_time->tm_hour);
    quarter %= 4;

    /* Indicate if we're outside MINUTE_TOLERANCE of the quarter */
    if (abs(offset) > MINUTE_TOLERANCE)
    {
        if (offset < -MINUTE_TOLERANCE)
            text_layer_set_text(layers[BOX_TOP_DETAIL], "getting on for");
        else
            text_layer_set_text(layers[BOX_TOP_DETAIL], "gone");
    }

    /* display the time to the nearest quarter */
    if (quarter != 0)
    {
        text_layer_set_text(layers[BOX_LINE1], quarter == 2 ? " half" : "quarter");
        text_layer_set_text(layers[BOX_LINE2], quarter == 3 ? "  to" : "past");
        text_layer_set_text(layers[BOX_LINE3], hour_string);
    }
    else
    {
        bool short_enough = strlen(hour_string) < 7;
        text_layer_set_text(layers[short_enough ? BOX_BIG_HOUR : BOX_LINE2], hour_string);
    }

    /* optionally display digital time */
    if (detail_visible)
    {
        static char date_str[7];
        static char time_str[6];
        static char am_pm_str[3];
        strftime(date_str, sizeof(date_str), "%a %d", tick_time);
        text_layer_set_text(layers[BOX_BOTTOM_DETAIL], date_str);
        strftime(time_str, sizeof(time_str), "%l:%M", tick_time);
        text_layer_set_text(layers[BOX_TIME], time_str);
        strftime(am_pm_str, sizeof(am_pm_str), "%P", tick_time);
        text_layer_set_text(layers[BOX_AM_PM], am_pm_str);
    }
}

/* ========================================================================== */
static void
tap_handler(AccelAxisType axis, int32_t direction)
{
    /* toggle detail_visible on tap and redisplay */
    detail_visible = !detail_visible;
    time_t now = time(NULL);
    struct tm* time_struct = localtime(&now);

    if (TESTING)
    {
        time_struct->tm_hour = testing_hour;
        time_struct->tm_min = testing_minutes[testing_index++];
        testing_index %= testing_value_count;
    }

    show_time(time_struct);
}

/* ========================================================================== */
static void
handle_minute_tick(struct tm *tick_time, TimeUnits units_changed)
{
    detail_visible = false;
	show_time(tick_time);
}

/* ========================================================================== */
static void
load_fonts()
{
    for (int i = 0; i < FONT_COUNT; ++i)
        fonts[i] = fonts_load_custom_font(resource_get_handle(font_resources[i]));
}

/* ========================================================================== */
static void
unload_fonts()
{
    for (int i = 0; i < FONT_COUNT; ++i)
        fonts_unload_custom_font(fonts[i]);
}

/* ========================================================================== */
static void
create_text_layers(Window* window)
{
    Layer* window_layer = window_get_root_layer(window);

    for (int i = 0; i < BOX_COUNT; ++i)
    {
        struct TextBox* box = &boxes[i];
        layers[i] = text_layer_create(GRect(box->x, box->y, box->w, box->h));
        text_layer_set_font(layers[i], fonts[box->font_index]);
        text_layer_set_background_color(layers[i], GColorClear);
        text_layer_set_text_color(layers[i], GColorWhite);
        text_layer_set_text_alignment(layers[i], box->alignment);
        text_layer_set_overflow_mode(layers[i], GTextOverflowModeWordWrap);
        layer_add_child(window_layer, text_layer_get_layer(layers[i]));
    }
}

/* ========================================================================== */
static void
destroy_text_layers()
{
    for (int i = 0; i < BOX_COUNT; ++i)
        text_layer_destroy(layers[i]);
}

/* ========================================================================== */
static void
main_window_load(Window* window)
{
    load_fonts();
    create_text_layers(window);
}

/* ========================================================================== */
static void
main_window_unload(Window* window)
{
    destroy_text_layers();
    unload_fonts();
}

/* ========================================================================== */
/* autoconfig received handler */
static void
in_received_handler(DictionaryIterator *iter,
                    void *context)
{
    autoconfig_in_received_handler(iter, context);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Configuration updated. BeforeText: %s", getBeforetext()); 
}

/* ========================================================================== */
static void
init()
{
    autoconfig_init();
    app_message_register_inbox_received(in_received_handler);

    /* Create main Window */
    s_main_window = window_create();
    window_set_window_handlers(s_main_window, (WindowHandlers) {
            .load = main_window_load,
            .unload = main_window_unload
        });
    window_set_background_color(s_main_window, GColorBlack);
    window_stack_push(s_main_window, true);

    /* Subscribe to the accelerometer tap service */
    accel_tap_service_subscribe(tap_handler);

    /* subscribe for tick timer */
    if (!TESTING)
        tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

    /* display the current time */
    time_t t = time(NULL);
    show_time(localtime(&t));
}

/* ========================================================================== */
static void
deinit()
{
    // Destroy main Window
    window_destroy(s_main_window);

    accel_tap_service_unsubscribe();

    autoconfig_deinit();
}

/* ========================================================================== */
int
main(void)
{
    init();
    app_event_loop();
    deinit();
}
