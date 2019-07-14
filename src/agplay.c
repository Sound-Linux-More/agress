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
#include <glib.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>

#define PACKED __attribute__ ((packed))

#define MAGIC 0x4741

#define MONO    1
#define STEREO  2
#define JSTEREO 3

#define SOUND_DEVICE "/dev/dsp"

#define DEF_SMOOTH 5

typedef struct agress_header_tag
{
    guint16 magic PACKED;
    guint16 freq PACKED;
    guint16 frame PACKED;
    guint8 channels PACKED;
    guint8 bits PACKED;
} agress_header;

void play_file (gchar *agfile);
gint open_sound (agress_header *hdr);
void play_8m (gint agress_fd, agress_header *hdr);
void play_8s (gint agress_fd, agress_header *hdr);
void play_8j (gint agress_fd, agress_header *hdr);
void play_16m (gint agress_fd, agress_header *hdr);
void play_16s (gint agress_fd, agress_header *hdr);
void play_16j (gint agress_fd, agress_header *hdr);

gint
open_sound (agress_header *hdr)
{
    gint format;
    gint channels;
    gint freq;
    gint fd;

    if ((fd = open (SOUND_DEVICE, O_WRONLY)) == -1)
        return -1;

    format = hdr->bits == 8 ? AFMT_U8 : AFMT_S16_LE;
    channels = hdr->channels == MONO ? 1 : 2;
    freq = hdr->freq;

    if (ioctl (fd, SNDCTL_DSP_SETFMT, &format) == -1)
        return -1;

    if (ioctl (fd, SNDCTL_DSP_CHANNELS, &channels) == -1)
        return -1;

    if (ioctl (fd, SNDCTL_DSP_SPEED, &freq) == -1)
        return -1;

    return fd;
}

void
play_8m (gint agress_fd, agress_header *hdr)
{
    guint8 *in_buf;
    guint8 *out_buf1;
    guint8 *out_buf2;
    guint8 *temp;
    guint16 frame_size;
    gint bytes_read;
    gint first_frame = 1;
    gint only_frame = 1;
    gint audio_fd;
    gint frame;

    if ((audio_fd = open_sound (hdr)) == -1)
    {
        fprintf (stderr, "open %s: %m\n", SOUND_DEVICE);
        exit (1);
    }

    frame = hdr->frame;

    in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
    out_buf1 = (guint8 *) g_malloc (frame * sizeof (guint8));
    out_buf2 = (guint8 *) g_malloc (frame * sizeof (guint8));

    for (;;)
    {
        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (!bytes_read)
            break;

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

        smooth_edge (out_buf1, out_buf2, frame, DEF_SMOOTH,
                     FMT_8, FMT_LE, FMT_U);

        write (audio_fd, out_buf2, frame);

        temp = out_buf1;
        out_buf1 = out_buf2;
        out_buf2 = temp;
        only_frame = 0;
    }

    if (!only_frame)
        write (audio_fd, out_buf2, frame);

    close (audio_fd);

    g_free (in_buf);
    g_free (out_buf1);
    g_free (out_buf2);
}

void
play_8s (gint agress_fd, agress_header *hdr)
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
    gint audio_fd;
    gint frame;
    gint i;

    if ((audio_fd = open_sound (hdr)) == -1)
    {
        fprintf (stderr, "open %s: %m\n", SOUND_DEVICE);
        exit (1);
    }

    frame = hdr->frame;

    in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
    out_buf = (guint8 *) g_malloc (2 * frame * sizeof (guint8));
    out_buf1 = (guint8 *) g_malloc (frame * sizeof (guint8));
    out_buf2 = (guint8 *) g_malloc (frame * sizeof (guint8));
    out_buf3 = (guint8 *) g_malloc (frame * sizeof (guint8));
    out_buf4 = (guint8 *) g_malloc (frame * sizeof (guint8));

    for (;;)
    {
        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (!bytes_read)
            break;

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

            smooth_edge (out_buf1, out_buf2, frame, DEF_SMOOTH,
                         FMT_8, FMT_LE, FMT_U);
        }

        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

            smooth_edge (out_buf3, out_buf4, frame, DEF_SMOOTH,
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

        write (audio_fd, out_buf, frame * 2);

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

        write (audio_fd, out_buf, frame * 2);
    }

    close (audio_fd);

    g_free (in_buf);
    g_free (out_buf);
    g_free (out_buf1);
    g_free (out_buf2);
    g_free (out_buf3);
    g_free (out_buf4);
}

void
play_8j (gint agress_fd, agress_header *hdr)
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
    gint audio_fd;
    gint frame;
    gint i;

    if ((audio_fd = open_sound (hdr)) == -1)
    {
        fprintf (stderr, "open %s: %m\n", SOUND_DEVICE);
        exit (1);
    }

    frame = hdr->frame;

    in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
    out_buf = (guint8 *) g_malloc (2 * frame * sizeof (guint8));
    out_buf1 = (guint8 *) g_malloc (frame * sizeof (guint8));
    out_buf2 = (guint8 *) g_malloc (frame * sizeof (guint8));
    out_buf3 = (guint8 *) g_malloc (frame * sizeof (guint8));
    out_buf4 = (guint8 *) g_malloc (frame * sizeof (guint8));

    for (;;)
    {
        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (!bytes_read)
            break;

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

            smooth_edge (out_buf1, out_buf2, frame, DEF_SMOOTH,
                         FMT_8, FMT_LE, FMT_U);
        }

        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

            smooth_edge (out_buf3, out_buf4, frame, DEF_SMOOTH,
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

        write (audio_fd, out_buf, frame * 2);

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

        write (audio_fd, out_buf, frame * 2);
    }

    close (audio_fd);

    g_free (in_buf);
    g_free (out_buf);
    g_free (out_buf1);
    g_free (out_buf2);
    g_free (out_buf3);
    g_free (out_buf4);
}

void
play_16m (gint agress_fd, agress_header *hdr)
{
    guint8 *in_buf;
    gint16 *out_buf1;
    gint16 *out_buf2;
    gint16 *temp;
    guint16 frame_size;
    gint bytes_read;
    gint first_frame = 1;
    gint only_frame = 1;
    gint audio_fd;
    gint frame;

    if ((audio_fd = open_sound (hdr)) == -1)
    {
        fprintf (stderr, "open %s: %m\n", SOUND_DEVICE);
        exit (1);
    }

    frame = hdr->frame;

    in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
    out_buf1 = (gint16 *) g_malloc (frame * sizeof (gint16));
    out_buf2 = (gint16 *) g_malloc (frame * sizeof (gint16));

    for (;;)
    {
        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (!bytes_read)
            break;

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

        smooth_edge (out_buf1, out_buf2, frame, DEF_SMOOTH,
                     FMT_16, FMT_LE, FMT_S);

        write (audio_fd, out_buf1, frame * 2);

        temp = out_buf1;
        out_buf1 = out_buf2;
        out_buf2 = temp;
        only_frame = 0;
    }

    if (!only_frame)
        write (audio_fd, out_buf2, frame * 2);

    close (audio_fd);

    g_free (in_buf);
    g_free (out_buf1);
    g_free (out_buf2);
}

void
play_16s (gint agress_fd, agress_header *hdr)
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
    gint audio_fd;
    gint frame;
    gint i;

    if ((audio_fd = open_sound (hdr)) == -1)
    {
        fprintf (stderr, "open %s: %m\n", SOUND_DEVICE);
        exit (1);
    }

    frame = hdr->frame;

    in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
    out_buf = (gint16 *) g_malloc (2 * frame * sizeof (gint16));
    out_buf1 = (gint16 *) g_malloc (frame * sizeof (gint16));
    out_buf2 = (gint16 *) g_malloc (frame * sizeof (gint16));
    out_buf3 = (gint16 *) g_malloc (frame * sizeof (gint16));
    out_buf4 = (gint16 *) g_malloc (frame * sizeof (gint16));

    for (;;)
    {
        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (!bytes_read)
            break;

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

            smooth_edge (out_buf1, out_buf2, frame, DEF_SMOOTH,
                         FMT_16, FMT_LE, FMT_S);
        }

        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

            smooth_edge (out_buf3, out_buf4, frame, DEF_SMOOTH,
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

        write (audio_fd, out_buf, frame * 4);

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

        write (audio_fd, out_buf, frame * 4);
    }

    close (audio_fd);

    g_free (in_buf);
    g_free (out_buf);
    g_free (out_buf1);
    g_free (out_buf2);
    g_free (out_buf3);
    g_free (out_buf4);
}

void
play_16j (gint agress_fd, agress_header *hdr)
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
    gint audio_fd;
    gint frame;
    gint i;

    if ((audio_fd = open_sound (hdr)) == -1)
    {
        fprintf (stderr, "open %s: %m\n", SOUND_DEVICE);
        exit (1);
    }

    frame = hdr->frame;

    in_buf = (guint8 *) g_malloc (G_MAXUINT16 * sizeof (guint8));
    out_buf = (gint16 *) g_malloc (2 * frame * sizeof (gint16));
    out_buf1 = (gint16 *) g_malloc (frame * sizeof (gint16));
    out_buf2 = (gint16 *) g_malloc (frame * sizeof (gint16));
    out_buf3 = (gint16 *) g_malloc (frame * sizeof (gint16));
    out_buf4 = (gint16 *) g_malloc (frame * sizeof (gint16));

    for (;;)
    {
        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (!bytes_read)
            break;

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

            smooth_edge (out_buf1, out_buf2, frame, DEF_SMOOTH,
                         FMT_16, FMT_LE, FMT_S);
        }

        bytes_read = read (agress_fd, &frame_size, sizeof (frame_size));

        if (bytes_read != sizeof (guint16))
        {
            fprintf (stderr, "Unexcpected end of file!\n");
            break;
        }

        bytes_read = read (agress_fd, in_buf, frame_size);

        if (bytes_read != frame_size)
        {
            fprintf (stderr, "Unexcpected end of file!\n");
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

            smooth_edge (out_buf3, out_buf4, frame, DEF_SMOOTH,
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

        write (audio_fd, out_buf, frame * 4);

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

        write (audio_fd, out_buf, frame * 4);
    }

    close (audio_fd);

    g_free (in_buf);
    g_free (out_buf);
    g_free (out_buf1);
    g_free (out_buf2);
    g_free (out_buf3);
    g_free (out_buf4);
}

void
play_file (gchar *agfile)
{
    agress_header hdr;
    gint agress_fd;

    agress_fd = open (agfile, O_RDONLY);

    if (agress_fd == -1)
    {
        fprintf (stderr, "Cannot open file: %s: %m\n", agfile);
        return;
    }

    if (read (agress_fd, &hdr, sizeof (hdr)) != sizeof (hdr))
    {
        fprintf (stderr, "%s: not an agress file\n", agfile);
        return;
    }

    if (hdr.magic != MAGIC)
    {
        fprintf (stderr, "%s: not an agress file\n", agfile);
        return;
    }

    if (hdr.bits == 8)
    {
        if (hdr.channels == MONO)
            play_8m (agress_fd, &hdr);
        else if (hdr.channels == STEREO)
            play_8s (agress_fd, &hdr);
        else if (hdr.channels == JSTEREO)
            play_8j (agress_fd, &hdr);
        else
            g_assert_not_reached ();
    }
    else if (hdr.bits == 16)
    {
        if (hdr.channels == MONO)
            play_16m (agress_fd, &hdr);
        else if (hdr.channels == STEREO)
            play_16s (agress_fd, &hdr);
        else if (hdr.channels == JSTEREO)
            play_16j (agress_fd, &hdr);
        else
            g_assert_not_reached ();
    }
    else
        g_assert_not_reached ();

    close (agress_fd);
}

int
main (int argc, char **argv)
{
    argv++;

    while (*argv)
    {
        printf ("playing file: %s\n", *argv);
        play_file (*argv);
        argv++;
    }

    return 0;
}
