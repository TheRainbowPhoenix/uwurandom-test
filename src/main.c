#include <gint/display.h>
#include <gint/serial.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <gint/exc.h>
#include <gint/clock.h>

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
void loop(uwu_settings_t settings)
{
    if (!settings.recv)
    {
        shiftstring_t *shs = malloc(sizeof(*shs));
        uwu_state state;
        char buf[SEND_BUFFER_SIZE] = { 0 };
        int rc;
        uint32_t bytes = 0;
        volatile bool flag = false;

        if ((rc = serial_open(&settings.serial)) != 0)
            gint_panic(-rc);

        shs_reset(shs);
        uwu_init_state(&state, uwu_op_table_default, ARRAY_SIZE(uwu_op_table_default));

        // TODO: allow breakouts
        float start = clock() / (float)CLOCKS_PER_SEC;
        while (1)
        {
            float diff = (clock() / (float)CLOCKS_PER_SEC) - start;
            flag = false;
            uwu_write_chars(&state, buf, sizeof(buf));
            shs_shiftin(shs, buf, sizeof(buf));
            serial_write_async(buf, sizeof(buf), GINT_CALL(set_flag, &flag));
            dclear(C_WHITE);
#if DWIDTH>128
            dprint(1,1,0x7BEF,"Sent %d bytes! (%d seconds | %.2f B/s)", bytes, (int) diff, (bytes / diff));
#else
            dprint(1,1,C_BLACK,"%dB|%ds|%.2f B/s", bytes, (int) diff, (bytes / diff));
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
            bytes += sizeof(buf);

            volatile int one = 1;
            getkey_opt(GETKEY_DEFAULT, &one);
        }
        uwu_destroy_state(&state);
    }
}
int main(void)
{
    uwu_settings_t settings = {
        /* 9600 baud, 8N1 */
        .serial.baudrate = 9600,
        .serial.data_width = 8,
        .serial.parity = SERIAL_PARITY_NONE,
        .serial.stop_bits = 1,
        .recv = false,
        .loopback = false
    };
#ifdef FXCG50
    extern font_t minifont;
    dfont(&minifont);
#endif
    srand(rtc_ticks());
    __printf_enable_fp();
    loop(settings);
	return 1;
}
