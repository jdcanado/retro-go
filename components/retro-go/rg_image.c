#include <stdio.h>
#include <lupng.h>
// #include <gifdec.h>
// #include <gifenc.h>

#include "rg_system.h"
#include "rg_image.h"


rg_image_t *rg_image_load_from_file(const char *filename, uint32_t flags)
{
    if (!filename)
        return NULL;

    FILE *fp = fopen(filename, "rb");
    if (fp)
    {
        fseek(fp, 0, SEEK_END);

        size_t data_len = RG_MIN(0x80000, ftell(fp));
        void *data = malloc(data_len);
        if (!data)
        {
            RG_LOGE("Unable to load image file '%s' (out of memory)!\n", filename);
            fclose(fp);
            return NULL;
        }

        fseek(fp, 0, SEEK_SET);
        fread(data, data_len, 1, fp);
        fclose(fp);

        rg_image_t *img = rg_image_load_from_memory(data, data_len, flags);
        free(data);

        return img;
    }

    RG_LOGE("Unable to load image file '%s'!\n", filename);
    return NULL;
}

rg_image_t *rg_image_load_from_memory(const uint8_t *data, size_t data_len, uint32_t flags)
{
    if (!data || data_len < 16)
        return NULL;

    if (memcmp(data, "\x89PNG", 4) == 0)
    {
        LuImage *png = luPngReadMem(data, data_len);
        if (!png)
        {
            RG_LOGE("PNG parsing failed!\n");
            return NULL;
        }

        rg_image_t *img = rg_image_alloc(png->width, png->height);
        if (img)
        {
            uint16_t *ptr = img->data;

            for (int y = 0; y < img->height; ++y)
            {
                for (int x = 0; x < img->width; ++x)
                {
                    int offset = (y * png->width * 3) + (x * 3);
                    int r = (png->data[offset+0] >> 3) & 0x1F;
                    int g = (png->data[offset+1] >> 2) & 0x3F;
                    int b = (png->data[offset+2] >> 3) & 0x1F;
                    *(ptr++) = (r << 11) | (g << 5) | b;
                }
            }
        }
        luImageRelease(png, NULL);
        return img;
    }
    else if (memcmp(data, "GIF89a", 6) == 0)
    {
    #if 0
        gd_GIF *gif = gd_open_gif("some_animation.gif");
        if (!gif)
        {
            RG_LOGE("GIF parsing failed!\n");
            return NULL;
        }

        rg_image_t *img = rg_image_alloc(gif->width, gif->height);
        if (img)
        {
            if (gd_get_frame(gif))
            {
                gd_render_frame(gif, buffer);
                /* insert code to render buffer to screen  */
            }
            free(buffer);
        }
        gd_close_gif(gif);
        return img;
    #endif
    }
    else // RAW565 (uint16 width, uint16 height, uint16 data[])
    {
        int img_width = ((uint16_t *)data)[0];
        int img_height = ((uint16_t *)data)[1];
        int size_diff = (img_width * img_height * 2 + 4) - data_len;

        // This might seem weird, but we need to account for some RAW565 being padded...
        if (size_diff >= 0 && size_diff <= 100)
        {
            rg_image_t *img = rg_image_alloc(img_width, img_height);
            if (img)
            {
                memcpy(img->data, data + 4, data_len - 4);
            }
            // else Maybe we could just return (rg_image_t *)buffer; ?
            return img;
        }
    }

    RG_LOGE("Image format not recognized!\n");
    return NULL;
}

bool rg_image_save_to_file(const char *filename, const rg_image_t *img, uint32_t flags)
{
    LuImage *png = luImageCreate(img->width, img->height, 3, 8, 0, 0);
    if (!png)
    {
        RG_LOGE("LuImage allocation failed!\n");
        return false;
    }

    uint8_t *img_ptr = png->data;

    for (int y = 0; y < img->height; y++)
    {
        for (int x = 0; x < img->width; x++)
        {
            uint32_t pixel = img->data[x * img->width + y];
            *(img_ptr++) = ((pixel >> 11) & 0x1F) << 3;
            *(img_ptr++) = ((pixel >> 5) & 0x3F) << 2;
            *(img_ptr++) = (pixel & 0x1F) << 3;
        }
    }

    if (luPngWriteFile(filename, png))
    {
        luImageRelease(png, 0);
        return true;
    }

    luImageRelease(png, 0);
    return false;
}

rg_image_t *rg_image_alloc(size_t width, size_t height)
{
    rg_image_t *img = malloc(sizeof(rg_image_t) + width * height * 2);
    if (!img)
    {
        RG_LOGE("Image alloc failed (%dx%d)\n", width, height);
        return NULL;
    }
    img->width = width;
    img->height = height;
    return img;
}

void rg_image_free(rg_image_t *img)
{
    free(img);
}
