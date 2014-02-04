#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* my god PNG is stupid. */
#define PNG_USER_MEM_SUPPORTED 1
#include <png.h>

bool writepng(const char* filename, const uint16_t* buf,
              uint32_t width, uint32_t height)
{
  png_structp png=NULL;
  png_infop info=NULL;

  FILE* fp = fopen(filename, "w+b");
  if(!fp) {
    perror("err");
    return false;
  }

  png = png_create_write_struct_2(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL,
                                  NULL, NULL, NULL);
  if(png == NULL) {
    fclose(fp);
    return false;
  }

  info = png_create_info_struct(png);
  if(info == NULL) {
    fclose(fp);
    png_destroy_write_struct(&png, (png_infopp)NULL);
    return false;
  }

  /* png error handling *nonsense* */
  if(setjmp(png_jmpbuf(png))) {
    fclose(fp);
    png_destroy_write_struct(&png, &info);
    return false;
  }

  png_init_io(png, fp);
  png_set_IHDR(png, info, width, height, 16, PNG_COLOR_TYPE_GRAY,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);
  png_write_info(png, info);

  for(uint32_t y=0; y < height; ++y) {
    png_write_row(png, (png_bytep) (buf + y*width));
  }
  png_write_end(png, NULL);

  fclose(fp);
  png_destroy_write_struct(&png, &info);
  return true;
}

bool readpng(const char* filename, uint8_t** buf,
             uint32_t* width, uint32_t* height) {
  FILE* fp = fopen(filename, "rb");
  if(!fp) {
    perror("opening file");
    return false;
  }
  const size_t header_bytes = 7;
  uint8_t header[header_bytes];
  if(fread(header, 1, header_bytes, fp) != header_bytes) {
    perror("reading header");
    fclose(fp);
    return false;
  }
  if(png_sig_cmp(header, 0, header_bytes) != 0) {
    fprintf(stderr, "'%s' is not a png file.\n", filename);
    fclose(fp);
    return false;
  }

  png_structp png = png_create_read_struct(
    PNG_LIBPNG_VER_STRING, NULL, NULL, NULL
  );
  if(!png) {
    fprintf(stderr, "could not create read struct.\n");
    fclose(fp);
    return false;
  }

  png_infop info = png_create_info_struct(png);
  if(!info) {
    fprintf(stderr, "could not create info struct.\n");
    png_destroy_read_struct(&png, NULL, NULL);
    fclose(fp);
    return false;
  }

  png_infop endinfo = png_create_info_struct(png);
  if(!endinfo) {
    fprintf(stderr, "could not create info struct.\n");
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return false;
  }

  if(setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, &endinfo);
    fclose(fp);
    return false;
  }

  png_init_io(png, fp);
  png_set_sig_bytes(png, header_bytes);
  png_read_info(png, info);

  png_uint_32 w, h;
  int depth, ctype;
  png_get_IHDR(png, info, &w, &h, &depth, &ctype, NULL, NULL, NULL);

  if(ctype != PNG_COLOR_TYPE_GRAY) {
    fprintf(stderr, "this function only handles grayscale images\n");
    png_destroy_read_struct(&png, &info, &endinfo);
    fclose(fp);
  }

  uint8_t bgcolor=0;
  if(png_get_valid(png, info, PNG_INFO_bKGD)) {
    png_color_16p bg;
    png_get_bKGD(png, info, &bg);
    if(ctype == PNG_COLOR_TYPE_GRAY && depth == 8) {
      bgcolor = bg->gray;
    }
  }
  *width = (uint32_t) w;
  *height = (uint32_t) h;

  if(depth < 8) {
    /* if the depth is less than 8 bits, have libpng expand it. */
    png_set_expand(png);
  } else if(depth == 16) {
    /* likewise, if it's 16 bit, have libpng quantize it.  Probably not the
     * best idea, but... eh.  It'll work for now. */
    fprintf(stderr, "[WARNING] quantizing 16bit -> 8bit\n");
    png_set_strip_16(png);
  }

  png_read_update_info(png, info);
  png_uint_32 rowbytes;
  rowbytes = png_get_rowbytes(png, info);
  if(rowbytes != w) {
    fprintf(stderr, "Rowbytes and width not equal..\n");
    png_destroy_read_struct(&png, &info, &endinfo);
    fclose(fp);
    return false;
  }

  /* allocate buffer and fill it with the background color. */
  *buf = malloc(sizeof(uint8_t) * w * h);
  memset(*buf, (int)bgcolor, rowbytes*h);

  if(((unsigned)(png_get_channels(png, info))) != 1) {
    fprintf(stderr, "too many channels, this should be grayscale!\n");
    free(*buf);
    png_destroy_read_struct(&png, &info, &endinfo);
    fclose(fp);
    return false;
  }

  /* setup PNGs stupid row pointers (it can't read into one large array) */
  png_bytep* row_pointers = calloc(h, sizeof(png_bytep));
  for(png_uint_32 i=0; i < h; ++i) {
    row_pointers[i] = (png_bytep)((*buf) + i * rowbytes);
  }
  png_read_image(png, row_pointers);
  png_read_end(png, NULL);
  free(row_pointers);
  png_destroy_read_struct(&png, &info, &endinfo);
  fclose(fp);
  return true;
}
