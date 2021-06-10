#include <rg_system.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

#include "gui.h"

#define IMAGE_LOGO_WIDTH    (47)
#define IMAGE_LOGO_HEIGHT   (51)
#define IMAGE_BANNER_WIDTH  (272)
#define IMAGE_BANNER_HEIGHT (32)

#define LIST_WIDTH          (RG_SCREEN_WIDTH)
#define LIST_HEIGHT         (RG_SCREEN_HEIGHT - LIST_Y_OFFSET)
#define LIST_LINE_COUNT     (LIST_HEIGHT / rg_gui_get_font_info().height)
#define LIST_X_OFFSET       (0)
#define LIST_Y_OFFSET       (48 + 8)

#define COVER_MAX_HEIGHT    (184)
#define COVER_MAX_WIDTH     (184)

static const theme_t gui_themes[] = {
    {0, C_GRAY, C_WHITE, C_AQUA},
    {0, C_GRAY, C_GREEN, C_AQUA},
    {0, C_WHITE, C_GREEN, C_AQUA},

    {5, C_GRAY, C_WHITE, C_AQUA},
    {5, C_GRAY, C_GREEN, C_AQUA},
    {5, C_WHITE, C_GREEN, C_AQUA},

    {11, C_GRAY, C_WHITE, C_AQUA},
    {11, C_GRAY, C_GREEN, C_AQUA},
    {11, C_WHITE, C_GREEN, C_AQUA},

    {16, C_GRAY, C_WHITE, C_AQUA},
    {16, C_GRAY, C_GREEN, C_AQUA},
    {16, C_WHITE, C_GREEN, C_AQUA},
};
const int gui_themes_count = sizeof(gui_themes) / sizeof(theme_t);

retro_gui_t gui;


void gui_event(gui_event_t event, tab_t *tab)
{
    if (tab && tab->event_handler)
        (*tab->event_handler)(event, tab);
}

tab_t *gui_add_tab(const char *name, const rg_image_t *logo, const rg_image_t *header, void *arg, void *event_handler)
{
    tab_t *tab = calloc(1, sizeof(tab_t));

    sprintf(tab->name, "%.63s", name);
    sprintf(tab->status[1].left, "Loading...");

    tab->event_handler = event_handler;
    tab->img_header = header;
    tab->img_logo = logo;
    tab->initialized = false;
    tab->is_empty = false;
    tab->arg = arg;

    gui.tabs[gui.tabcount++] = tab;

    RG_LOGI("Tab '%s' added at index %d\n", tab->name, gui.tabcount - 1);

    return tab;
}

void gui_init_tab(tab_t *tab)
{
    if (tab->initialized)
        return;

    tab->initialized = true;
    // tab->status[0] = 0;

    char key_name[32];
    sprintf(key_name, "Sel.%.11s", tab->name);
    tab->listbox.cursor = rg_settings_get_app_int32(key_name, 0);

    gui_event(TAB_INIT, tab);

    tab->listbox.cursor = RG_MAX(RG_MIN(tab->listbox.cursor, tab->listbox.length - 1), 0);
    gui_event(TAB_SCROLL, tab);
}

tab_t *gui_get_tab(int index)
{
    return (index >= 0 && index < gui.tabcount) ? gui.tabs[index] : NULL;
}

tab_t *gui_get_current_tab()
{
    return gui_get_tab(gui.selected);
}

tab_t *gui_set_current_tab(int index)
{
    index %= gui.tabcount;

    if (index < 0)
        index += gui.tabcount;

    gui.selected = index;

    return gui_get_tab(gui.selected);
}

void gui_set_status(tab_t *tab, const char *left, const char *right)
{
    if (!tab)
        tab = gui_get_current_tab();
    if (left)
        strcpy(tab->status[1].left, left);
    if (right)
        strcpy(tab->status[1].right, right);
}

void gui_save_position(bool commit)
{
    tab_t *tab = gui_get_current_tab();
    char key_name[32];
    sprintf(key_name, "Sel.%.11s", tab->name);
    rg_settings_set_app_int32(key_name, tab->listbox.cursor);
    rg_settings_set_app_int32("SelectedTab", gui.selected);
    if (commit) rg_settings_save();
}

listbox_item_t *gui_get_selected_item(tab_t *tab)
{
    listbox_t *list = &tab->listbox;

    if (list->cursor >= 0 && list->cursor < list->length)
        return &list->items[list->cursor];

    return NULL;
}

static int list_comparator(const void *p, const void *q)
{
    return strcasecmp(((listbox_item_t*)p)->text, ((listbox_item_t*)q)->text);
}

void gui_sort_list(tab_t *tab, int sort_mode)
{
    if (tab->listbox.length == 0)
        return;

    qsort((void*)tab->listbox.items, tab->listbox.length, sizeof(listbox_item_t), list_comparator);
}

void gui_resize_list(tab_t *tab, int new_size)
{
    int cur_size = tab->listbox.length;

    if (new_size == cur_size)
        return;

    if (new_size == 0)
    {
        free(tab->listbox.items);
        tab->listbox.items = NULL;
    }
    else
    {
        tab->listbox.items = realloc(tab->listbox.items, new_size * sizeof(listbox_item_t));
        for (int i = cur_size; i < new_size; i++)
            memset(&tab->listbox.items[i], 0, sizeof(listbox_item_t));
    }

    tab->listbox.length = new_size;
    tab->listbox.cursor = RG_MAX(RG_MIN(tab->listbox.cursor, new_size - 1), 0);

    gui_event(TAB_SCROLL, tab);

    RG_LOGI("Resized list '%s' from %d to %d items\n", tab->name, cur_size, new_size);
}

void gui_scroll_list(tab_t *tab, scroll_mode_t mode, int arg)
{
    listbox_t *list = &tab->listbox;

    int cur_cursor = RG_MAX(RG_MIN(list->cursor, list->length - 1), 0);
    int old_cursor = list->cursor;
    int max = LIST_LINE_COUNT - 2;

    if (mode == SCROLL_ABSOLUTE)
    {
        cur_cursor = arg;
    }
    else if (mode == SCROLL_LINE_UP)
    {
        cur_cursor--;
    }
    else if (mode == SCROLL_LINE_DOWN)
    {
        cur_cursor++;
    }
    else if (mode == SCROLL_PAGE_UP)
    {
        char start = list->items[cur_cursor].text[0];
        while (--cur_cursor > 0 && max-- > 0)
        {
           if (start != list->items[cur_cursor].text[0]) break;
        }
    }
    else if (mode == SCROLL_PAGE_DOWN)
    {
        char start = list->items[cur_cursor].text[0];
        while (++cur_cursor < list->length - 1 && max-- > 0)
        {
           if (start != list->items[cur_cursor].text[0]) break;
        }
    }

    if (cur_cursor < 0) cur_cursor = list->length - 1;
    if (cur_cursor >= list->length) cur_cursor = 0;

    list->cursor = cur_cursor;
    gui_event(TAB_SCROLL, tab);

    if (cur_cursor != old_cursor)
    {
        gui_draw_status(tab);
        gui_draw_list(tab);
        gui_event(TAB_REDRAW, tab);
    }
}

void gui_redraw()
{
    tab_t *tab = gui_get_current_tab();
    gui_draw_header(tab);
    gui_draw_status(tab);
    gui_draw_list(tab);
    gui_event(TAB_REDRAW, tab);
}

void gui_draw_navbar()
{
    for (int i = 0; i < gui.tabcount; i++)
    {
        rg_display_write(i * IMAGE_LOGO_WIDTH, 0, IMAGE_LOGO_WIDTH, IMAGE_LOGO_HEIGHT, 0,
            gui.tabs[i]->img_logo->data);
    }
}

void gui_draw_header(tab_t *tab)
{
    int x_pos = IMAGE_LOGO_WIDTH;
    int y_pos = IMAGE_LOGO_HEIGHT;

    rg_gui_draw_fill_rect(x_pos, 0, RG_SCREEN_WIDTH - x_pos, LIST_Y_OFFSET, C_BLACK);
    rg_gui_draw_fill_rect(0, y_pos, RG_SCREEN_WIDTH, LIST_Y_OFFSET - y_pos, C_BLACK);

    if (tab->img_logo)
        rg_gui_draw_image(0, 0, IMAGE_LOGO_WIDTH, IMAGE_LOGO_HEIGHT, tab->img_logo);
    else
        rg_gui_draw_fill_rect(0, 0, IMAGE_LOGO_WIDTH, IMAGE_LOGO_HEIGHT, C_BLACK);

    if (tab->img_header)
        rg_gui_draw_image(x_pos + 1, 0, IMAGE_BANNER_WIDTH, IMAGE_BANNER_HEIGHT, tab->img_header);
    else
        rg_gui_draw_fill_rect(x_pos + 1, 0, IMAGE_BANNER_WIDTH, IMAGE_BANNER_HEIGHT, C_BLACK);
}

void gui_draw_status(tab_t *tab)
{
    int status_x = IMAGE_LOGO_WIDTH + 11;
    int status_y = IMAGE_BANNER_HEIGHT + 1;
    char *txt_left = tab->status[tab->status[1].left[0] ? 1 : 0].left;
    char *txt_right = tab->status[tab->status[1].right[0] ? 1 : 0].right;

    rg_gui_draw_battery(-27, 3);
    rg_gui_draw_text(status_x, status_y, RG_SCREEN_WIDTH, txt_right, C_SNOW, C_BLACK, RG_TEXT_ALIGN_LEFT);
    rg_gui_draw_text(status_x, status_y, 0, txt_left, C_WHITE, C_BLACK, RG_TEXT_ALIGN_RIGHT);
}

void gui_draw_list(tab_t *tab)
{
    const theme_t *theme = &gui_themes[gui.theme % gui_themes_count];
    const listbox_t *list = &tab->listbox;
    char text_label[64];
    uint16_t color_fg = theme->list_standard;
    uint16_t color_bg = theme->list_background;

    int lines = LIST_LINE_COUNT;
    int y = LIST_Y_OFFSET;

    for (int i = 0; i < lines; i++)
    {
        int entry = list->cursor + i - (lines / 2);

        if (entry >= 0 && entry < list->length) {
            sprintf(text_label, "%.63s", list->items[entry].text);
        } else {
            text_label[0] = '\0';
        }

        color_fg = (entry == list->cursor) ? theme->list_selected : theme->list_standard;
        color_bg = (int)(16.f / lines * i) << theme->list_background;

        y += rg_gui_draw_text(LIST_X_OFFSET, y, LIST_WIDTH, text_label, color_fg, color_bg, 0).height;
    }

    if (y < RG_SCREEN_HEIGHT)
        rg_gui_draw_fill_rect(0, y, LIST_WIDTH, RG_SCREEN_HEIGHT - y, color_bg);
}

void gui_draw_preview(tab_t *tab, retro_emulator_file_t *file)
{
    const char *dirname = file->emulator->dirname;
    bool show_missing_cover = false;
    uint32_t order;
    char path[256];

    switch (gui.show_preview)
    {
        case PREVIEW_MODE_COVER_SAVE:
            show_missing_cover = true;
            order = 0x0312;
            break;
        case PREVIEW_MODE_SAVE_COVER:
            show_missing_cover = true;
            order = 0x0123;
            break;
        case PREVIEW_MODE_COVER_ONLY:
            show_missing_cover = true;
            order = 0x0012;
            break;
        case PREVIEW_MODE_SAVE_ONLY:
            show_missing_cover = false;
            order = 0x0003;
            break;
        default:
            show_missing_cover = false;
            order = 0x0000;
    }

    rg_image_t *img = NULL;
    uint32_t errors = 0;

    while (order && !img)
    {
        int type = order & 0xF;

        order >>= 4;

        if (file->missing_cover & (1 << type))
        {
            continue;
        }

        gui.joystick = rg_input_read_gamepad();

        if (type == 0x1 || type == 0x2)
        {
            emulator_crc32_file(file);
        }

        if (gui.joystick & GAMEPAD_KEY_ANY)
        {
            break;
        }

        if (type == 0x1) // Game cover (old format)
            sprintf(path, RG_BASE_PATH_ROMART "/%s/%X/%08X.art", dirname, file->checksum >> 28, file->checksum);
        else if (type == 0x2) // Game cover (png)
            sprintf(path, RG_BASE_PATH_ROMART "/%s/%X/%08X.png", dirname, file->checksum >> 28, file->checksum);
        else if (type == 0x3) // Save state screenshot (png)
            sprintf(path, "%s/%s/%s.%s.png", RG_BASE_PATH_SAVES, dirname, file->name, file->ext);
        else if (type == 0x4) // use default image (not currently used)
            sprintf(path, RG_BASE_PATH_ROMART "/%s/default.png", dirname);
        else
            continue;

        if (rg_vfs_filesize(path) > 0)
        {
            img = rg_image_load_from_file(path, 0);
            if (!img)
                errors++;
        }

        file->missing_cover |= (img ? 0 : 1) << type;
    }

    if (img)
    {
        int height = RG_MIN(img->height, COVER_MAX_HEIGHT);
        int width = RG_MIN(img->width, COVER_MAX_WIDTH);

        rg_gui_draw_image(-width, -height, width, height, img);
        rg_image_free(img);
    }
    else if (file->checksum && (show_missing_cover || errors))
    {
        RG_LOGI("No image found for '%s'\n", file->name);
        gui_set_status(tab, NULL, errors ? "Bad cover" : "No cover");
        gui_draw_status(tab);
    }
}
