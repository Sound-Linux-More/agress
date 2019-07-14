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

#define ALPHA   -1.58615986717275
#define BETA    -0.05297864003258
#define GAMMA    0.88293362717904
#define DELTA    0.44350482244527
#define EPSILON  1.14960430535816

#define TYPE_S 0
#define TYPE_A 1
#define TYPE_B 2

#define MIN_FRAME_SIZE 1

#define ROUND(x) ((x) < 0.0 ? ((gint) ((x) - 0.5)) : ((gint) ((x) + 0.5)))
#define SIGN(x) ((x) >= 0 ? 0 : 1)

typedef struct bit_stream_tag
{
    guint8 *first_byte;
    guint8 *next_byte;
    guint8 *last_byte;
    guint8 bits;
    guint8 mask;
} bit_stream;

static gint
power_of_two (gint num);

static void
round_signal (gdouble *input_signal, gint *output_signal,
              gint signal_length);

static void
interlace (gdouble *input_signal, gdouble *output_signal,
           gint signal_length);

static void
deinterlace (gdouble *input_signal, gdouble *output_signal,
             gint signal_length);

static void
analysis_filter (gdouble *signal, gint signal_length);

static void
synthesis_filter (gdouble *signal, gint signal_length);

static void
fdwt (gdouble *input_signal, gdouble *output_signal,
      gint signal_length);

static void
idwt (gdouble *input_signal, gdouble *output_signal,
      gint signal_length);

static void
init_write_bits (bit_stream *stream, gint8 *buffer,
                 gint buffer_size);

static void
init_read_bits (bit_stream *stream, gint8 *buffer,
                gint buffer_size);

static gint
write_bit (bit_stream *stream, gint bit);

static gint
read_bit (bit_stream *stream, gint *bit);

static gint
flush_bits (bit_stream *stream);

static void
make_zeromap (gint *dwt, gint *map, gint length);

static gint
is_significant (gint *dwt, gint *map, gint index,
                gint type, gint threshold);

static gint
is_type_a (gint index, gint length);

static gint
is_type_b (gint index, gint length);

static gint
initial_threshold (gint *dwt, gint length);

static void
coeff_init (gint *dwt, gint threshold, gint sign, gint index);

static void
spiht_init (GList **LIP, GList **LIS, gint length);

static gint
significance_encode (gint *dwt, gint *map, gint length, gint threshold,
                     GList **LIP, GList **LSP, GList **LIS,
                     bit_stream *stream);

static gint
refinement_encode (gint *dwt, gint threshold,
                   GList **LSP, bit_stream *stream);

static gint
significance_decode (gint *dwt, gint length, gint threshold,
                     GList **LIP, GList **LSP, GList **LIS,
                     bit_stream *stream);

static gint
refinement_decode (gint *dwt, gint threshold,
                   GList **LSP, bit_stream *stream);

static gint
spiht_encode (gint *dwt, gint length, guint8 *buffer,
              gint buffer_size);

static void
spiht_decode (gint *dwt, gint length, guint8 *buffer,
              gint buffer_size);

static void
smooth_edge_s8 (gint8 *signal_1, gint8 *signal_2,
                gint signal_length, gint smooth_factor);

static void
smooth_edge_u8 (guint8 *signal_1, guint8 *signal_2,
                gint signal_length, gint smooth_factor);

static void
smooth_edge_s16le (gint16 *signal_1, gint16 *signal_2,
                   gint signal_length, gint smooth_factor);

static void
smooth_edge_s16be (gint16 *signal_1, gint16 *signal_2,
                   gint signal_length, gint smooth_factor);

static void
smooth_edge_u16le (guint16 *signal_1, guint16 *signal_2,
                   gint signal_length, gint smooth_factor);

static void
smooth_edge_u16be (guint16 *signal_1, guint16 *signal_2,
                   gint signal_length, gint smooth_factor);

static gint
power_of_two (gint num)
{
    gint msf, lsf;

    g_assert (num > 1);

    msf = g_bit_nth_msf (num, -1);
    lsf = g_bit_nth_lsf (num, -1);

    g_assert (msf == lsf);

    return msf;
}

static void
interlace (gdouble *input_signal, gdouble *output_signal,
           gint signal_length)
{
    gint half, i;

    half = signal_length / 2;

    for (i = 0; i < half; i++)
    {
        output_signal[2 * i] = input_signal[i];
        output_signal[2 * i + 1] = input_signal[i + half];
    }
}

static void
deinterlace (gdouble *input_signal, gdouble *output_signal,
             gint signal_length)
{
    gint half, i;

    half = signal_length / 2;

    for (i = 0; i < half; i++)
    {
        output_signal[i] = input_signal[2 * i];
        output_signal[i + half] = input_signal[2 * i + 1];
    }
}

static void
analysis_filter (gdouble *signal, gint signal_length)
{
    gint index;

    for (index = 1; index < signal_length - 2; index += 2)
        signal[index] +=
            ALPHA * (signal[index - 1] + signal[index + 1]);

    signal[signal_length - 1] +=
        2.0 * ALPHA * signal[signal_length - 2];

    signal[0] +=
        2.0 * BETA * signal[1];

    for (index = 2; index < signal_length; index += 2)
        signal[index] +=
            BETA * (signal[index + 1] + signal[index - 1]);

    for (index = 1; index < signal_length - 2; index += 2)
        signal[index] +=
            GAMMA * (signal[index - 1] + signal[index + 1]);

    signal[signal_length - 1] +=
        2.0 * GAMMA * signal[signal_length - 2];

    signal[0] = EPSILON * (signal[0] + 2.0 * DELTA * signal[1]);

    for (index = 2; index < signal_length; index += 2)
        signal[index] = EPSILON * (signal[index] +
                                   DELTA * (signal[index + 1] + signal[index - 1]));

    for (index = 1; index < signal_length; index += 2)
        signal[index] /= (-EPSILON);
}

static void
synthesis_filter (gdouble *signal, gint signal_length)
{
    gint index;

    for (index = 1; index < signal_length; index += 2)
        signal[index] *= (-EPSILON);

    signal[0] =
        signal[0] / EPSILON - 2.0 * DELTA * signal[1];

    for (index = 2; index < signal_length; index += 2)
        signal[index] = signal[index] / EPSILON -
                        DELTA * (signal[index + 1] + signal[index - 1]);

    for (index = 1; index < signal_length - 2; index += 2)
        signal[index] -=
            GAMMA * (signal[index - 1] + signal[index + 1]);

    signal[signal_length - 1] -=
        2.0 * GAMMA * signal[signal_length - 2];

    signal[0] -=
        2.0 * BETA * signal[1];

    for (index = 2; index < signal_length; index += 2)
        signal[index] -=
            BETA * (signal[index + 1] + signal[index - 1]);

    for (index = 1; index < signal_length - 2; index += 2)
        signal[index] -=
            ALPHA * (signal[index - 1] + signal[index + 1]);

    signal[signal_length - 1] -=
        2.0 * ALPHA * signal[signal_length - 2];
}

static void
fdwt (gdouble *input_signal, gdouble *output_signal,
      gint signal_length)
{
    gint scale, scales;
    gdouble *temp;

    scales = power_of_two (signal_length);

    temp = (gdouble *) g_malloc (signal_length * sizeof (gdouble));
    g_memmove (temp, input_signal, signal_length * sizeof (gdouble));

    for (scale = 0; scale < scales; scale++)
    {
        analysis_filter (temp, signal_length);
        deinterlace (temp, output_signal, signal_length);
        g_memmove (temp, output_signal, signal_length * sizeof (gdouble));
        signal_length /= 2;
    }

    g_free (temp);
}

static void
idwt (gdouble *input_signal, gdouble *output_signal,
      gint signal_length)
{
    gint scale, scales;
    gdouble *temp;

    scales = power_of_two (signal_length);

    temp = g_malloc (signal_length * sizeof (gdouble));
    g_memmove (temp, input_signal, signal_length * sizeof (gdouble));
    signal_length = 2;

    for (scale = 0; scale < scales; scale++)
    {
        interlace (temp, output_signal, signal_length);
        synthesis_filter (output_signal, signal_length);
        g_memmove (temp, output_signal, signal_length * sizeof (gdouble));
        signal_length *= 2;
    }

    g_free (temp);
}

static void
init_write_bits (bit_stream *stream, gint8 *buffer,
                 gint buffer_size)
{
    stream->first_byte = buffer;
    stream->next_byte = buffer;
    stream->last_byte = buffer + buffer_size;
    stream->bits = 0;
    stream->mask = 0x80;
}

static void
init_read_bits (bit_stream *stream, gint8 *buffer,
                gint buffer_size)
{
    stream->first_byte = buffer;
    stream->next_byte = buffer;
    stream->last_byte = buffer + buffer_size;
    stream->bits = 0;
    stream->mask = 0x00;
}

static gint
write_bit (bit_stream *stream, gint bit)
{
    if (stream->next_byte >= stream->last_byte)
        return FALSE;

    if (bit)
        stream->bits |= stream->mask;

    stream->mask >>= 1;

    if (!stream->mask)
    {
        *stream->next_byte++ = stream->bits;
        stream->bits = 0;
        stream->mask = 0x80;
    }

    return TRUE;
}

static gint
read_bit (bit_stream *stream, gint *bit)
{
    *bit = 0;

    if (!stream->mask)
    {
        if (stream->next_byte >= stream->last_byte)
            return FALSE;

        stream->bits = *stream->next_byte++;
        stream->mask = 0x80;
    }

    if (stream->bits & stream->mask)
        *bit = 1;

    stream->mask >>= 1;

    return TRUE;
}

static gint
flush_bits (bit_stream *stream)
{
    if (stream->next_byte >= stream->last_byte)
        return FALSE;

    if (stream->mask != 0x80)
        *stream->next_byte++ = stream->bits;

    return TRUE;
}

static void
make_zeromap (gint *dwt, gint *map, gint length)
{
    gint cur_start, cur_length;
    gint levels;
    gint i, j;

    levels = power_of_two (length) - 1;

    for (i = length / 2; i < length; i++)
        map[i] = ABS (dwt[i]);

    cur_start = length / 4;
    cur_length = length / 2;

    for (i = 0; i < levels; i++)
    {
        for (j = cur_start; j < cur_length; j++)
        {
            map[j] = MAX (map[2 * j], map[2 * j + 1]);
            map[j] = MAX (map[j], ABS (dwt[2 * j]));
            map[j] = MAX (map[j], ABS (dwt[2 * j + 1]));
        }

        cur_start /= 2;
        cur_length /= 2;
    }

    map[0] = MAX (map[1], ABS (dwt[1]));
}

static gint
is_significant (gint *dwt, gint *map, gint index,
                gint type, gint threshold)
{
    switch (type)
    {
    case TYPE_S:
    {
        if (ABS (dwt[index]) >= threshold)
            return TRUE;
        else
            return FALSE;
    }

    case TYPE_A:
    {
        if (map[index] >= threshold)
            return TRUE;
        else
            return FALSE;
    }

    case TYPE_B:
    {
        if (map[2 * index] >= threshold)
            return TRUE;
        if (map[2 * index + 1] >= threshold)
            return TRUE;
        return FALSE;
    }

    default:
        g_assert_not_reached ();
    }
}

static gint
is_type_a (gint index, gint length)
{
    if (!index)
        return FALSE;

    if (index >= length / 2)
        return FALSE;

    return TRUE;
}

static gint
is_type_b (gint index, gint length)
{
    if (!index)
        return FALSE;

    if (index >= length / 4)
        return FALSE;

    return TRUE;
}

static gint
initial_threshold (gint *dwt, gint length)
{
    gint threshold, max = 0;
    gint i;

    for (i = 0; i < length; i++)
    {
        if (ABS (dwt[i]) > max)
            max = ABS (dwt[i]);
    }

    if (!max)
        return 0;

    threshold = 1 << g_bit_nth_msf (max, -1);

    return threshold;
}

static void
coeff_init (gint *dwt, gint threshold, gint sign, gint index)
{
    dwt[index] = threshold + threshold / 2;
    dwt[index] = sign ? -dwt[index] : dwt[index];
}

static void
spiht_init (GList **LIP, GList **LIS, gint length)
{
    *LIP = g_list_append (*LIP, GINT_TO_POINTER (0));
    *LIP = g_list_append (*LIP, GINT_TO_POINTER (1));

    if (is_type_a (1, length) == TRUE)
        *LIS = g_list_append (*LIS, GINT_TO_POINTER (1));
}

static gint
significance_encode (gint *dwt, gint *map, gint length,
                     gint threshold, GList **LIP, GList **LSP, GList **LIS,
                     bit_stream *stream)
{
    GList *cur, *next;
    gint index, sign;
    gint rc;

    cur = *LIP;

    while (cur != NULL)
    {
        next = g_list_next (cur);
        index = GPOINTER_TO_INT (cur->data);
        rc = is_significant (dwt, map, index, TYPE_S, threshold);

        if (rc == TRUE)
        {
            if (write_bit (stream, 1) != TRUE)
                return FALSE;

            sign = SIGN (dwt[index]);

            if (write_bit (stream, sign) != TRUE)
                return FALSE;

            *LSP = g_list_append (*LSP, GINT_TO_POINTER (index));
            *LIP = g_list_delete_link (*LIP, cur);
        }
        else
        {
            if (write_bit (stream, 0) != TRUE)
                return FALSE;
        }

        cur = next;
    }

    cur = *LIS;

    while (cur != NULL)
    {
        next = g_list_next (cur);
        index = GPOINTER_TO_INT (cur->data);

        if (index > 0)
        {
            rc = is_significant (dwt, map, index, TYPE_A, threshold);

            if (rc == TRUE)
            {
                if (write_bit (stream, 1) != TRUE)
                    return FALSE;

                rc = is_significant (dwt, map, 2 * index, TYPE_S, threshold);

                if (rc == TRUE)
                {
                    if (write_bit (stream, 1) != TRUE)
                        return FALSE;

                    sign = SIGN (dwt[2 * index]);

                    if (write_bit (stream, sign) != TRUE)
                        return FALSE;

                    *LSP = g_list_append (*LSP, GINT_TO_POINTER (2 * index));
                }
                else
                {
                    if (write_bit (stream, 0) != TRUE)
                        return FALSE;

                    *LIP = g_list_append (*LIP, GINT_TO_POINTER (2 * index));
                }

                rc = is_significant (dwt, map, 2 * index + 1, TYPE_S, threshold);

                if (rc == TRUE)
                {
                    if (write_bit (stream, 1) != TRUE)
                        return FALSE;

                    sign = SIGN (dwt[2 * index + 1]);

                    if (write_bit (stream, sign) != TRUE)
                        return FALSE;

                    *LSP = g_list_append (*LSP, GINT_TO_POINTER (2 * index + 1));
                }
                else
                {
                    if (write_bit (stream, 0) != TRUE)
                        return FALSE;

                    *LIP = g_list_append (*LIP, GINT_TO_POINTER (2 * index + 1));
                }

                if (is_type_b (index, length) == TRUE)
                {
                    *LIS = g_list_append (*LIS, GINT_TO_POINTER (-index));
                    next = g_list_next (cur);
                    *LIS = g_list_delete_link (*LIS, cur);
                }
                else
                {
                    *LIS = g_list_delete_link (*LIS, cur);
                }
            }
            else
            {
                if (write_bit (stream, 0) != TRUE)
                    return FALSE;
            }
        }
        else
        {
            index = ABS (index);

            rc = is_significant (dwt, map, index, TYPE_B, threshold);

            if (rc == TRUE)
            {
                if (write_bit (stream, 1) != TRUE)
                    return FALSE;

                *LIS = g_list_append (*LIS, GINT_TO_POINTER (2 * index));
                *LIS = g_list_append (*LIS, GINT_TO_POINTER (2 * index + 1));
                next = g_list_next (cur);
                *LIS = g_list_delete_link (*LIS, cur);
            }
            else
            {
                if (write_bit (stream, 0) != TRUE)
                    return FALSE;
            }
        }

        cur = next;
    }

    return TRUE;
}

static gint
refinement_encode (gint *dwt, gint threshold,
                   GList **LSP, bit_stream *stream)
{
    GList *cur;
    gint bit;

    threshold /= 2;

    if (!threshold)
        return TRUE;

    cur = *LSP;

    while (cur != NULL)
    {
        bit = ABS (dwt[GPOINTER_TO_INT (cur->data)]);
        bit = bit & threshold ? 1 : 0;

        if (write_bit (stream, bit) != TRUE)
            return FALSE;

        cur = cur->next;
    }

    return TRUE;
}

static gint
significance_decode (gint *dwt, gint length, gint threshold,
                     GList **LIP, GList **LSP, GList **LIS,
                     bit_stream *stream)
{
    GList *cur, *next;
    gint index, sign;
    gint bit;

    cur = *LIP;

    while (cur != NULL)
    {
        next = g_list_next (cur);
        index = GPOINTER_TO_INT (cur->data);

        if (read_bit (stream, &bit) != TRUE)
            return FALSE;

        if (bit == 1)
        {
            if (read_bit (stream, &sign) != TRUE)
                return FALSE;

            coeff_init (dwt, threshold, sign, index);

            *LSP = g_list_append (*LSP, GINT_TO_POINTER (index));
            *LIP = g_list_delete_link (*LIP, cur);
        }

        cur = next;
    }

    cur = *LIS;

    while (cur != NULL)
    {
        next = g_list_next (cur);
        index = GPOINTER_TO_INT (cur->data);

        if (index > 0)
        {
            if (read_bit (stream, &bit) != TRUE)
                return FALSE;

            if (bit == 1)
            {
                if (read_bit (stream, &bit) != TRUE)
                    return FALSE;

                if (bit == 1)
                {
                    if (read_bit (stream, &sign) != TRUE)
                        return FALSE;

                    coeff_init (dwt, threshold, sign, 2 *index);

                    *LSP = g_list_append (*LSP, GINT_TO_POINTER (2 * index));
                }
                else
                {
                    *LIP = g_list_append (*LIP, GINT_TO_POINTER (2 * index));
                }

                if (read_bit (stream, &bit) != TRUE)
                    return FALSE;

                if (bit == 1)
                {
                    if (read_bit (stream, &sign) != TRUE)
                        return FALSE;

                    coeff_init (dwt, threshold, sign, 2 *index + 1);

                    *LSP = g_list_append (*LSP, GINT_TO_POINTER (2 * index + 1));
                }
                else
                {
                    *LIP = g_list_append (*LIP, GINT_TO_POINTER (2 * index + 1));
                }

                if (is_type_b (index, length) == TRUE)
                {
                    *LIS = g_list_append (*LIS, GINT_TO_POINTER (-index));
                    next = g_list_next (cur);
                    *LIS = g_list_delete_link (*LIS, cur);
                }
                else
                {
                    *LIS = g_list_delete_link (*LIS, cur);
                }
            }
        }
        else
        {
            index = ABS (index);

            if (read_bit (stream, &bit) != TRUE)
                return FALSE;

            if (bit == TRUE)
            {
                *LIS = g_list_append (*LIS, GINT_TO_POINTER (2 * index));
                *LIS = g_list_append (*LIS, GINT_TO_POINTER (2 * index + 1));
                next = g_list_next (cur);
                *LIS = g_list_delete_link (*LIS, cur);
            }
        }

        cur = next;
    }

    return TRUE;
}

static gint
refinement_decode (gint *dwt, gint threshold,
                   GList **LSP, bit_stream *stream)
{
    GList *cur;
    gint bit, coeff;

    threshold /= 2;

    if (!threshold)
        return TRUE;

    cur = *LSP;

    while (cur != NULL)
    {
        if (read_bit (stream, &bit) != TRUE)
            return FALSE;

        coeff = dwt[GPOINTER_TO_INT (cur->data)];

        if (coeff > 0)
        {
            coeff -= (threshold - threshold / 2);
            if (bit == 1)
                coeff += threshold;
        }
        else
        {
            coeff += (threshold - threshold / 2);
            if (bit == 1)
                coeff -= threshold;
        }

        dwt[GPOINTER_TO_INT (cur->data)] = coeff;

        cur = cur->next;
    }

    return TRUE;
}

static gint
spiht_encode (gint *dwt, gint length, guint8 *buffer,
              gint buffer_size)
{
    GList *LIP, *LSP, *LIS;
    bit_stream stream;
    gint threshold, rc;
    gint *map;

    LIP = LSP = LIS = NULL;

    map = (gint *) g_malloc (length * sizeof (gint));
    make_zeromap (dwt, map, length);

    init_write_bits (&stream, buffer + 1, buffer_size - 1);

    threshold = initial_threshold (dwt, length);

    if (threshold > 0)
        buffer[0] = g_bit_storage (threshold);
    else
        buffer[0] = 0;

    spiht_init (&LIP, &LIS, length);

    while (threshold > 0)
    {
        rc = significance_encode (dwt, map, length, threshold,
                                  &LIP, &LSP, &LIS, &stream);

        if (rc != TRUE)
            break;

        rc = refinement_encode (dwt, threshold, &LSP, &stream);

        if (rc != TRUE)
            break;

        threshold >>= 1;
    }

    flush_bits (&stream);
    g_free (map);

    g_list_free (LIP);
    g_list_free (LSP);
    g_list_free (LIS);

    return (stream.next_byte - stream.first_byte + 1);
}

static void
spiht_decode (gint *dwt, gint length, guint8 *buffer,
              gint buffer_size)
{
    GList *LIP, *LSP, *LIS;
    bit_stream stream;
    gint threshold, rc;
    gint bits;

    LIP = LSP = LIS = NULL;

    init_read_bits (&stream, buffer + 1, buffer_size - 1);
    memset (dwt, 0, length * sizeof (gint));

    bits = buffer[0];

    if (bits > 0)
        threshold = 1 << (bits - 1);
    else
        threshold = 0;

    spiht_init (&LIP, &LIS, length);

    while (threshold > 0)
    {
        rc = significance_decode (dwt, length, threshold,
                                  &LIP, &LSP, &LIS, &stream);

        if (rc != TRUE)
            break;

        rc = refinement_decode (dwt, threshold, &LSP, &stream);

        if (rc != TRUE)
            break;

        threshold >>= 1;
    }

    g_list_free (LIP);
    g_list_free (LSP);
    g_list_free (LIS);
}

static void
round_signal (gdouble *input_signal, gint *output_signal,
              gint signal_length)
{
    gint i;

    for (i = 0; i < signal_length; i++)
        output_signal[i] = CLAMP (input_signal[i], G_MININT, G_MAXINT);
}

gint
encode_frame (void *input_buffer, gint input_size,
              guint8 *output_buffer, gint output_size,
              gint input_bits, gint input_endian,
              gint input_sign)
{
    gdouble *input_signal, *output_signal;
    gint signal_length, stream_size;
    gint max_coeff, max_pos;
    gint *dwt;
    gint i;

    g_assert (input_buffer != NULL);
    g_assert (output_buffer != NULL);
    g_assert (input_size > 1);
    g_assert (output_size >= MIN_FRAME_SIZE);
    power_of_two (input_size);

    if (input_bits == FMT_8)
        signal_length = input_size;
    else if (input_bits == FMT_16)
        signal_length = input_size / 2;
    else
        g_assert_not_reached ();

    power_of_two (signal_length);

    input_signal = (gdouble *) g_malloc (signal_length * sizeof (gdouble));
    output_signal = (gdouble *) g_malloc (signal_length * sizeof (gdouble));
    dwt = (gint *) g_malloc (signal_length * sizeof (gint));

    if (input_bits == FMT_8)
    {
        if (input_sign == FMT_U)
        {
            guint8 *sample = input_buffer;

            for (i = 0; i < signal_length; i++)
                input_signal[i] = sample[i] + G_MININT8;
        }
        else if (input_sign == FMT_S)
        {
            gint8 *sample = input_buffer;

            for (i = 0; i < signal_length; i++)
                input_signal[i] = sample[i];
        }
        else
            g_assert_not_reached ();
    }
    else if (input_bits == FMT_16)
    {
        if (input_sign == FMT_U)
        {
            if (input_endian == FMT_LE)
            {
                guint16 *sample = input_buffer;

                for (i = 0; i < signal_length; i++)
                    input_signal[i] = GUINT16_FROM_LE (sample[i]) + G_MININT16;
            }
            else if (input_endian == FMT_BE)
            {
                guint16 *sample = input_buffer;

                for (i = 0; i < signal_length; i++)
                    input_signal[i] = GUINT16_FROM_BE (sample[i]) + G_MININT16;
            }
            else
                g_assert_not_reached ();
        }
        else if (input_sign == FMT_S)
        {
            if (input_endian == FMT_LE)
            {
                gint16 *sample = input_buffer;

                for (i = 0; i < signal_length; i++)
                    input_signal[i] = GINT16_FROM_LE (sample[i]);
            }
            else if (input_endian == FMT_BE)
            {
                gint16 *sample = input_buffer;

                for (i = 0; i < signal_length; i++)
                    input_signal[i] = GINT16_FROM_BE (sample[i]);
            }
            else
                g_assert_not_reached ();
        }
        else
            g_assert_not_reached ();
    }
    else
        g_assert_not_reached ();

    fdwt (input_signal, output_signal, signal_length);
    round_signal (output_signal, dwt, signal_length);

    stream_size =
        spiht_encode (dwt, signal_length, output_buffer, output_size);

    g_free (input_signal);
    g_free (output_signal);
    g_free (dwt);

    return stream_size;
}

void
decode_frame (guint8 *input_buffer, gint input_size,
              void *output_buffer, gint output_size,
              gint input_bits, gint output_endian,
              gint output_sign)
{
    gdouble *input_signal, *output_signal;
    gint signal_length;
    gint max_coeff, max_pos;
    gint *dwt;
    gint i;

    g_assert (input_buffer != NULL);
    g_assert (output_buffer != NULL);
    g_assert (input_size >= MIN_FRAME_SIZE);
    g_assert (output_size > 1);
    power_of_two (output_size);

    if (input_bits == FMT_8)
        signal_length = output_size;
    else if (input_bits == FMT_16)
        signal_length = output_size / 2;
    else
        g_assert_not_reached ();

    power_of_two (signal_length);

    input_signal = (gdouble *) g_malloc (signal_length * sizeof (gdouble));
    output_signal = (gdouble *) g_malloc (signal_length * sizeof (gdouble));
    dwt = (gint *) g_malloc (signal_length * sizeof (gint));

    spiht_decode (dwt, signal_length, input_buffer, input_size);

    for (i = 0; i < signal_length; i++)
        input_signal[i] = dwt[i];

    idwt (input_signal, output_signal, signal_length);

    if (input_bits == FMT_8)
    {
        if (output_sign == FMT_U)
        {
            guint8 *sample = output_buffer;
            gdouble temp;

            for (i = 0; i < signal_length; i++)
            {
                temp = output_signal[i] - G_MININT8;
                sample[i] = CLAMP (temp, 0, G_MAXUINT8);
            }
        }
        else if (output_sign == FMT_S)
        {
            gint8 *sample = output_buffer;

            for (i = 0; i < signal_length; i++)
                sample[i] = CLAMP (output_signal[i], G_MININT8, G_MAXINT8);
        }
        else
            g_assert_not_reached ();
    }
    else if (input_bits == FMT_16)
    {
        if (output_sign == FMT_U)
        {
            if (output_endian == FMT_LE)
            {
                guint16 *sample = output_buffer;
                gdouble temp;

                for (i = 0; i < signal_length; i++)
                {
                    temp = output_signal[i] - G_MININT16;
                    sample[i] = CLAMP (temp, 0, G_MAXUINT16);
                    sample[i] = GUINT16_TO_LE (sample[i]);
                }
            }
            else if (output_endian == FMT_BE)
            {
                guint16 *sample = output_buffer;
                gdouble temp;

                for (i = 0; i < signal_length; i++)
                {
                    temp = output_signal[i] - G_MININT16;
                    sample[i] = CLAMP (temp, 0, G_MAXUINT16);
                    sample[i] = GUINT16_TO_BE (sample[i]);
                }
            }
            else
                g_assert_not_reached ();
        }
        else if (output_sign == FMT_S)
        {
            if (output_endian == FMT_LE)
            {
                gint16 *sample = output_buffer;

                for (i = 0; i < signal_length; i++)
                {
                    sample[i] = CLAMP (output_signal[i], G_MININT16, G_MAXINT16);
                    sample[i] = GINT16_TO_LE (sample[i]);
                }
            }
            else if (output_endian == FMT_BE)
            {
                gint16 *sample = output_buffer;

                for (i = 0; i < signal_length; i++)
                {
                    sample[i] = CLAMP (output_signal[i], G_MININT16, G_MAXINT16);
                    sample[i] = GINT16_TO_BE (sample[i]);
                }
            }
            else
                g_assert_not_reached ();
        }
        else
            g_assert_not_reached ();
    }
    else
        g_assert_not_reached ();

    g_free (input_signal);
    g_free (output_signal);
    g_free (dwt);
}

static void
smooth_edge_s8 (gint8 *signal_1, gint8 *signal_2,
                gint signal_length, gint smooth_factor)
{
    gint8 *win1, *win2;
    gint sum = 0;
    gint i;

    g_assert (smooth_factor <= signal_length);

    win1 = (gint8 *) g_alloca (2 * smooth_factor * sizeof (gint8));
    win2 = (gint8 *) g_alloca (2 * smooth_factor * sizeof (gint8));

    g_memmove (win1, signal_1 + signal_length - smooth_factor,
               smooth_factor * sizeof (gint8));
    g_memmove (win1 + smooth_factor, signal_2,
               smooth_factor * sizeof (gint8));
    g_memmove (win2, win1,
               2 * smooth_factor * sizeof (gint8));

    for (i = 0; i < smooth_factor; i++)
        sum += win1[i];

    for (i = 0; i < smooth_factor; i++)
    {
        win2[i + smooth_factor / 2] = sum / smooth_factor;
        sum -= win1[i];
        sum += win1[i + smooth_factor];
    }

    g_memmove (signal_1 + signal_length - smooth_factor, win2,
               smooth_factor * sizeof (gint8));
    g_memmove (signal_2, win2 + smooth_factor,
               smooth_factor * sizeof (gint8));
}

static void
smooth_edge_u8 (guint8 *signal_1, guint8 *signal_2,
                gint signal_length, gint smooth_factor)
{
    guint8 *win1, *win2;
    gint sum = 0;
    gint i;

    g_assert (smooth_factor <= signal_length);

    win1 = (guint8 *) g_alloca (2 * smooth_factor * sizeof (guint8));
    win2 = (guint8 *) g_alloca (2 * smooth_factor * sizeof (guint8));

    g_memmove (win1, signal_1 + signal_length - smooth_factor,
               smooth_factor * sizeof (guint8));
    g_memmove (win1 + smooth_factor, signal_2,
               smooth_factor * sizeof (guint8));
    g_memmove (win2, win1,
               2 * smooth_factor * sizeof (guint8));

    for (i = 0; i < smooth_factor; i++)
        sum += win1[i];

    for (i = 0; i < smooth_factor; i++)
    {
        win2[i + smooth_factor / 2] = sum / smooth_factor;
        sum -= win1[i];
        sum += win1[i + smooth_factor];
    }

    g_memmove (signal_1 + signal_length - smooth_factor, win2,
               smooth_factor * sizeof (guint8));
    g_memmove (signal_2, win2 + smooth_factor,
               smooth_factor * sizeof (guint8));
}

static void
smooth_edge_s16le (gint16 *signal_1, gint16 *signal_2,
                   gint signal_length, gint smooth_factor)
{
    gint16 *win1, *win2;
    gint sum = 0;
    gint i;

    g_assert (smooth_factor <= signal_length);

    win1 = (gint16 *) g_alloca (2 * smooth_factor * sizeof (gint16));
    win2 = (gint16 *) g_alloca (2 * smooth_factor * sizeof (gint16));

    g_memmove (win1, signal_1 + signal_length - smooth_factor,
               smooth_factor * sizeof (gint16));
    g_memmove (win1 + smooth_factor, signal_2,
               smooth_factor * sizeof (gint16));
    g_memmove (win2, win1,
               2 * smooth_factor * sizeof (gint16));

    for (i = 0; i < smooth_factor; i++)
        sum += GINT16_FROM_LE (win1[i]);

    for (i = 0; i < smooth_factor; i++)
    {
        win2[i + smooth_factor / 2] = GINT16_TO_LE (sum / smooth_factor);
        sum -= GINT16_FROM_LE (win1[i]);
        sum += GINT16_FROM_LE (win1[i + smooth_factor]);
    }

    g_memmove (signal_1 + signal_length - smooth_factor, win2,
               smooth_factor * sizeof (gint16));
    g_memmove (signal_2, win2 + smooth_factor,
               smooth_factor * sizeof (gint16));
}

static void
smooth_edge_s16be (gint16 *signal_1, gint16 *signal_2,
                   gint signal_length, gint smooth_factor)
{
    gint16 *win1, *win2;
    gint sum = 0;
    gint i;

    g_assert (smooth_factor <= signal_length);

    win1 = (gint16 *) g_alloca (2 * smooth_factor * sizeof (gint16));
    win2 = (gint16 *) g_alloca (2 * smooth_factor * sizeof (gint16));

    g_memmove (win1, signal_1 + signal_length - smooth_factor,
               smooth_factor * sizeof (gint16));
    g_memmove (win1 + smooth_factor, signal_2,
               smooth_factor * sizeof (gint16));
    g_memmove (win2, win1,
               2 * smooth_factor * sizeof (gint16));

    for (i = 0; i < smooth_factor; i++)
        sum += GINT16_FROM_BE (win1[i]);

    for (i = 0; i < smooth_factor; i++)
    {
        win2[i + smooth_factor / 2] = GINT16_TO_BE (sum / smooth_factor);
        sum -= GINT16_FROM_BE (win1[i]);
        sum += GINT16_FROM_BE (win1[i + smooth_factor]);
    }

    g_memmove (signal_1 + signal_length - smooth_factor, win2,
               smooth_factor * sizeof (gint16));
    g_memmove (signal_2, win2 + smooth_factor,
               smooth_factor * sizeof (gint16));
}

static void
smooth_edge_u16le (guint16 *signal_1, guint16 *signal_2,
                   gint signal_length, gint smooth_factor)
{
    guint16 *win1, *win2;
    gint sum = 0;
    gint i;

    g_assert (smooth_factor <= signal_length);

    win1 = (guint16 *) g_alloca (2 * smooth_factor * sizeof (guint16));
    win2 = (guint16 *) g_alloca (2 * smooth_factor * sizeof (guint16));

    g_memmove (win1, signal_1 + signal_length - smooth_factor,
               smooth_factor * sizeof (guint16));
    g_memmove (win1 + smooth_factor, signal_2,
               smooth_factor * sizeof (guint16));
    g_memmove (win2, win1,
               2 * smooth_factor * sizeof (guint16));

    for (i = 0; i < smooth_factor; i++)
        sum += GUINT16_FROM_LE (win1[i]);

    for (i = 0; i < smooth_factor; i++)
    {
        win2[i + smooth_factor / 2] = GUINT16_TO_LE (sum / smooth_factor);
        sum -= GUINT16_FROM_LE (win1[i]);
        sum += GUINT16_FROM_LE (win1[i + smooth_factor]);
    }

    g_memmove (signal_1 + signal_length - smooth_factor, win2,
               smooth_factor * sizeof (gint16));
    g_memmove (signal_2, win2 + smooth_factor,
               smooth_factor * sizeof (gint16));
}

static void
smooth_edge_u16be (guint16 *signal_1, guint16 *signal_2,
                   gint signal_length, gint smooth_factor)
{
    guint16 *win1, *win2;
    gint sum = 0;
    gint i;

    g_assert (smooth_factor <= signal_length);

    win1 = (guint16 *) g_alloca (2 * smooth_factor * sizeof (guint16));
    win2 = (guint16 *) g_alloca (2 * smooth_factor * sizeof (guint16));

    g_memmove (win1, signal_1 + signal_length - smooth_factor,
               smooth_factor * sizeof (guint16));
    g_memmove (win1 + smooth_factor, signal_2,
               smooth_factor * sizeof (guint16));
    g_memmove (win2, win1,
               2 * smooth_factor * sizeof (guint16));

    for (i = 0; i < smooth_factor; i++)
        sum += GUINT16_FROM_BE (win1[i]);

    for (i = 0; i < smooth_factor; i++)
    {
        win2[i + smooth_factor / 2] = GUINT16_TO_BE (sum / smooth_factor);
        sum -= GUINT16_FROM_BE (win1[i]);
        sum += GUINT16_FROM_BE (win1[i + smooth_factor]);
    }

    g_memmove (signal_1 + signal_length - smooth_factor, win2,
               smooth_factor * sizeof (gint16));
    g_memmove (signal_2, win2 + smooth_factor,
               smooth_factor * sizeof (gint16));
}

void
smooth_edge (void *signal_1, void *signal_2, gint signal_length,
             gint smooth_factor, gint bits, gint endian, gint sign)
{
    g_assert (smooth_factor <= signal_length);

    if (bits == FMT_8)
    {
        if (sign == FMT_U)
        {
            smooth_edge_u8 (signal_1, signal_2,
                            signal_length, smooth_factor);
        }
        else if (sign == FMT_S)
        {
            smooth_edge_s8 (signal_1, signal_2,
                            signal_length, smooth_factor);
        }
        else
            g_assert_not_reached ();
    }
    else if (bits == FMT_16)
    {
        if (sign == FMT_U)
        {
            if (endian == FMT_LE)
            {
                smooth_edge_u16le (signal_1, signal_2,
                                   signal_length, smooth_factor);
            }
            else if (endian == FMT_BE)
            {
                smooth_edge_u16be (signal_1, signal_2,
                                   signal_length, smooth_factor);
            }
            else
                g_assert_not_reached ();
        }
        else if (sign == FMT_S)
        {
            if (endian == FMT_LE)
            {
                smooth_edge_s16le (signal_1, signal_2,
                                   signal_length, smooth_factor);
            }
            else if (endian == FMT_BE)
            {
                smooth_edge_s16be (signal_1, signal_2,
                                   signal_length, smooth_factor);
            }
            else
                g_assert_not_reached ();
        }
        else
            g_assert_not_reached ();
    }
    else
        g_assert_not_reached ();
}
