/*
 * AGRESS - Прогрессивный аудио кодер
 *
 * Данная программа является свободным программным обеспечением.
 * Вы вправе распространять ее и/или модифицировать в соответствии
 * с условиями версии 2 либо по вашему выбору с условиями более
 * поздней версии Стандартной Общественной Лицензии GNU,
 * опубликованной Free Software Foundation.
 *
 * Copyleft (С) 2004 Александр Симаков
 *
 * http://www.entropyware.info
 * xander@entropyware.info
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <agress.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <popt.h>
#include <glib.h>
#include <time.h>

#define RIFF 0x46464952
#define WAVE 0x45564157
#define FMT  0x20746d66
#define DATA 0x61746164

#define MAGIC 0x4741

#define ENCODE 1
#define DECODE 2

#define MONO    1
#define STEREO  2
#define JSTEREO 3

#define PACKED __attribute__ ((packed))

typedef struct wave_header_tag
{
  guint32 id_riff PACKED;
  guint32 len_riff PACKED;
  guint32 id_chuck PACKED;
  guint32 fmt PACKED;
  guint32 len_chuck PACKED;
  guint16 type PACKED;
  guint16 channels PACKED;
  guint32 freq PACKED;
  guint32 bytes PACKED;
  guint16 align PACKED;
  guint16 bits PACKED;
  guint32 id_data PACKED;
  guint32 len_data PACKED;
} wave_header;

typedef struct agress_header_tag
{
  guint16 magic PACKED;
  guint16 freq PACKED;
  guint16 frame PACKED;
  guint8 channels PACKED;
  guint8 bits PACKED;
} agress_header;

poptContext ctx;

gint encode = -1;
gchar *input = NULL;
gchar *output = NULL;
gdouble ratio = 4.0;
gint frame = 1024;
gint mode = JSTEREO;
gint smooth = 5;
gdouble ms_ratio = 70.0;

wave_header w_hdr;
agress_header a_hdr;

FILE *wav;
FILE *agress;

void print_help ();
gint file_size (FILE *f);
void parse_options (int argc, char **argv);

void encode_m8 ();
void encode_m16 ();
void encode_s8m ();
void encode_s8s ();
void encode_s16m ();
void encode_s16s ();
void encode_s16j ();

void decode_8m ();
void decode_8s ();
void decode_8j ();
void decode_16m ();
void decode_16s ();
void decode_16j ();

void
print_help ()
{
  poptPrintHelp (ctx, stderr, 0);
  exit (1);
}

gint
file_size (FILE *f)
{
  gint save_pos, size_of_file;

  save_pos = ftell (f);
  fseek (f, 0, SEEK_END);
  size_of_file = ftell (f);
  fseek (f, save_pos, SEEK_SET);

  return size_of_file;
}

void
parse_options (int argc, char **argv)
{
  gint msf, lsf;
  gint rc;

  struct poptOption options[] = {
    {"encode", 'e', POPT_ARG_VAL, &encode, ENCODE,
     "Encode file", NULL},
    {"decode", 'd', POPT_ARG_VAL, &encode, DECODE,
     "Decode file", NULL},
    {"input", 'i', POPT_ARG_STRING, &input, 0,
     "Input file", "PATHNAME"},
    {"output", 'o', POPT_ARG_STRING, &output, 0,
     "Output file", "PATHNAME"},
    {"ratio", 'r', POPT_ARG_DOUBLE, &ratio, 0,
     "Compression ratio", "NUMBER"},
    {"frame", 'f', POPT_ARG_INT, &frame, 0,
     "Frame size (power of 2)", "NUMBER"},
    {"smooth", 'h', POPT_ARG_INT, &smooth, 0,
     "Smooth factor", "NUMBER"},
    {"mono", 'm', POPT_ARG_VAL, &mode, MONO,
     "Downsample to mono", NULL},
    {"stereo", 's', POPT_ARG_VAL, &mode, STEREO,
     "Full stereo", NULL},
    {"jstereo", 'j', POPT_ARG_VAL, &mode, JSTEREO,
     "Joint stereo", NULL},
    {"mid-side-ratio", 'R', POPT_ARG_DOUBLE, &ms_ratio, 0,
     "Mid-side percent ratio", "NUMBER"},
    POPT_AUTOHELP POPT_TABLEEND
  };

  ctx = poptGetContext (NULL, argc, (const char **) argv, options, 0);
  rc = poptGetNextOpt (ctx);

  if (rc < -1)
    {
      fprintf (stderr, "%s: %s\n",
               poptBadOption (ctx, POPT_BADOPTION_NOALIAS),
               poptStrerror (rc));
      exit (1);
    }

  if ((ms_ratio <= 1.0) || (ms_ratio >= 99.0))
    print_help ();

  smooth = CLAMP (smooth, 1, frame);

  msf = g_bit_nth_msf (frame, -1);
  lsf = g_bit_nth_lsf (frame, -1);

  if ((frame < 2) || (msf != lsf))
    print_help ();

  if (ratio < 1.0)
    print_help ();

  if ((input == NULL) || (output == NULL))
    print_help ();

  if (encode == -1)
    print_help ();
}

void
encode_m8 ()
{
  guint8 *in_buf;
  guint8 *out_buf;
  gint in_frame_size;
  gint out_frame_size;
  gint bytes_read;
  guint16 real_size;
  gint i;

  a_hdr.magic = MAGIC;
  a_hdr.freq = w_hdr.freq;
  a_hdr.channels = MONO;
  a_hdr.bits = 8;
  a_hdr.frame = frame;

  if (fwrite (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: i/o error\n", output);
      exit (1);
    }

  in_frame_size = frame * sizeof (guint8);
  out_frame_size = CLAMP (frame / ratio - 2, 2, G_MAXUINT16);

  in_buf = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf = (guint8 *) g_malloc (out_frame_size * sizeof (guint8));

  while ((bytes_read = fread (in_buf, 1, in_frame_size, wav)) > 0)
    {
      if (bytes_read < in_frame_size)
        memset ((guint8 *) in_buf + bytes_read, 0, in_frame_size - bytes_read);

      real_size = encode_frame (in_buf, in_frame_size,
                                out_buf, out_frame_size,
                                FMT_8, FMT_LE, FMT_U);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);
    }
}

void
encode_m16 ()
{
  gint16 *in_buf;
  guint8 *out_buf;
  gint in_frame_size;
  gint out_frame_size;
  gint bytes_read;
  guint16 real_size;
  gint i;

  a_hdr.magic = MAGIC;
  a_hdr.freq = w_hdr.freq;
  a_hdr.channels = MONO;
  a_hdr.bits = 16;
  a_hdr.frame = frame;

  if (fwrite (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: i/o error\n", output);
      exit (1);
    }

  in_frame_size = frame * sizeof (gint16);
  out_frame_size = CLAMP (2.0 * frame / ratio - 2, 2, G_MAXUINT16);

  in_buf = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf = (guint8 *) g_malloc (out_frame_size * sizeof (guint8));

  while ((bytes_read = fread (in_buf, 1, in_frame_size, wav)) > 0)
    {
      if (bytes_read < in_frame_size)
        memset ((guint8 *) in_buf + bytes_read, 0, in_frame_size - bytes_read);

      real_size = encode_frame (in_buf, in_frame_size,
                                out_buf, out_frame_size,
                                FMT_16, FMT_LE, FMT_S);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);
    }
}

void
encode_s8m ()
{
  guint8 *in_buf;
  guint8 *in_left;
  guint8 *in_right;
  guint8 *out_buf;
  gint in_frame_size;
  gint out_frame_size;
  gint bytes_read;
  guint16 real_size;
  gint i;

  a_hdr.magic = MAGIC;
  a_hdr.freq = w_hdr.freq;
  a_hdr.channels = MONO;
  a_hdr.bits = 8;
  a_hdr.frame = frame;

  if (fwrite (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: i/o error\n", output);
      exit (1);
    }

  in_frame_size = 2 * frame * sizeof (guint8);
  out_frame_size = CLAMP (2.0 * frame / ratio - 2, 2, G_MAXUINT16);

  in_buf = (guint8 *) g_malloc (2 * frame * sizeof (guint8));
  in_left = (guint8 *) g_malloc (frame * sizeof (guint8));
  in_right = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf = (guint8 *) g_malloc (out_frame_size * sizeof (guint8));

  while ((bytes_read = fread (in_buf, 1, in_frame_size, wav)) > 0)
    {
      if (bytes_read < in_frame_size)
        memset ((guint8 *) in_buf + bytes_read, 0, in_frame_size - bytes_read);

      for (i = 0; i < frame; i++)
        {
          in_left[i] = in_buf[2 * i];
          in_right[i] = in_buf[2 * i + 1];
        }

      for (i = 0; i < frame; i++)
        in_buf[i] = (in_left[i] + in_right[i]) / 2;

      real_size = encode_frame (in_buf, in_frame_size / 2,
                                out_buf, out_frame_size,
                                FMT_8, FMT_LE, FMT_U);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);
    }
}

void
encode_s8s ()
{
  guint8 *in_buf;
  guint8 *in_left;
  guint8 *in_right;
  guint8 *out_buf;
  gint in_frame_size;
  gint out_frame_size;
  gint bytes_read;
  guint16 real_size;
  gint i;

  a_hdr.magic = MAGIC;
  a_hdr.freq = w_hdr.freq;
  a_hdr.channels = STEREO;
  a_hdr.bits = 8;
  a_hdr.frame = frame;

  if (fwrite (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: i/o error\n", output);
      exit (1);
    }

  in_frame_size = 2 * frame * sizeof (guint8);
  out_frame_size = CLAMP (frame / ratio - 2, 2, G_MAXUINT16);

  in_buf = (guint8 *) g_malloc (2 * frame * sizeof (guint8));
  in_left = (guint8 *) g_malloc (frame * sizeof (guint8));
  in_right = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf = (guint8 *) g_malloc (out_frame_size * sizeof (guint8));

  while ((bytes_read = fread (in_buf, 1, in_frame_size, wav)) > 0)
    {
      if (bytes_read < in_frame_size)
        memset ((guint8 *) in_buf + bytes_read, 0, in_frame_size - bytes_read);

      for (i = 0; i < frame; i++)
        {
          in_left[i] = in_buf[2 * i];
          in_right[i] = in_buf[2 * i + 1];
        }

      real_size = encode_frame (in_left, in_frame_size / 2,
                                out_buf, out_frame_size,
                                FMT_8, FMT_LE, FMT_U);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);

      real_size = encode_frame (in_right, in_frame_size / 2,
                                out_buf, out_frame_size,
                                FMT_8, FMT_LE, FMT_U);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);
    }
}

void
encode_s8j ()
{
  guint8 *in_buf;
  guint8 *in_left;
  guint8 *in_right;
  guint8 *out_buf;
  gint in_frame_size;
  gint mid_frame_size;
  gint side_frame_size;
  gint bytes_read;
  guint16 real_size;
  gint i;

  a_hdr.magic = MAGIC;
  a_hdr.freq = w_hdr.freq;
  a_hdr.channels = JSTEREO;
  a_hdr.bits = 8;
  a_hdr.frame = frame;

  if (fwrite (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: i/o error\n", output);
      exit (1);
    }

  in_frame_size = 2 * frame * sizeof (guint8);
  mid_frame_size = CLAMP (2.0 * frame / ratio * ms_ratio / 100.0 - 2,
                          2, G_MAXUINT16);
  side_frame_size = CLAMP (2.0 * frame / ratio - mid_frame_size - 2,
                           2, G_MAXUINT16);

  in_buf = (guint8 *) g_malloc (2 * frame * sizeof (guint8));
  in_left = (guint8 *) g_malloc (frame * sizeof (guint8));
  in_right = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf = (guint8 *) g_malloc (MAX (mid_frame_size, side_frame_size)
                                 * sizeof (guint8));

  while ((bytes_read = fread (in_buf, 1, in_frame_size, wav)) > 0)
    {
      if (bytes_read < in_frame_size)
        memset ((guint8 *) in_buf + bytes_read, 0, in_frame_size - bytes_read);

      for (i = 0; i < frame; i++)
        {
          in_left[i]  = CLAMP ((in_buf[2 * i] + in_buf[2 * i + 1]) / 2,
                               0, G_MAXUINT8);
          in_right[i] = CLAMP ((in_buf[2 * i] - in_buf[2 * i + 1]) / 2,
                               0, G_MAXUINT8);
        }

      real_size = encode_frame (in_left, in_frame_size / 2,
                                out_buf, mid_frame_size,
                                FMT_8, FMT_LE, FMT_U);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);

      real_size = encode_frame (in_right, in_frame_size / 2,
                                out_buf, side_frame_size,
                                FMT_8, FMT_LE, FMT_U);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);
    }
}

void
encode_s16m ()
{
  gint16 *in_buf;
  gint16 *in_left;
  gint16 *in_right;
  guint8 *out_buf;
  gint in_frame_size;
  gint out_frame_size;
  gint bytes_read;
  guint16 real_size;
  gint i;

  a_hdr.magic = MAGIC;
  a_hdr.freq = w_hdr.freq;
  a_hdr.channels = MONO;
  a_hdr.bits = 16;
  a_hdr.frame = frame;

  if (fwrite (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: i/o error\n", output);
      exit (1);
    }

  in_frame_size = 2 * frame * sizeof (gint16);
  out_frame_size = CLAMP (4.0 * frame / ratio - 2, 2, G_MAXUINT16);

  in_buf = (gint16 *) g_malloc (2 * frame * sizeof (gint16));
  in_left = (gint16 *) g_malloc (frame * sizeof (gint16));
  in_right = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf = (guint8 *) g_malloc (out_frame_size * sizeof (guint8));

  while ((bytes_read = fread (in_buf, 1, in_frame_size, wav)) > 0)
    {
      if (bytes_read < in_frame_size)
        memset ((guint8 *) in_buf + bytes_read, 0, in_frame_size - bytes_read);

      for (i = 0; i < frame; i++)
        {
          in_left[i] = in_buf[2 * i];
          in_right[i] = in_buf[2 * i + 1];
        }

      for (i = 0; i < frame; i++)
        in_buf[i] = (in_left[i] + in_right[i]) / 2;

      real_size = encode_frame (in_buf, in_frame_size / 2,
                                out_buf, out_frame_size,
                                FMT_16, FMT_LE, FMT_S);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);
    }
}

void
encode_s16s ()
{
  gint16 *in_buf;
  gint16 *in_left;
  gint16 *in_right;
  guint8 *out_buf;
  gint in_frame_size;
  gint out_frame_size;
  gint bytes_read;
  guint16 real_size;
  gint i;

  a_hdr.magic = MAGIC;
  a_hdr.freq = w_hdr.freq;
  a_hdr.channels = STEREO;
  a_hdr.bits = 16;
  a_hdr.frame = frame;

  if (fwrite (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: i/o error\n", output);
      exit (1);
    }

  in_frame_size = 2 * frame * sizeof (gint16);
  out_frame_size = CLAMP (2.0 * frame / ratio - 2, 2, G_MAXUINT16);

  in_buf = (gint16 *) g_malloc (2 * frame * sizeof (gint16));
  in_left = (gint16 *) g_malloc (frame * sizeof (gint16));
  in_right = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf = (guint8 *) g_malloc (out_frame_size * sizeof (guint8));

  while ((bytes_read = fread (in_buf, 1, in_frame_size, wav)) > 0)
    {
      if (bytes_read < in_frame_size)
        memset ((guint8 *) in_buf + bytes_read, 0, in_frame_size - bytes_read);

      for (i = 0; i < frame; i++)
        {
          in_left[i] = in_buf[2 * i];
          in_right[i] = in_buf[2 * i + 1];
        }

      real_size = encode_frame (in_left, in_frame_size / 2,
                                out_buf, out_frame_size,
                                FMT_16, FMT_LE, FMT_S);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);

      real_size = encode_frame (in_right, in_frame_size / 2,
                                out_buf, out_frame_size,
                                FMT_16, FMT_LE, FMT_S);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);
    }
}

void
encode_s16j ()
{
  gint16 *in_buf;
  gint16 *in_left;
  gint16 *in_right;
  guint8 *out_buf;
  gint in_frame_size;
  gint mid_frame_size;
  gint side_frame_size;
  gint bytes_read;
  guint16 real_size;
  gint i;

  a_hdr.magic = MAGIC;
  a_hdr.freq = w_hdr.freq;
  a_hdr.channels = JSTEREO;
  a_hdr.bits = 16;
  a_hdr.frame = frame;

  if (fwrite (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: i/o error\n", output);
      exit (1);
    }

  in_frame_size = 2 * frame * sizeof (gint16);
  mid_frame_size = CLAMP (4.0 * frame / ratio * ms_ratio / 100.0 - 2,
                          2, G_MAXUINT16);
  side_frame_size = CLAMP (4.0 * frame / ratio - mid_frame_size - 2,
                           2, G_MAXUINT16);

  in_buf = (gint16 *) g_malloc (2 * frame * sizeof (gint16));
  in_left = (gint16 *) g_malloc (frame * sizeof (gint16));
  in_right = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf = (guint8 *) g_malloc (MAX (mid_frame_size, side_frame_size)
                                 * sizeof (guint8));

  while ((bytes_read = fread (in_buf, 1, in_frame_size, wav)) > 0)
    {
      if (bytes_read < in_frame_size)
        memset ((guint8 *) in_buf + bytes_read, 0, in_frame_size - bytes_read);

      for (i = 0; i < frame; i++)
        {
          in_left[i]  = CLAMP ((in_buf[2 * i] + in_buf[2 * i + 1]) / 2,
                               G_MININT16, G_MAXINT16);
          in_right[i] = CLAMP ((in_buf[2 * i] - in_buf[2 * i + 1]) / 2,
                               G_MININT16, G_MAXINT16);
        }

      real_size = encode_frame (in_left, in_frame_size / 2,
                                out_buf, mid_frame_size,
                                FMT_16, FMT_LE, FMT_S);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);

      real_size = encode_frame (in_right, in_frame_size / 2,
                                out_buf, side_frame_size,
                                FMT_16, FMT_LE, FMT_S);

      fwrite (&real_size, 1, sizeof (real_size), agress);
      fwrite (out_buf, 1, real_size, agress);
    }
}

void
decode_8m ()
{
  guint8 *in_buf;
  guint8 *out_buf1;
  guint8 *out_buf2;
  guint8 *temp;
  guint16 frame_size;
  gint bytes_read;
  gint first_frame = 1;
  gint only_frame = 1;
  gint f_size;

  fseek (wav, sizeof (w_hdr), SEEK_SET);

  in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
  out_buf1 = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf2 = (guint8 *) g_malloc (frame * sizeof (guint8));

  for (;;)
    {
      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (!bytes_read)
        break;

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf1, frame,
                        FMT_8, FMT_LE, FMT_U);
          first_frame = 0;
          continue;
        }

      decode_frame (in_buf, frame_size, out_buf2, frame,
                    FMT_8, FMT_LE, FMT_U);

      smooth_edge (out_buf1, out_buf2, frame, smooth,
                   FMT_8, FMT_LE, FMT_U);

      fwrite (out_buf1, 1, frame, wav);

      temp = out_buf1;
      out_buf1 = out_buf2;
      out_buf2 = temp;
      only_frame = 0;
    }

  if (!only_frame)
    fwrite (out_buf2, 1, frame, wav);

  f_size = ftell (wav);
  fseek (wav, 0, SEEK_SET);

  w_hdr.id_riff = RIFF;
  w_hdr.len_riff = f_size - 8;
  w_hdr.id_chuck = WAVE;
  w_hdr.fmt = FMT;
  w_hdr.len_chuck = 16;
  w_hdr.type = 1;
  w_hdr.channels = 1;
  w_hdr.freq = a_hdr.freq;
  w_hdr.bytes = a_hdr.freq;
  w_hdr.align = 1;
  w_hdr.bits = 8;
  w_hdr.id_data = DATA;
  w_hdr.len_data = f_size - 44;

  fwrite (&w_hdr, 1, sizeof (w_hdr), wav);
}

void
decode_8s ()
{
  guint8 *in_buf;
  guint8 *out_buf;
  guint8 *out_buf1;
  guint8 *out_buf2;
  guint8 *out_buf3;
  guint8 *out_buf4;
  guint8 *temp;
  guint16 frame_size;
  gint bytes_read;
  gint first_frame = 1;
  gint only_frame = 1;
  gint f_size;
  gint i;

  fseek (wav, sizeof (w_hdr), SEEK_SET);

  in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
  out_buf = (guint8 *) g_malloc (2 * frame * sizeof (guint8));
  out_buf1 = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf2 = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf3 = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf4 = (guint8 *) g_malloc (frame * sizeof (guint8));

  for (;;)
    {
      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (!bytes_read)
        break;

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf1, frame,
                        FMT_8, FMT_LE, FMT_U);
        }
      else
        {
          decode_frame (in_buf, frame_size, out_buf2, frame,
                        FMT_8, FMT_LE, FMT_U);

          smooth_edge (out_buf1, out_buf2, frame, smooth,
                       FMT_8, FMT_LE, FMT_U);
        }

      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf3, frame,
                        FMT_8, FMT_LE, FMT_U);
        }
      else
        {
          decode_frame (in_buf, frame_size, out_buf4, frame,
                        FMT_8, FMT_LE, FMT_U);

          smooth_edge (out_buf3, out_buf4, frame, smooth,
                       FMT_8, FMT_LE, FMT_U);
        }

      if (first_frame)
        {
          first_frame = 0;
          continue;
        }

      for (i = 0; i < frame; i++)
        {
          out_buf[2 * i] = out_buf1[i];
          out_buf[2 * i + 1] = out_buf3[i];
        }

      fwrite (out_buf, 1, frame * 2, wav);

      temp = out_buf1;
      out_buf1 = out_buf2;
      out_buf2 = temp;

      temp = out_buf3;
      out_buf3 = out_buf4;
      out_buf4 = temp;

      only_frame = 0;
    }

  if (!only_frame)
    {
      for (i = 0; i < frame; i++)
        {
          out_buf[2 * i] = out_buf2[i];
          out_buf[2 * i + 1] = out_buf4[i];
        }

      fwrite (out_buf, 1, frame * 2, wav);
    }

  f_size = ftell (wav);
  fseek (wav, 0, SEEK_SET);

  w_hdr.id_riff = RIFF;
  w_hdr.len_riff = f_size - 8;
  w_hdr.id_chuck = WAVE;
  w_hdr.fmt = FMT;
  w_hdr.len_chuck = 16;
  w_hdr.type = 1;
  w_hdr.channels = 2;
  w_hdr.freq = a_hdr.freq;
  w_hdr.bytes = a_hdr.freq * 2;
  w_hdr.align = 2;
  w_hdr.bits = 8;
  w_hdr.id_data = DATA;
  w_hdr.len_data = f_size - 44;

  fwrite (&w_hdr, 1, sizeof (w_hdr), wav);
}

void
decode_8j ()
{
  guint8 *in_buf;
  guint8 *out_buf;
  guint8 *out_buf1;
  guint8 *out_buf2;
  guint8 *out_buf3;
  guint8 *out_buf4;
  guint8 *temp;
  guint16 frame_size;
  gint bytes_read;
  gint first_frame = 1;
  gint only_frame = 1;
  gint f_size;
  gint i;

  fseek (wav, sizeof (w_hdr), SEEK_SET);

  in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
  out_buf = (guint8 *) g_malloc (2 * frame * sizeof (guint8));
  out_buf1 = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf2 = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf3 = (guint8 *) g_malloc (frame * sizeof (guint8));
  out_buf4 = (guint8 *) g_malloc (frame * sizeof (guint8));

  for (;;)
    {
      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (!bytes_read)
        break;

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf1, frame,
                        FMT_8, FMT_LE, FMT_U);
        }
      else
        {
          decode_frame (in_buf, frame_size, out_buf2, frame,
                        FMT_8, FMT_LE, FMT_U);

          smooth_edge (out_buf1, out_buf2, frame, smooth,
                       FMT_8, FMT_LE, FMT_U);
        }

      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf3, frame,
                        FMT_8, FMT_LE, FMT_U);
        }
      else
        {
          decode_frame (in_buf, frame_size, out_buf4, frame,
                        FMT_8, FMT_LE, FMT_U);

          smooth_edge (out_buf3, out_buf4, frame, smooth,
                       FMT_8, FMT_LE, FMT_U);
        }

      if (first_frame)
        {
          first_frame = 0;
          continue;
        }

      for (i = 0; i < frame; i++)
        {
          out_buf[2 * i] = CLAMP (out_buf1[i] + out_buf3[i], 0, G_MAXUINT8);
          out_buf[2 * i + 1] = CLAMP (out_buf1[i] - out_buf3[i], 0, G_MAXUINT8);
        }

      fwrite (out_buf, 1, frame * 2, wav);

      temp = out_buf1;
      out_buf1 = out_buf2;
      out_buf2 = temp;

      temp = out_buf3;
      out_buf3 = out_buf4;
      out_buf4 = temp;

      only_frame = 0;
    }

  if (!only_frame)
    {
      for (i = 0; i < frame; i++)
        {
          out_buf[2 * i] = CLAMP (out_buf2[i] + out_buf4[i], 0, G_MAXUINT8);
          out_buf[2 * i + 1] = CLAMP (out_buf2[i] - out_buf4[i], 0, G_MAXUINT8);
        }

      fwrite (out_buf, 1, frame * 2, wav);
    }

  f_size = ftell (wav);
  fseek (wav, 0, SEEK_SET);

  w_hdr.id_riff = RIFF;
  w_hdr.len_riff = f_size - 8;
  w_hdr.id_chuck = WAVE;
  w_hdr.fmt = FMT;
  w_hdr.len_chuck = 16;
  w_hdr.type = 1;
  w_hdr.channels = 2;
  w_hdr.freq = a_hdr.freq;
  w_hdr.bytes = a_hdr.freq * 2;
  w_hdr.align = 2;
  w_hdr.bits = 8;
  w_hdr.id_data = DATA;
  w_hdr.len_data = f_size - 44;

  fwrite (&w_hdr, 1, sizeof (w_hdr), wav);
}

void
decode_16m ()
{
  guint8 *in_buf;
  gint16 *out_buf1;
  gint16 *out_buf2;
  gint16 *temp;
  guint16 frame_size;
  gint bytes_read;
  gint first_frame = 1;
  gint only_frame = 1;
  gint f_size;

  fseek (wav, sizeof (w_hdr), SEEK_SET);

  in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
  out_buf1 = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf2 = (gint16 *) g_malloc (frame * sizeof (gint16));

  for (;;)
    {
      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (!bytes_read)
        break;

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf1, frame * 2,
                        FMT_16, FMT_LE, FMT_S);
          first_frame = 0;
          continue;
        }

      decode_frame (in_buf, frame_size, out_buf2, frame * 2,
                    FMT_16, FMT_LE, FMT_S);

      smooth_edge (out_buf1, out_buf2, frame, smooth,
                   FMT_16, FMT_LE, FMT_S);

      fwrite (out_buf1, 1, frame * 2, wav);

      temp = out_buf1;
      out_buf1 = out_buf2;
      out_buf2 = temp;
      only_frame = 0;
    }

  if (!only_frame)
    fwrite (out_buf2, 1, frame * 2, wav);

  f_size = ftell (wav);
  fseek (wav, 0, SEEK_SET);

  w_hdr.id_riff = RIFF;
  w_hdr.len_riff = f_size - 8;
  w_hdr.id_chuck = WAVE;
  w_hdr.fmt = FMT;
  w_hdr.len_chuck = 16;
  w_hdr.type = 1;
  w_hdr.channels = 1;
  w_hdr.freq = a_hdr.freq;
  w_hdr.bytes = a_hdr.freq * 2;
  w_hdr.align = 2;
  w_hdr.bits = 16;
  w_hdr.id_data = DATA;
  w_hdr.len_data = f_size - 44;

  fwrite (&w_hdr, 1, sizeof (w_hdr), wav);
}

void
decode_16s ()
{
  guint8 *in_buf;
  gint16 *out_buf;
  gint16 *out_buf1;
  gint16 *out_buf2;
  gint16 *out_buf3;
  gint16 *out_buf4;
  gint16 *temp;
  guint16 frame_size;
  gint bytes_read;
  gint first_frame = 1;
  gint only_frame = 1;
  gint f_size;
  gint i;

  fseek (wav, sizeof (w_hdr), SEEK_SET);

  in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
  out_buf = (gint16 *) g_malloc (2 * frame * sizeof (gint16));
  out_buf1 = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf2 = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf3 = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf4 = (gint16 *) g_malloc (frame * sizeof (gint16));

  for (;;)
    {
      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (!bytes_read)
        break;

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf1, frame * 2,
                        FMT_16, FMT_LE, FMT_S);
        }
      else
        {
          decode_frame (in_buf, frame_size, out_buf2, frame * 2,
                        FMT_16, FMT_LE, FMT_S);

          smooth_edge (out_buf1, out_buf2, frame, smooth,
                       FMT_16, FMT_LE, FMT_S);
        }

      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf3, frame * 2,
                        FMT_16, FMT_LE, FMT_S);
        }
      else
        {
          decode_frame (in_buf, frame_size, out_buf4, frame * 2,
                        FMT_16, FMT_LE, FMT_S);

          smooth_edge (out_buf3, out_buf4, frame, smooth,
                       FMT_16, FMT_LE, FMT_S);
        }

      if (first_frame)
        {
          first_frame = 0;
          continue;
        }

      for (i = 0; i < frame; i++)
        {
          out_buf[2 * i] = out_buf1[i];
          out_buf[2 * i + 1] = out_buf3[i];
        }

      fwrite (out_buf, 1, frame * 4, wav);

      temp = out_buf1;
      out_buf1 = out_buf2;
      out_buf2 = temp;

      temp = out_buf3;
      out_buf3 = out_buf4;
      out_buf4 = temp;

      only_frame = 0;
    }

  if (!only_frame)
    {
      for (i = 0; i < frame; i++)
        {
          out_buf[2 * i] = out_buf2[i];
          out_buf[2 * i + 1] = out_buf4[i];
        }

      fwrite (out_buf, 1, frame * 4, wav);
    }

  f_size = ftell (wav);
  fseek (wav, 0, SEEK_SET);

  w_hdr.id_riff = RIFF;
  w_hdr.len_riff = f_size - 8;
  w_hdr.id_chuck = WAVE;
  w_hdr.fmt = FMT;
  w_hdr.len_chuck = 16;
  w_hdr.type = 1;
  w_hdr.channels = 2;
  w_hdr.freq = a_hdr.freq;
  w_hdr.bytes = a_hdr.freq * 4;
  w_hdr.align = 4;
  w_hdr.bits = 16;
  w_hdr.id_data = DATA;
  w_hdr.len_data = f_size - 44;

  fwrite (&w_hdr, 1, sizeof (w_hdr), wav);
}

void
decode_16j ()
{
  guint8 *in_buf;
  gint16 *out_buf;
  gint16 *out_buf1;
  gint16 *out_buf2;
  gint16 *out_buf3;
  gint16 *out_buf4;
  gint16 *temp;
  guint16 frame_size;
  gint bytes_read;
  gint first_frame = 1;
  gint only_frame = 1;
  gint f_size;
  gint i;

  fseek (wav, sizeof (w_hdr), SEEK_SET);

  in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
  out_buf = (gint16 *) g_malloc (2 * frame * sizeof (gint16));
  out_buf1 = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf2 = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf3 = (gint16 *) g_malloc (frame * sizeof (gint16));
  out_buf4 = (gint16 *) g_malloc (frame * sizeof (gint16));

  for (;;)
    {
      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (!bytes_read)
        break;

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf1, frame * 2,
                        FMT_16, FMT_LE, FMT_S);
        }
      else
        {
          decode_frame (in_buf, frame_size, out_buf2, frame * 2,
                        FMT_16, FMT_LE, FMT_S);

          smooth_edge (out_buf1, out_buf2, frame, smooth,
                       FMT_16, FMT_LE, FMT_S);
        }

      bytes_read = fread (&frame_size, 1, sizeof (frame_size), agress);

      if (bytes_read != sizeof (guint16))
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      bytes_read = fread (in_buf, 1, frame_size, agress);

      if (bytes_read != frame_size)
        {
          fprintf (stderr, "%s: unexcpected end of file\n", input);
          break;
        }

      if (first_frame)
        {
          decode_frame (in_buf, frame_size, out_buf3, frame * 2,
                        FMT_16, FMT_LE, FMT_S);
        }
      else
        {
          decode_frame (in_buf, frame_size, out_buf4, frame * 2,
                        FMT_16, FMT_LE, FMT_S);

          smooth_edge (out_buf3, out_buf4, frame, smooth,
                       FMT_16, FMT_LE, FMT_S);
        }

      if (first_frame)
        {
          first_frame = 0;
          continue;
        }

      for (i = 0; i < frame; i++)
        {
          out_buf[2 * i] = CLAMP (out_buf1[i] + out_buf3[i],
                                  G_MININT16, G_MAXINT16);
          out_buf[2 * i + 1] = CLAMP (out_buf1[i] - out_buf3[i],
                                      G_MININT16, G_MAXINT16);
        }

      fwrite (out_buf, 1, frame * 4, wav);

      temp = out_buf1;
      out_buf1 = out_buf2;
      out_buf2 = temp;

      temp = out_buf3;
      out_buf3 = out_buf4;
      out_buf4 = temp;

      only_frame = 0;
    }

  if (!only_frame)
    {
      for (i = 0; i < frame; i++)
        {
          out_buf[2 * i] = CLAMP (out_buf2[i] + out_buf4[i],
                                  G_MININT16, G_MAXINT16);
          out_buf[2 * i + 1] = CLAMP (out_buf2[i] - out_buf4[i],
                                      G_MININT16, G_MAXINT16);
        }

      fwrite (out_buf, 1, frame * 4, wav);
    }

  f_size = ftell (wav);
  fseek (wav, 0, SEEK_SET);

  w_hdr.id_riff = RIFF;
  w_hdr.len_riff = f_size - 8;
  w_hdr.id_chuck = WAVE;
  w_hdr.fmt = FMT;
  w_hdr.len_chuck = 16;
  w_hdr.type = 1;
  w_hdr.channels = 2;
  w_hdr.freq = a_hdr.freq;
  w_hdr.bytes = a_hdr.freq * 4;
  w_hdr.align = 4;
  w_hdr.bits = 16;
  w_hdr.id_data = DATA;
  w_hdr.len_data = f_size - 44;

  fwrite (&w_hdr, 1, sizeof (w_hdr), wav);
}

void
encode_file ()
{
  wav = fopen (input, "rb");

  if (!wav)
    {
      fprintf (stderr, "Cannot open file: %s: %m\n", input);
      exit (1);
    }

  agress = fopen (output, "wb");

  if (!agress)
    {
      fprintf (stderr, "Cannot create file: %s: %m\n", output);
      exit (1);
    }

  if (fread (&w_hdr, 1, sizeof (w_hdr), wav) != sizeof (w_hdr))
    {
      fprintf (stderr, "%s: not a wav file\n", input);
      exit (1);
    }

  if ((w_hdr.id_riff != RIFF)
      || (w_hdr.id_chuck != WAVE)
      || (w_hdr.fmt != FMT)
      || (w_hdr.id_data != DATA)
      || (w_hdr.type != 1))
    {
      fprintf (stderr, "%s: not a wav file\n", input);
      exit (1);
    }

  if (w_hdr.bits == 8)
    {
      if (w_hdr.channels == 1)
        {
          encode_m8 (&w_hdr);
        }
      else if (w_hdr.channels == 2)
        {
          if (mode == MONO)
            encode_s8m ();
          else if (mode == STEREO)
            encode_s8s ();
          else if (mode == JSTEREO)
            encode_s8j ();
          else
            g_assert_not_reached ();
        }
    }
  else if (w_hdr.bits == 16)
    {
      if (w_hdr.channels == 1)
        {
          encode_m16 ();
        }
      else if (w_hdr.channels == 2)
        {
          if (mode == MONO)
            encode_s16m ();
          else if (mode == STEREO)
            encode_s16s ();
          else if (mode == JSTEREO)
            encode_s16j ();
          else
            g_assert_not_reached ();
        }
      else
        g_assert_not_reached ();
    }
  else
    g_assert_not_reached ();
}

void
decode_file ()
{
  agress = fopen (input, "rb");

  if (!agress)
    {
      fprintf (stderr, "Cannot open file: %s: %m\n", input);
      exit (1);
    }

  wav = fopen (output, "wb");

  if (!wav)
    {
      fprintf (stderr, "Cannot create file: %s: %m\n", output);
      exit (1);
    }

  if (fread (&a_hdr, 1, sizeof (a_hdr), agress) != sizeof (a_hdr))
    {
      fprintf (stderr, "%s: not an agress file\n", input);
      exit (1);
    }

  if (a_hdr.magic != MAGIC)
    {
      fprintf (stderr, "%s: not an agress file\n", input);
      exit (1);
    }

  frame = a_hdr.frame;

  if (a_hdr.bits == 8)
    {
      if (a_hdr.channels == MONO)
        decode_8m ();
      else if (a_hdr.channels == STEREO)
        decode_8s ();
      else if (a_hdr.channels == JSTEREO)
        decode_8j ();
      else
        g_assert_not_reached ();
    }
  else if (a_hdr.bits == 16)
    {
      if (a_hdr.channels == MONO)
        decode_16m ();
      else if (a_hdr.channels == STEREO)
        decode_16s ();
      else if (a_hdr.channels == JSTEREO)
        decode_16j ();
      else
        g_assert_not_reached ();
    }
  else
    g_assert_not_reached ();
}

int
main (int argc, char **argv)
{
  parse_options (argc, argv);

  if (encode == ENCODE)
    encode_file ();
  else
    decode_file ();

  return 0;
}
