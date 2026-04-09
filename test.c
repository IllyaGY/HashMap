#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "hashmap.h"

#define NAME_CAPACITY 128
#define STATUS_CAPACITY 256

typedef struct render_ctx {
    int row;
    int max_rows;
    int hidden_count;
} render_ctx;

static void set_status(char *status, size_t status_size, const char *message){
    if (status == NULL || status_size == 0)
    {
        return;
    }

    snprintf(status, status_size, "%s", message);
}

static int prompt_name(const char *prompt, char *buffer, size_t buffer_size){
    int max_y = 0;
    int max_x = 0;

    getmaxyx(stdscr, max_y, max_x);
    nodelay(stdscr, FALSE);
    echo();
    curs_set(1);

    move(max_y - 1, 0);
    clrtoeol();
    mvprintw(max_y - 1, 0, "%s", prompt);
    refresh();

    int rc = getnstr(buffer, (int)buffer_size - 1);

    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);

    if (rc == ERR)
    {
        return ERR;
    }

    buffer[buffer_size - 1] = '\0';
    if (buffer[0] == '\0')
    {
        return EINVAL;
    }

    (void)max_x;
    return 0;
}

static void render_entry(
    size_t bucket_index,
    size_t chain_index,
    const void *key,
    size_t key_len,
    const void *value,
    size_t value_len,
    void *ctx
){
    (void)key_len;
    (void)value_len;

    render_ctx *render = ctx;
    if (render == NULL)
    {
        return;
    }

    if (render->row >= render->max_rows)
    {
        render->hidden_count++;
        return;
    }

    mvprintw(
        render->row,
        0,
        "bucket=%zu chain=%zu key=%s value=%s",
        bucket_index,
        chain_index,
        (const char *)key,
        (const char *)value
    );
    render->row++;
}

static int draw_screen(const hashmap *map, const char *status){
    size_t size = 0;
    size_t capacity = 0;
    int err = get_size_hm(map, &size);
    if (err != 0)
    {
        return err;
    }

    err = get_capacity_hm(map, &capacity);
    if (err != 0)
    {
        return err;
    }

    int max_y = 0;
    int max_x = 0;
    getmaxyx(stdscr, max_y, max_x);

    clear();
    mvprintw(0, 0, "=== HashMap ===");
    mvprintw(1, 0, "size=%zu capacity=%zu", size, capacity);
    mvprintw(2, 0, "commands: i = insert, r = remove, q = quit");

    render_ctx ctx = {
        .row = 4,
        .max_rows = max_y - 2,
        .hidden_count = 0
    };

    if (size == 0)
    {
        mvprintw(ctx.row, 0, "(empty)");
    }
    else
    {
        err = foreach_hm(map, render_entry, &ctx);
        if (err != 0)
        {
            return err;
        }
    }

    if (ctx.hidden_count > 0)
    {
        mvprintw(max_y - 2, 0, "... %d more entr%s hidden", ctx.hidden_count, ctx.hidden_count == 1 ? "y" : "ies");
    }

    move(max_y - 1, 0);
    clrtoeol();
    mvprintw(max_y - 1, 0, "%s", status);
    (void)max_x;
    refresh();
    return 0;
}

int main(void){
    hashmap *map = NULL;
    if (create_hm(&map, 8) != 0)
    {
        fprintf(stderr, "failed to create hashmap\n");
        return 1;
    }

    char command[16];
    char name[NAME_CAPACITY];
    char status[STATUS_CAPACITY];
    set_status(status, sizeof(status), "ready");

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);

    while (1) {
        int err = draw_screen(map, status);
        if (err != 0)
        {
            endwin();
            fprintf(stderr, "display failed: %d\n", err);
            delete_hm(&map);
            return 1;
        }

        int ch = getch();
        if (ch == ERR)
        {
            napms(16);
            continue;
        }

        command[0] = (char)ch;
        command[1] = '\0';

        switch (command[0]) {
            case 'i':
            {
                int prompt_rc = prompt_name("name to add: ", name, sizeof(name));
                if (prompt_rc == ERR)
                {
                    set_status(status, sizeof(status), "input cancelled");
                    break;
                }
                if (prompt_rc == EINVAL)
                {
                    set_status(status, sizeof(status), "please enter a non-empty name");
                    break;
                }

                err = insert_hm(map, name, strlen(name) + 1, name, strlen(name) + 1);
                if (err != 0)
                {
                    snprintf(status, sizeof(status), "insert failed: %d", err);
                }
                else
                {
                    snprintf(status, sizeof(status), "added: %s", name);
                }
                break;
            }

            case 'r':
            {
                int prompt_rc = prompt_name("name to remove: ", name, sizeof(name));
                if (prompt_rc == ERR)
                {
                    set_status(status, sizeof(status), "input cancelled");
                    break;
                }
                if (prompt_rc == EINVAL)
                {
                    set_status(status, sizeof(status), "please enter a non-empty name");
                    break;
                }

                err = remove_hm(map, name, strlen(name) + 1);
                if (err == ENOENT)
                {
                    snprintf(status, sizeof(status), "name not found: %s", name);
                }
                else if (err != 0)
                {
                    snprintf(status, sizeof(status), "remove failed: %d", err);
                }
                else
                {
                    snprintf(status, sizeof(status), "removed: %s", name);
                }
                break;
            }

            case 'q':
                endwin();
                delete_hm(&map);
                return 0;

            default:
                set_status(status, sizeof(status), "unknown command");
                break;
        }
    }
}
