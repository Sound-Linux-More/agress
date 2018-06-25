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

#ifndef __AGRESS_H__
#define __AGRESS_H__

#include <glib/gtypes.h>

G_BEGIN_DECLS

#define FMT_U			0x00
#define FMT_S			0x01
#define FMT_8			0x00
#define FMT_16			0x01
#define FMT_LE			0x00
#define FMT_BE			0x01

gint
encode_frame (void *input_buffer, gint input_size,
	      guint8 *output_buffer, gint output_size,
	      gint input_bits, gint input_endian,
	      gint input_sign);
void
decode_frame (guint8 *input_buffer, gint input_size,
	      void *output_buffer, gint output_size,
	      gint input_bits, gint output_endian,
	      gint output_sign);
void
smooth_edge (void *signal_1, void *signal_2, gint signal_length,
	     gint smooth_factor, gint bits, gint endian, gint sign);

G_END_DECLS

#endif /* __AGRESS_H__ */
