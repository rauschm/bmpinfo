/****************************************************************************/
/*  B M P I N F O . C                                                       */
/*                                                                          */
/*  Das Programm gibt alle Informationen über eine BMP-Datei in Text-Form   */
/*  aus. Es fehlen lediglich die nicht verwendeten Paletteneinträge und     */
/*  die 2 Anwendungs-abhängigen Werte aus dem Header. Nicht implementiert   */
/*  sind Komprimierungsverfahen. Das Programm bricht sofort ab, wenn es     */
/*  inkorrekte Daten erkennt. In diesem Fall wird ein Hexdump der Datei     */
/*  ausgegeben (maximal 4000 Bytes). Bei nicht kritischen Verstößen wird    */
/*  dagegen lediglich eine Meldung ausgegeben.                              */
/*                                                                          */
/*  Aufruf: bmpinfo Datei                                                   */
/****************************************************************************/

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct
{
  FILE* f;
  unsigned char* buffer;
} tidy_info = { NULL, NULL };

#define TIDY_INFO(x) do { tidy_info.x = x; } while (0)

typedef struct { unsigned int B:8, G:8, R:8; } BGR;
typedef struct { unsigned int B:8, G:8, R:8, A:8; } BGRA;

#pragma pack(push,1)
typedef struct BmpFile {
  struct {
    char          BM[2];
    unsigned int  file_size;               /* unit: Bytes */
    char          application_value_1[2];  /* = 0 */
    char          application_value_2[2];  /* = 0 */
    unsigned int  bitmap_offset;           /* unit: Bytes = 54/62/70/118/1078 */
  } header;

  struct {
    unsigned int   dib_header_size;        /* unit: Bytes = 40 */
    signed int     image_width;            /* unit = Pixel */
    signed int     image_height;           /* unit = Pixel, >0: flip image */
    unsigned short color_planes;           /* := 1 */
    unsigned short bits_per_pixel;         /* = 1, 2, 4, 8, 24 für RGB */
    unsigned int   compression_method;     /* = 0, d.h. keine Kompression */
    unsigned int   bitmap_raw_size;        /* unit: bytes = (width * bpp +31)/32*4 * |height| */
    signed int     horizontal_resolution;  /* = width im Druck, z.B. 0x2E23 für 300 DPI */
    signed int     vertical_resolution;    /* = height im Druck, z.B. 11811 für 300 DPI */
    unsigned int   number_of_colors;       /* = 0, d. h. default = 2^bpp */
    unsigned int   number_of_important_colors;  /* = 0, d. h. jede Farbe ist wichtig */
  } dib_header;

  union {
    struct bpp_1 {
      BGRA colormap[2];
      struct {
        unsigned int ix00:1, ix01:1, ix02:1, ix03:1, ix04:1, ix05:1, ix06:1, ix07:1,
                     ix08:1, ix09:1, ix10:1, ix11:1, ix12:1, ix13:1, ix14:1, ix15:1,
                     ix16:1, ix17:1, ix18:1, ix19:1, ix20:1, ix21:1, ix22:1, ix23:1,
                     ix24:1, ix25:1, ix26:1, ix27:1, ix28:1, ix29:1, ix30:1, ix31:1;
      } bitmap[1];
    } b1;
    struct bpp_2 {
      BGRA colormap[4];
      struct {
        unsigned int ix00:2, ix01:2, ix02:2, ix03:2, ix04:2, ix05:2, ix06:2, ix07:2,
                     ix08:2, ix09:2, ix10:2, ix11:2, ix12:2, ix13:2, ix14:2, ix15:2;
      } bitmap[1];
    } b2;
    struct bpp_4 {
      BGRA colormap[16];
      struct {
        unsigned int ix00:4, ix01:4, ix02:4, ix03:4, ix04:4, ix05:4, ix06:4, ix07:4;
      } bitmap[1];
    } b4;
    struct bpp_8 {
      BGRA colormap[256];
      struct {
        unsigned int ix00:8, ix01:8, ix02:8, ix03:8;
      } bitmap[1];
    } b8;
    struct bpp_24 {
      BGR bitmap[4];
    } b24;
  };
} BmpFile;
#pragma pack(pop)

typedef struct FileInfo
{
  unsigned int   fileSize;
  unsigned char* fileData;
} FileInfo;


#include "bmpinfo_proto.h"


void main(int argc, char* argv[]) {
  if (argc != 2)
    exitWithErrorMessage("Gibt Informationen über eine BMP-Datei aus\n"
                         "Aufruf: bmpinfo Datei\n");

  FileInfo bmpFileInfo = readBmpFile(argv[1]);
  if (printBmpInfos((BmpFile*) bmpFileInfo.fileData, bmpFileInfo.fileSize))
  {
    dumpBuffer(0, bmpFileInfo.fileData,
               bmpFileInfo.fileSize < 4000 ? bmpFileInfo.fileSize : 4000);
  }
  free(bmpFileInfo.fileData);
}


int printBmpInfos(BmpFile* b, unsigned int fileSize)
{
  unsigned int uist, usoll;

  if (b->header.BM[0] != 'B' || b->header.BM[1] != 'M')
  {
    fprintf(stderr, "file corrupt, Magic != BM: %02x%02x\n",
            b->header.BM[0], b->header.BM[1]);
    return 1;
  }
  printf("Magic: %32s\n", "BM");

  if ((uist = b->header.file_size) != (usoll = fileSize))
  {
    fprintf(stderr, "file corrupt, internal size != %u: %u\n",
            usoll, uist);
    return 1;
  }
  printf("File Size: %28u\n", uist);

  if (   (uist = b->dib_header.bits_per_pixel) != 1
      && uist != 2 && uist != 4 && uist != 8 && uist != 24)
  {
    fprintf(stderr, "file corrupt, incorrect bpp != %s: %u \n",
            "(1,2,4,8,24)", uist);
    return 1;
  }
  printf("Bits per Pixel: %23u\n", uist);

  if ((uist = b->dib_header.number_of_colors) >
      (usoll = (1 << b->dib_header.bits_per_pixel) & 0x1ff))
  {
    fprintf(stderr, "file corrupt, number of palette colors too large > %u: %u\n",
            uist, usoll);
    return 1;
  }

  if ((uist = b->header.bitmap_offset) > (usoll = fileSize))
  {
    /* der Wert liegt hinter dem Dateiende */
    fprintf(stderr, "file corrupt, incorrect bitmap offset > %u: %u\n",
            usoll, uist);
    return 1;
  }
  if ((uist = b->header.bitmap_offset) <
      (usoll = 54 + (  b->dib_header.bits_per_pixel == 24 ? 0
      : b->dib_header.number_of_colors != 0 ? b->dib_header.number_of_colors * 4
      : (1 << b->dib_header.bits_per_pixel) * 4)))
  {
    /* der Wert liegt innerhalb der Header- oder Paletten-Daten */
    fprintf(stderr, "file corrupt, incorrect bitmap offset < %u: %u\n",
            usoll, uist);
    return 1;
  }
  if(uist > usoll)
  {
    fprintf(stderr, "INFO: bitmap offset larger than necessary > %u: %u\n",
            usoll, uist);
  }
  printf("Bitmap Offset: %24u\n", uist);

  if ((uist = b->dib_header.bitmap_raw_size) >
      (usoll = fileSize - b->header.bitmap_offset))
  {
    /* die Daten passen nicht mehr zwischen Offset und Dateiende */
    fprintf(stderr, "file corrupt, incorrect bitmap size > %u: %u\n",
            usoll, uist);
    return 1;
  }
  if (uist < usoll)
  {
    fprintf(stderr, "INFO: file contains bytes after bitmap: %u\n",
            usoll - uist);
  }
  printf("Bitmap Size (raw): %20u\n", uist);

  if (((b->dib_header.image_width * b->dib_header.bits_per_pixel + 31) / 32) * 4
      * abs(b->dib_header.image_height) != b->dib_header.bitmap_raw_size)
  {
    fprintf(stderr, "file corrupt, image width * image height doesn't "
            "fit bitmap size %u: %u * %u\n",
            b->dib_header.bitmap_raw_size,
            b->dib_header.image_width, b->dib_header.image_height);
    return 1;
  }
  printf("Image Width: %26d\n", b->dib_header.image_width);
  printf("Image Height: %9s %15u\n",
         b->dib_header.image_height > 0 ? "(flipped)" :"",
         abs(b->dib_header.image_height));

  if (b->dib_header.color_planes != 1)
  {
    fprintf(stderr, "file corrupt, color planes != 1: %d\n",
            b->dib_header.color_planes);
    return 1;
  }
  printf("Color Planes: %25u\n", 1);

  if (b->dib_header.compression_method != 0)
  {
    fprintf(stderr, "sorry, compression not yet supported by this tool\n");
    return 1;
  }
  printf("Compression: %26s\n", "none");

  printf("Horizontal Resolution: %16d\n", b->dib_header.horizontal_resolution);
  printf("Vertical Resolution: %18d\n", b->dib_header.vertical_resolution);

  /* oben schon geprüft! */
  if (b->dib_header.number_of_colors == 0 && b->dib_header.bits_per_pixel != 24)
  {
    printf("Number of Palette Colors: %9s %3u\n", "0 =", 1 << b->dib_header.bits_per_pixel);
  }
  else
  {
    printf("Number of Palette Colors: %13u\n", b->dib_header.number_of_colors);
  }
  if ((uist = b->dib_header.number_of_important_colors) >
      (usoll = b->dib_header.number_of_colors))
  {
    fprintf(stderr, "file corrupt, incorrect number of important colors > %u: %u\n",
            usoll, uist);
    return 1;
  }
  printf("Number of Important Palette Colors: %3u\n", uist);

  printf("Bitmap Data (index:Blue Green Red):\n");

  int bytes_per_line = ((b->dib_header.image_width * b->dib_header.bits_per_pixel + 31) / 32) * 4;
  unsigned char* y_ptr = ((unsigned char *) b)
                       + b->header.bitmap_offset
                       + (b->dib_header.image_height < 0 ? 0
                          : (abs(b->dib_header.image_height) - 1) * bytes_per_line);
  int delta_y = b->dib_header.image_height < 0 ? bytes_per_line : -bytes_per_line;
  unsigned int pixel_mask = (1 << b->dib_header.bits_per_pixel) - 1;
  unsigned int index_digits = b->dib_header.bits_per_pixel <= 2 ? 1
                            : b->dib_header.bits_per_pixel == 4 ? 2 : 3;

  for (int y = abs(b->dib_header.image_height); y > 0; --y)
  {
    if (b->dib_header.bits_per_pixel == 24)
    {
      BGR* x_ptr = (BGR*) y_ptr;
      for (int x = b->dib_header.image_width; x > 0; --x)
      {
        printf("%02x%02x%02x%c", x_ptr->B, x_ptr->G, x_ptr->R,
                                 (x > 1 ? '-' : '\n'));
        * (unsigned char**) &x_ptr += 3;
      }
    }
    else
    {
      unsigned char* x_ptr = y_ptr;
      int pixels_left = 8;
      for (int x = b->dib_header.image_width; x > 0; --x)
      {
        pixels_left -= b->dib_header.bits_per_pixel;
        unsigned int cx = (*x_ptr >> pixels_left) & pixel_mask;
        printf("%0*u:%02x%02x%02x%c", index_digits, cx,
                                      b->b8.colormap[cx].B,
                                      b->b8.colormap[cx].G,
                                      b->b8.colormap[cx].R,
                                      (x > 1 ? '-' : '\n'));

        if (pixels_left == 0)
        {
          pixels_left = 8;
          x_ptr += 1;
        }
      }
    }
    y_ptr += delta_y;
  }
  return 0;
}


FileInfo readBmpFile(char* fileName)
{
  FILE* f = openFile(fileName);
  FileInfo bmpFileInfo = readFile(f);
  closeFile(f);
  return bmpFileInfo;
}


FILE* openFile(char* fileName)
{
  FILE* f;

  if ((f = fopen(fileName, "rb")) == NULL)
    exitWithErrorMessage("open error %s: %s\n", fileName, strerror(errno));
  TIDY_INFO(f);
  return f;
}


void closeFile(FILE* f)
{
  fclose(f);
  f = NULL;
  TIDY_INFO(f);
}


FileInfo readFile(FILE* f)
{
  FileInfo fi = { getFileSize(f), allocBuffer(fi.fileSize) };
  if (fread(fi.fileData, 1, fi.fileSize, f) != fi.fileSize)
    exitWithErrorMessage("read error: %s\n", strerror(errno));
  return fi;
}


unsigned int getFileSize(FILE* f)
{
  fseek(f, 0L, SEEK_END);
  unsigned int fileSize = ftell(f);
  fseek(f, 0L, SEEK_SET);
  return fileSize;
}


unsigned char* allocBuffer(unsigned int bufferSize)
{
  unsigned char* buffer = malloc(bufferSize);
  if (buffer == NULL)
    exitWithErrorMessage("malloc error: %s", strerror(errno));
  TIDY_INFO(buffer);
  return buffer;
}


void exitWithErrorMessage(char* format, ...)
{
  va_list args;

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  tidy_up();
  exit(1);
}


void tidy_up(void)
{
  if (tidy_info.f != NULL)
    fclose(tidy_info.f);
  if (tidy_info.buffer != NULL)
    free(tidy_info.buffer);
}


void dumpBuffer(int fileOffset, unsigned char* buffer, int bufferLength)
{
  for (int i = 0; i < bufferLength; i += 16, fileOffset += 16)
    dumpLine(fileOffset, &buffer[i], bufferLength - i);
}


void dumpLine(int fileOffset, unsigned char* buffer, int availableCharsInBuffer)
{
  char hexArea[49];
  char asciiArea[17];

  if (availableCharsInBuffer > 16)
    availableCharsInBuffer = 16;
  for (int i = 0; i < availableCharsInBuffer; i++)
  {
    dumpCharAsHex(&hexArea[3 * i], buffer[i]);
    dumpCharAsAscii(&asciiArea[i], buffer[i]);
  }
  hexArea[3 * availableCharsInBuffer] = '\0';
  asciiArea[availableCharsInBuffer] = '\0';
  printf("%08x:%-48s  %s\n", fileOffset, hexArea, asciiArea);
}


void dumpCharAsHex(char* hexArea, unsigned char c)
{
  hexArea[0] = ' ';
  hexArea[1] = c / 16 + ((c / 16 < 10) ? '0' : 'A' - 10);
  hexArea[2] = c % 16 + ((c % 16 < 10) ? '0' : 'A' - 10);
}


void dumpCharAsAscii(char* asciiArea, unsigned char c)
{
  asciiArea[0] = (c < 32 || c >= 127) ? '.' : c;
}
