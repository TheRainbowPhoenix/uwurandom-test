#include <gint/display.h>
#include <gint/serial.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <gint/exc.h>
#include <gint/clock.h>

#include <justui/jwidget.h>
#include <justui/jlabel.h>
#include <justui/jscene.h>
#include <justui/jlist.h>

#include <fxlibc/printf.h>

#include "uwurandom_markov_data.h"
#include "uwurandom_ops.h"

typedef struct uwu_settings_t {
    serial_config_t serial;
    bool recv, loopback;
} uwu_settings_t;

#define SEND_BUFFER_SIZE 1
#define FONT_WIDTH 6
#define FONT_HEIGHT 8

#define LINE_WIDTH (DWIDTH/FONT_WIDTH)
#define LINE_HEIGHT ((DHEIGHT-16)/FONT_HEIGHT)
#define BUFFER_SIZE (LINE_WIDTH*LINE_HEIGHT)
typedef struct shiftstring_t {
    size_t count;
    char buffer[BUFFER_SIZE+4];
} shiftstring_t;

static void shs_reset(shiftstring_t *shs)
{
    shs->count = 0;
    memset(shs->buffer, 0, sizeof(shs->buffer));
}
static void shs_shiftin(shiftstring_t *shs, char *str, size_t count)
{
    size_t new = shs->count + count;
    size_t shiftout = 0;
    char *from = shs->buffer + shs->count;
    if (new >= BUFFER_SIZE)
    {
        shiftout = new - BUFFER_SIZE;
        shiftout = LINE_WIDTH * ((shiftout / LINE_WIDTH) + !!(shiftout % LINE_WIDTH));
    }

    if (shiftout > 0)
    {
        for (size_t i = shiftout; i < BUFFER_SIZE; i++)
            shs->buffer[i - shiftout] = shs->buffer[i];
        shs->count -= shiftout;
        from = shs->buffer + BUFFER_SIZE - shiftout;
        memset(from, 0, (uintptr_t)(&shs->buffer[BUFFER_SIZE+4] - from));
    }
    memcpy(from, str, count);
    shs->count += count;
    memset(shs->buffer + BUFFER_SIZE, 0, 4);
}

static void set_flag(volatile bool *flag)
{
    *flag = true;
}
#include <gint/mpu/scif.h>
void loop(uwu_settings_t settings)
{
    shiftstring_t *shs = malloc(sizeof(*shs));
    uwu_state state;
    char buf[SEND_BUFFER_SIZE] = { 0 };
    int rc;
    uint32_t bytes = 0;
    volatile bool flag = false;

    if ((rc = serial_open(&settings.serial)) != 0)
        gint_panic(-rc);
    SH7305_SCIF.SCFCR.LOOP = 1;

    shs_reset(shs);

    uwu_init_state(&state, uwu_op_table_default, ARRAY_SIZE(uwu_op_table_default));

    float start = clock() / (float)CLOCKS_PER_SEC;
    while (1)
    {
        float diff = (clock() / (float)CLOCKS_PER_SEC) - start;
        int data_count = 0;
        flag = false;
        if (!settings.recv)
        {
            data_count = sizeof(buf);
            uwu_write_chars(&state, buf, data_count);
            serial_write_async(buf, data_count, GINT_CALL(set_flag, &flag));
        }
        else
        {
            timeout_t tm = timeout_make_ms(10);
            int rc = serial_read_sync_timeout(buf, sizeof(buf), &tm);
            if (rc < 0) data_count = 0;
            else data_count = rc;
            flag = true;
        }
        shs_shiftin(shs, buf, data_count);
        dclear(C_WHITE);
#if DWIDTH>128
        const char *verb = settings.recv ? "Got" : "Sent";
        dprint(1,1,0x7BEF,"%s %d bytes! (%d seconds | %.2f B/s)", verb, bytes, (int) diff, (bytes / diff));
#else
        dprint(1,1,C_BLACK,"%dB|%.1f B/s", bytes, (bytes / diff));
#endif
        const char *end = shs->buffer + strlen(shs->buffer);
        for (size_t line = 0; line < (DHEIGHT-16)/FONT_HEIGHT; line++)
        {
            const size_t width = (DWIDTH/FONT_WIDTH);
            const char *buf = &shs->buffer[line * width];
            size_t len;
            if (buf >= end)
                break;
            len = strnlen(buf, width);

            dtext_opt(0, line * FONT_HEIGHT + 8, C_BLACK, C_WHITE, DTEXT_LEFT, DTEXT_TOP, buf, len);
        }
        dupdate();

        while (!flag) sleep();
        bytes += data_count;

        volatile int one = 1;
        getkey_opt(GETKEY_DEFAULT, &one);
    }
    uwu_destroy_state(&state);
}
typedef struct ui_param {
    const char *title;
    void (*write_value)(struct ui_param *self);
    void (*on_click)(struct ui_param *self);
    int data;

    struct {
        jwidget *hbox;
        jlabel *text;
        jlabel *value;
    } ui;
} ui_param_t;

static void
add_uiparam(jwidget *vbox, ui_param_t *param)
{
    param->ui.hbox = jwidget_create(vbox);
    param->ui.text = jlabel_create(param->title, param->ui.hbox);
    param->ui.value = jlabel_create("N/A", param->ui.hbox);

    jlayout_set_hbox(param->ui.hbox)->spacing = 1;
    jwidget_set_stretch(param->ui.hbox, 1, 0, true);
    jwidget_set_stretch(param->ui.text, 1, 0, true);

    if (param->write_value)
        param->write_value(param);
}

static int supported_baudrates[] = { 300, 600, 1200, 2400, 4800, 9600, 19200, 31250, 38400, 57600, 115200 };
static void write_baudrate(ui_param_t *param)
{
    jlabel_asprintf(param->ui.value, "%d", supported_baudrates[param->data]);
}
static void update_baudrate(ui_param_t *param)
{
    param->data = (param->data + 1) % (sizeof(supported_baudrates)/sizeof(supported_baudrates[0]));
}

static void write_dwidth(ui_param_t *param)
{
    jlabel_asprintf(param->ui.value, "%d-bits", param->data);
}
static void update_dwidth(ui_param_t *param)
{
    if (param->data == 7)
        param->data = 8;
    else 
        param->data = 7;
}

static void write_parity(ui_param_t *param)
{
    switch (param->data)
    {
        case SERIAL_PARITY_NONE:
            jlabel_asprintf(param->ui.value, "None");
            break;
        case SERIAL_PARITY_EVEN:
            jlabel_asprintf(param->ui.value, "Even");
            break;
        case SERIAL_PARITY_ODD:
            jlabel_asprintf(param->ui.value, "Odd");
            break;
    }
}
static void update_parity(ui_param_t *param)
{
    param->data = (param->data + 1) % 3;
}

static void write_fc(ui_param_t *param)
{
    jlabel_asprintf(param->ui.value, param->data ? "XON/XOFF" : "None");
}
static void update_fc(ui_param_t *param)
{
    param->data = param->data ? SERIAL_FLOWCONTROL_NONE : SERIAL_FLOWCONTROL_SOFTWARE;
}

static void write_sb(ui_param_t *param)
{
    jlabel_asprintf(param->ui.value, "%d-bits", param->data);
}
static void update_sb(ui_param_t *param)
{
    param->data = param->data == 1 ? 2 : 1;
}

static void write_rc(ui_param_t *param)
{
    jlabel_asprintf(param->ui.value, "%s", param->data ? "Yes" : "No");
}
static void update_rc(ui_param_t *param)
{
    param->data = !param->data;
}
static void write_none(ui_param_t *param)
{
    jlabel_asprintf(param->ui.value, "");
}

int main(void)
{
    srand(rtc_ticks());
    __printf_enable_fp();
    jscene *scene = jscene_create_fullscreen(NULL);
    jwidget *vbox = jwidget_create(scene);
    jlayout_set_vbox(vbox)->spacing = 1;
    jwidget_set_padding(vbox, 0, 0, 0, 0);
    jwidget_set_margin(vbox, 0, 0, 0, 0);
    jwidget_set_stretch(vbox, 1, 1, true);
    jlabel *title = jlabel_create("uwurandom", vbox);
    jwidget_set_stretch(title, 1, 0, false);
    jwidget_set_padding(title, 1, 1, 0, 1);
    jwidget_set_background(title, C_BLACK);
    jlabel_set_text_color(title, C_WHITE);

    ui_param_t ui_params[7] = {
        { .title = "Baud rate", .data = 5, .write_value = write_baudrate, .on_click = update_baudrate },
        { .title = "Data width", .data = 8, .write_value = write_dwidth, .on_click = update_dwidth },
        { .title = "Parity", .data = 0, .write_value = write_parity, .on_click = update_parity },
        { .title = "Flowcontrol", .data = 0, .write_value = write_fc, .on_click = update_fc },
        { .title = "Stop bits", .data = 1, .write_value = write_sb, .on_click = update_sb },
        { .title = "Receive?", .data = 0, .write_value = write_rc, .on_click = update_rc },

        { .title = "GO", .data = 0, .write_value = write_none, .on_click = update_rc }
    };
    static const size_t param_count = 7;
    for (size_t i = 0; i < param_count; i++)
        add_uiparam(vbox, &ui_params[i]);
    static ssize_t selected = 0;

    jwidget_set_background(ui_params[selected].ui.text, C_BLACK);
    jlabel_set_text_color(ui_params[selected].ui.text, C_WHITE);

    while (1)
    {
        jevent ev = jscene_run(scene);

        if (ev.type == JSCENE_PAINT)
        {
            dclear(C_WHITE);
            jscene_render(scene);
            dupdate();
        }

        if (ev.key.key == KEY_EXE && ev.key.type == KEYEV_UP)
        {
            ui_params[selected].on_click(&ui_params[selected]);
            ui_params[selected].write_value(&ui_params[selected]);

            if (selected == (param_count - 1))
                break;
        }
        else if (ev.key.key == KEY_DOWN && ev.key.type != KEYEV_UP)
        {
            jwidget_set_background(ui_params[selected].ui.text, C_WHITE);
            jlabel_set_text_color(ui_params[selected].ui.text, C_BLACK);
            selected = (selected + 1) % param_count;
            jwidget_set_background(ui_params[selected].ui.text, C_BLACK);
            jlabel_set_text_color(ui_params[selected].ui.text, C_WHITE);
        }
        else if (ev.key.key == KEY_UP && ev.key.type != KEYEV_UP)
        {
            jwidget_set_background(ui_params[selected].ui.text, C_WHITE);
            jlabel_set_text_color(ui_params[selected].ui.text, C_BLACK);
            if (selected == 0) selected = param_count - 1;
            else selected = (selected - 1);
            jwidget_set_background(ui_params[selected].ui.text, C_BLACK);
            jlabel_set_text_color(ui_params[selected].ui.text, C_WHITE);
        }
    }
    uwu_settings_t settings = {
        /* 9600 baud, 8N1 */
        .serial.baudrate = supported_baudrates[ui_params[0].data],
        .serial.data_width = ui_params[1].data,
        .serial.parity = ui_params[2].data,
        .serial.flow_control = ui_params[3].data,
        .serial.stop_bits = ui_params[4].data,
        .recv = ui_params[5].data,

        .loopback = false
    };
#ifdef FXCG50
    extern font_t minifont;
    dfont(&minifont);
#endif
    loop(settings);
	return 1;
}
