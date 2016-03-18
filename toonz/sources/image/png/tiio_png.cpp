

#if _MSC_VER >= 1400
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "tmachine.h"
#include "texception.h"
#include "tfilepath.h"
#include "tiio_png.h"
#include "tiio.h"
#include "../compatibility/tfile_io.h"

#define PNG_INTERNAL
#include "lpng124/png.h"

#include "tpixel.h"

// see http://www.libpng.org/pub/png/libpng-1.2.5-manual.html
using namespace std;
//------------------------------------------------------------

extern "C" {
void tnz_abort_handler(void)
{
	//throw "PNG error";
}

void tnz_error_fun(png_structp pngPtr, png_const_charp error_message)
{
	pngPtr->m_canDelete = 0;
	//throw error_message;
}
}

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

	//=========================================================

	inline USHORT
	mySwap(USHORT val)
{
#if TNZ_LITTLE_ENDIAN
	return ((val) | ((val & 0xff) << 8)); //((val>>8)|((val&0xff)<<8)); (vecchio codice)
#else
	return val;
#endif
}

//=========================================================

class PngReader : public Tiio::Reader
{
	FILE *m_chan;
	png_structp m_png_ptr;
	png_infop m_info_ptr, m_end_info_ptr;
	int m_bit_depth, m_color_type, m_interlace_type;
	int m_compression_type, m_filter_type;
	unsigned int m_sig_read;
	int m_y;
	bool m_is16bitEnabled;
	unsigned char *m_rowBuffer;
	unsigned char *m_tempBuffer; //Buffer temporaneo
public:
	PngReader()
		: m_chan(0), m_png_ptr(0), m_info_ptr(0), m_end_info_ptr(0), m_bit_depth(0), m_color_type(0), m_interlace_type(0), m_compression_type(0), m_filter_type(0), m_sig_read(0), m_y(0), m_is16bitEnabled(true), m_rowBuffer(0), m_tempBuffer(0)
	{
	}

	~PngReader()
	{
		if (m_png_ptr && m_png_ptr->m_canDelete == 1) //se la png e' corrotta, liberarla faceva crashare in release!
		{
			try {
				png_read_end(m_png_ptr, m_end_info_ptr);
			} catch (...) {
			}
			try {
				png_destroy_read_struct(&m_png_ptr, &m_info_ptr,
										&m_end_info_ptr);
			} catch (...) {
			}
		} //da quando leggiamo solo le info ho errori di tutti i tipi
		delete[] m_rowBuffer;
		delete[] m_tempBuffer;
	}

	virtual bool read16BitIsEnabled() const { return m_is16bitEnabled; }
	virtual void enable16BitRead(bool enabled) { m_is16bitEnabled = enabled; }

	void readLineInterlace(char *buffer) { readLineInterlace(buffer, 0, m_info.m_lx - 1, 1); }
	void readLineInterlace(short *buffer) { readLineInterlace(buffer, 0, m_info.m_lx - 1, 1); }

	void open(FILE *file)
	{
		try {
			m_chan = file;
		} catch (...) {
			perror("uffa");
			return;
		}

		unsigned char signature[8]; // da 1 a 8 bytes
		fread(signature, 1, sizeof signature, m_chan);
		bool isPng = !png_sig_cmp(signature, 0, sizeof signature);
		if (!isPng)
			return;

		m_png_ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING, 0, tnz_error_fun, 0);
		if (!m_png_ptr)
			return;
		m_info_ptr = png_create_info_struct(m_png_ptr);
		if (!m_info_ptr) {
			png_destroy_read_struct(&m_png_ptr,
									(png_infopp)0, (png_infopp)0);
			return;
		}
		m_end_info_ptr = png_create_info_struct(m_png_ptr);
		if (!m_end_info_ptr) {
			png_destroy_read_struct(&m_png_ptr,
									&m_info_ptr, (png_infopp)0);
			return;
		}

		png_init_io(m_png_ptr, m_chan);

		png_set_sig_bytes(m_png_ptr, sizeof signature);

#if defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
		png_set_bgr(m_png_ptr);
		png_set_swap_alpha(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
		png_set_bgr(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
		png_set_swap_alpha(m_png_ptr);
#elif !defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
		@unknow channel order
#endif

		png_read_info(m_png_ptr, m_info_ptr);

		if (png_get_valid(m_png_ptr, m_info_ptr, PNG_INFO_pHYs)) {
			png_uint_32 xdpi = png_get_x_pixels_per_meter(m_png_ptr, m_info_ptr);
			png_uint_32 ydpi = png_get_y_pixels_per_meter(m_png_ptr, m_info_ptr);

			m_info.m_dpix = tround(xdpi * 0.0254);
			m_info.m_dpiy = tround(ydpi * 0.0254);
		}

		int rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);

		if (m_rowBuffer)
			delete[] m_rowBuffer;
		//m_rowBuffer = new unsigned char[rowBytes];

		TUINT32 lx = 0, ly = 0;
		png_get_IHDR(m_png_ptr, m_info_ptr, &lx, &ly, &m_bit_depth, &m_color_type,
					 &m_interlace_type, &m_compression_type, &m_filter_type);
		m_info.m_lx = lx;
		m_info.m_ly = ly;

		m_info.m_bitsPerSample = m_bit_depth;

		int channels = png_get_channels(m_png_ptr, m_info_ptr);
		m_info.m_samplePerPixel = channels;

		if (channels == 1 || channels == 2) {
			if (m_bit_depth < 8) // (m_bit_depth == 1 || m_bit_depth == 2 || m_bit_depth == 4)
				m_rowBuffer = new unsigned char[lx * 3];
			else
				m_rowBuffer = new unsigned char[rowBytes * 4];
		} else {
			m_rowBuffer = new unsigned char[rowBytes];
		}

		//if (m_interlace_type==1)
		//    m_tempBuffer = new unsigned char[ly*rowBytes];

		/* if (m_interlace_type==1)
   {
	   if (channels==1 || channels==2)
	   {
		   if (m_bit_depth < 8)                          // (m_bit_depth == 1 || m_bit_depth == 2 || m_bit_depth == 4)
			   m_tempBuffer = new unsigned char[ly*lx*3];
		   else
			   m_tempBuffer = new unsigned char[ly*rowBytes*4];
       }
       else
       {
	       m_tempBuffer = new unsigned char[ly*rowBytes];
       }     
   }*/

		/*if (m_color_type == PNG_COLOR_TYPE_PALETTE)
	   {
      png_destroy_read_struct(&m_png_ptr,
           &m_info_ptr, (png_infopp)0);
      return;
     }*/

		if (m_color_type == PNG_COLOR_TYPE_PALETTE) {
			m_info.m_valid = true;
			png_set_palette_to_rgb(m_png_ptr);
		}

		if (m_color_type == PNG_COLOR_TYPE_GRAY &&
			m_bit_depth < 8)
			png_set_gray_1_2_4_to_8(m_png_ptr);

		if (png_get_valid(m_png_ptr, m_info_ptr,
						  PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(m_png_ptr);

		if (m_bit_depth == 16 && !m_is16bitEnabled)
			png_set_strip_16(m_png_ptr);

#if defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
		if (m_color_type == PNG_COLOR_TYPE_RGB ||
			m_color_type == PNG_COLOR_TYPE_RGB_ALPHA)
			png_set_bgr(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
		if (m_color_type == PNG_COLOR_TYPE_RGB_ALPHA)
			png_set_swap_alpha(m_png_ptr);
#elif !defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM) && !defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
		@unknow channel order
#endif

		if (m_color_type == PNG_COLOR_TYPE_GRAY ||
			m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(m_png_ptr);
	}

	void readLine(char *buffer, int x0, int x1, int shrink)
	{
		int ly = m_info.m_ly;
		if (!m_tempBuffer) {
			int lx = m_info.m_lx;
			int channels = png_get_channels(m_png_ptr, m_info_ptr);
			int rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);
			if (m_interlace_type == 1) {
				if (channels == 1 || channels == 2) {
					if (m_bit_depth < 8)
						m_tempBuffer = new unsigned char[ly * lx * 3];
					else
						m_tempBuffer = new unsigned char[ly * rowBytes * 4];
				} else {
					m_tempBuffer = new unsigned char[ly * rowBytes];
				}
			}
		}
		int lx = m_info.m_lx;

		if (m_interlace_type == 1 && lx > 4) {
			readLineInterlace(&buffer[0], x0, x1, shrink);
			m_y++;
			if (m_tempBuffer && m_y == ly) {
				delete[] m_tempBuffer;
				m_tempBuffer = 0;
			}
			return;
		}

		int y = m_info.m_ly - 1 - m_y;
		if (y < 0)
			return;
		m_y++;

		png_bytep row_pointer = m_rowBuffer;
		png_read_row(m_png_ptr, row_pointer, NULL);

		writeRow(buffer);

		if (m_tempBuffer && m_y == ly) {
			delete[] m_tempBuffer;
			m_tempBuffer = 0;
		}
	}

	void readLine(short *buffer, int x0, int x1, int shrink)
	{
		int ly = m_info.m_ly;
		if (!m_tempBuffer) {
			int lx = m_info.m_lx;
			int channels = png_get_channels(m_png_ptr, m_info_ptr);
			int rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);
			if (m_interlace_type == 1) {
				if (channels == 1 || channels == 2) {
					if (m_bit_depth < 8) // (m_bit_depth == 1 || m_bit_depth == 2 || m_bit_depth == 4)
						m_tempBuffer = new unsigned char[ly * lx * 3];
					else
						m_tempBuffer = new unsigned char[ly * rowBytes * 4];
				} else {
					m_tempBuffer = new unsigned char[ly * rowBytes];
				}
			}
		}

		if (m_png_ptr->interlaced == 1) {
			readLineInterlace(&buffer[0], x0, x1, shrink);
			m_y++;
			if (m_tempBuffer && m_y == ly) {
				delete[] m_tempBuffer;
				m_tempBuffer = 0;
			}
			return;
		}

		int y = m_info.m_ly - 1 - m_y;
		if (y < 0)
			return;
		m_y++;

		png_bytep row_pointer = m_rowBuffer;
		png_read_row(m_png_ptr, row_pointer, NULL);

		writeRow(buffer);

		if (m_tempBuffer && m_y == ly) {
			delete[] m_tempBuffer;
			m_tempBuffer = 0;
		}
	}

	int skipLines(int lineCount)
	{
		for (int i = 0; i < lineCount; i++) {
			if (m_interlace_type == 1 && m_info.m_lx > 4) // pezza. Studiare il codice
			{
				int channels = png_get_channels(m_png_ptr, m_info_ptr);
				char *lineBuffer = (char *)malloc(4 * m_info.m_lx * sizeof(char));
				readLine(lineBuffer, 0, m_info.m_lx - 1, 1);
				free(lineBuffer);
			} else {
				m_y++;
				png_bytep row_pointer = m_rowBuffer;
				png_read_row(m_png_ptr, row_pointer, NULL);
			}
		}
		return lineCount;
	}

	Tiio::RowOrder getRowOrder() const
	{
		return Tiio::TOP2BOTTOM;
	}

	void writeRow(char *buffer)
	{
		if (m_color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
			m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
			if (m_bit_depth == 16) {
				TPixel32 *pix = (TPixel32 *)buffer;
				int i = -2;
				for (int j = 0; j < m_info.m_lx; j++) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
					pix[j].m = m_rowBuffer[i = i + 2];
					pix[j].r = m_rowBuffer[i = i + 2];
					pix[j].g = m_rowBuffer[i = i + 2];
					pix[j].b = m_rowBuffer[i = i + 2];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
					pix[j].r = m_rowBuffer[i = i + 2];
					pix[j].g = m_rowBuffer[i = i + 2];
					pix[j].b = m_rowBuffer[i = i + 2];
					pix[j].m = m_rowBuffer[i = i + 2];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
					pix[j].b = m_rowBuffer[i = i + 2];
					pix[j].g = m_rowBuffer[i = i + 2];
					pix[j].r = m_rowBuffer[i = i + 2];
					pix[j].m = m_rowBuffer[i = i + 2];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
					pix[j].m = m_rowBuffer[i = i + 2];
					pix[j].b = m_rowBuffer[i = i + 2];
					pix[j].g = m_rowBuffer[i = i + 2];
					pix[j].r = m_rowBuffer[i = i + 2];
#else
					@unknow channel order
#endif
				}
			} else {
				TPixel32 *pix = (TPixel32 *)buffer;
				int i = 0;
				for (int j = 0; j < m_info.m_lx; j++) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
					pix[j].m = m_rowBuffer[i++];
					pix[j].r = m_rowBuffer[i++];
					pix[j].g = m_rowBuffer[i++];
					pix[j].b = m_rowBuffer[i++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
					pix[j].r = m_rowBuffer[i++];
					pix[j].g = m_rowBuffer[i++];
					pix[j].b = m_rowBuffer[i++];
					pix[j].m = m_rowBuffer[i++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
					pix[j].b = m_rowBuffer[i++];
					pix[j].g = m_rowBuffer[i++];
					pix[j].r = m_rowBuffer[i++];
					pix[j].m = m_rowBuffer[i++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
					pix[j].m = m_rowBuffer[i++];
					pix[j].b = m_rowBuffer[i++];
					pix[j].g = m_rowBuffer[i++];
					pix[j].r = m_rowBuffer[i++];
#else
					@unknow channel order
#endif
				}
			}
		} else //qui gestisce RGB senza alpha.
		{	  // grayscale e' gestito come RGB perche' si usa png_set_gray_to_rgb
			if (m_bit_depth == 16) {
				TPixel32 *pix = (TPixel32 *)buffer;
				int i = -2;
				for (int j = 0; j < m_info.m_lx; j++) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
					pix[j].r = m_rowBuffer[i = i + 2];
					pix[j].g = m_rowBuffer[i = i + 2];
					pix[j].b = m_rowBuffer[i = i + 2];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
					pix[j].b = m_rowBuffer[i = i + 2];
					pix[j].g = m_rowBuffer[i = i + 2];
					pix[j].r = m_rowBuffer[i = i + 2];
#else
					@unknow channel order
#endif
					pix[j].m = 255;
				}
			} else {
				TPixel32 *pix = (TPixel32 *)buffer;
				int i = 0;
				for (int j = 0; j < m_info.m_lx; j++) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
					pix[j].r = m_rowBuffer[i++];
					pix[j].g = m_rowBuffer[i++];
					pix[j].b = m_rowBuffer[i++];
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
					pix[j].b = m_rowBuffer[i++];
					pix[j].g = m_rowBuffer[i++];
					pix[j].r = m_rowBuffer[i++];
#else
					@unknow channel order
#endif
					pix[j].m = 255;
				}
			}
		}
	}

	void writeRow(short *buffer)
	{
		if (m_color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
			m_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
			TPixel64 *pix = (TPixel64 *)buffer;
			int i = -2; //0;
			for (int j = 0; j < m_info.m_lx; j++) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
				pix[j].r = mySwap(m_rowBuffer[i = i + 2]); //i++
				pix[j].g = mySwap(m_rowBuffer[i = i + 2]);
				pix[j].b = mySwap(m_rowBuffer[i = i + 2]);
				pix[j].m = mySwap(m_rowBuffer[i = i + 2]);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
				pix[j].b = mySwap(m_rowBuffer[i = i + 2]); //i++
				pix[j].g = mySwap(m_rowBuffer[i = i + 2]);
				pix[j].r = mySwap(m_rowBuffer[i = i + 2]);
				pix[j].m = mySwap(m_rowBuffer[i = i + 2]);
#else
				@unknow channel order
#endif
				//pix[j].m = 255;
			}
		} else //qui gestisce RGB senza alpha.
		{	  // grayscale e' gestito come RGB perche' si usa png_set_gray_to_rgb
			TPixel64 *pix = (TPixel64 *)buffer;
			int i = -2;
			for (int j = 0; j < m_info.m_lx; j++) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
				pix[j].r = mySwap(m_rowBuffer[i = i + 2]);
				pix[j].g = mySwap(m_rowBuffer[i = i + 2]);
				pix[j].b = mySwap(m_rowBuffer[i = i + 2]);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
				pix[j].b = mySwap(m_rowBuffer[i = i + 2]);
				pix[j].g = mySwap(m_rowBuffer[i = i + 2]);
				pix[j].r = mySwap(m_rowBuffer[i = i + 2]);
#else
				@unknow channel order
#endif
				pix[j].m = mySwap(255);
			}
		}
	}

	void copyPixel(int count, int dstX, int dstDx, int dstY)
	{

		int channels = png_get_channels(m_png_ptr, m_info_ptr);
		int rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);
		int lx = m_info.m_lx;

		if ((channels == 4 || channels == 3) && m_bit_depth == 16) {
			for (int i = 0; i < count; i += 2) {
				for (int j = 0; j < channels * 2; j++) {
					(m_tempBuffer + (dstY * rowBytes))[(i * dstDx + dstX) * channels + j] = m_rowBuffer[i * channels + j];
				}
			}
		} else if (channels == 2 && m_bit_depth == 16) {
			for (int i = 0; i < count; i += 2) {
				for (int j = 0; j < 4 * 2; j++) {
					(m_tempBuffer + (dstY * rowBytes * 4))[(i * dstDx + dstX) * 4 + j] = m_rowBuffer[i * 4 + j];
				}
			}
		} else if (channels == 1 && m_bit_depth == 16) {
			for (int i = 0; i < count; i += 2) {
				for (int j = 0; j < 3 * 2; j++) {
					(m_tempBuffer + (dstY * rowBytes * 4))[(i * dstDx + dstX) * 3 + j] = m_rowBuffer[i * 3 + j];
				}
			}
		} else if (channels == 1 && m_bit_depth == 8) {
			for (int i = 0; i < count; i++) {
				for (int j = 0; j < 3; j++) {
					(m_tempBuffer + (dstY * rowBytes * 4))[(i * dstDx + dstX) * 3 + j] = m_rowBuffer[i * 3 + j];
				}
			}
		} else if (channels == 2 && m_bit_depth == 8) {
			for (int i = 0; i < count; i++) {
				for (int j = 0; j < 4; j++) {
					(m_tempBuffer + (dstY * rowBytes * 4))[(i * dstDx + dstX) * 4 + j] = m_rowBuffer[i * 4 + j];
				}
			}
		} else if ((channels == 1 || channels == 2) && m_bit_depth < 8) {
			for (int i = 0; i < count; i++) {
				for (int j = 0; j < 3; j++) {
					(m_tempBuffer + (dstY * lx * 3))[(i * dstDx + dstX) * 3 + j] = m_rowBuffer[i * 3 + j];
				}
			}
		} else {
			for (int i = 0; i < count; i++) {
				for (int j = 0; j < channels; j++) {
					(m_tempBuffer + (dstY * rowBytes))[(i * dstDx + dstX) * channels + j] = m_rowBuffer[i * channels + j];
				}
			}
		}
	}

	void readLineInterlace(char *buffer, int x0, int x1, int shrink)
	{

		//m_png_ptr->row_number è l'indice di riga che scorre quando chiamo la png_read_row
		int rowNumber = m_png_ptr->row_number;
		//numRows è il numero di righe processate in ogni passo
		int numRows = (m_png_ptr->height) / 8;

		int passPng = m_png_ptr->pass; //conosco il passo d'interlacciamento

		int passRow = 5 + (m_y & 1); //passo desiderato per la riga corrente

		int channels = png_get_channels(m_png_ptr, m_info_ptr);

		int rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);
		png_bytep row_pointer = m_rowBuffer;

		int lx = m_info.m_lx;

		while (passPng <= passRow && rowNumber <= numRows) //finchè il passo d'interlacciamento è minore o uguale
														   // <             //del passo desiderato effettua tante volte le lettura della riga
														   //quant'è il passo desiderato per quella riga
		{
			rowNumber = m_png_ptr->row_number;
			png_read_row(m_png_ptr, row_pointer, NULL);
			numRows = m_png_ptr->num_rows;
			//devo memorizzare la riga letta nel buffer di appoggio in base al passo di riga
			//e al blocchetto di appartenenza della riga

			//copio i pixel che desidero nel buffer temporaneo

			//il membro di PngReader copyPixel deve tener conto della riga processata
			//e del passo che si è effettuato ( e in base a quello so quanti pixel copiare!)

			if (m_bit_depth == 16) {
				if (passPng == 0)
					copyPixel(lx / 4, 0, 8, rowNumber * 8);
				else if (passPng == 1)
					copyPixel(lx / 4, 8, 8, rowNumber * 8);
				else if (passPng == 2)
					copyPixel(lx / 2, 0, 4, rowNumber * 8 + 4);
				else if (passPng == 3)
					copyPixel(lx / 2, 4, 4, rowNumber * 4);
				else if (passPng == 4)
					copyPixel(lx, 0, 2, rowNumber * 4 + 2);
				else if (passPng == 5)
					copyPixel(lx, 2, 2, rowNumber * 2);
				else if (passPng == 6)
					copyPixel(2 * lx, 0, 1, rowNumber * 2 + 1);
			} else {
				if (passPng == 0)
					copyPixel((lx + 7) / 8, 0, 8, rowNumber * 8);
				else if (passPng == 1)
					copyPixel((lx + 3) / 8, 4, 8, rowNumber * 8);
				else if (passPng == 2)
					copyPixel((lx + 3) / 4, 0, 4, rowNumber * 8 + 4);
				else if (passPng == 3)
					copyPixel((lx + 1) / 4, 2, 4, rowNumber * 4);
				else if (passPng == 4)
					copyPixel((lx + 1) / 2, 0, 2, rowNumber * 4 + 2);
				else if (passPng == 5)
					copyPixel(lx / 2, 1, 2, rowNumber * 2);
				else if (passPng == 6)
					copyPixel(lx, 0, 1, rowNumber * 2 + 1);
			}

			passPng = m_png_ptr->pass;
		}

		// fase di copia
		if (channels == 1 || channels == 2) {
			if (m_bit_depth < 8)
				memcpy(m_rowBuffer, m_tempBuffer + ((m_y)*lx * 3), lx * 3);
			else
				memcpy(m_rowBuffer, m_tempBuffer + ((m_y)*rowBytes * 4), rowBytes * 4);
		} else {
			memcpy(m_rowBuffer, m_tempBuffer + ((m_y)*rowBytes), rowBytes);
		}

		// fase di copia vecchia
		//memcpy(m_rowBuffer, m_tempBuffer+((m_y)*rowBytes), rowBytes);

		//tutto quello che segue lo metto in una funzione in cui scrivo il buffer di restituzione della readLine
		//è una funzione comune alle ReadLine
		writeRow(buffer);
	}

	void readLineInterlace(short *buffer, int x0, int x1, int shrink)
	{

		//m_png_ptr->row_number è l'indice di riga che scorre quando chiamo la png_read_row
		int rowNumber = m_png_ptr->row_number;
		//numRows è il numero di righe processate in ogni passo
		int numRows = (m_png_ptr->height) / 8;

		int passPng = m_png_ptr->pass; //conosco il passo d'interlacciamento

		int passRow = 5 + (m_y & 1); //passo desiderato per la riga corrente

		int channels = png_get_channels(m_png_ptr, m_info_ptr);
		int lx = m_info.m_lx;

		int rowBytes = png_get_rowbytes(m_png_ptr, m_info_ptr);
		png_bytep row_pointer = m_rowBuffer;

		while (passPng <= passRow && rowNumber < numRows) //finchè il passo d'interlacciamento è minore o uguale
														  //del passo desiderato effettua tante volte le lettura della riga
														  //quant'è il passo desiderato per quella riga
		{
			rowNumber = m_png_ptr->row_number;
			png_read_row(m_png_ptr, row_pointer, NULL);
			numRows = m_png_ptr->num_rows;
			int lx = m_info.m_lx;

			//devo memorizzare la riga letta nel buffer di appoggio in base al passo di riga
			//e al blocchetto di appartenenza della riga

			//copio i pixel che desidero nel buffer temporaneo

			//il membro di PngReader copyPixel deve tener conto della riga processata
			//e del passo che si è effettuato ( e in base a quello so quanti pixel copiare!)

			int channels = png_get_channels(m_png_ptr, m_info_ptr);

			if (passPng == 0)
				copyPixel(lx / 4, 0, 8, rowNumber * 8);
			else if (passPng == 1)
				copyPixel(lx / 4, 8, 8, rowNumber * 8);
			else if (passPng == 2)
				copyPixel(lx / 2, 0, 4, rowNumber * 8 + 4);
			else if (passPng == 3)
				copyPixel(lx / 2, 4, 4, rowNumber * 4);
			else if (passPng == 4)
				copyPixel(lx, 0, 2, rowNumber * 4 + 2);
			else if (passPng == 5)
				copyPixel(lx, 2, 2, rowNumber * 2);
			else if (passPng == 6)
				copyPixel(2 * lx, 0, 1, rowNumber * 2 + 1);

			passPng = m_png_ptr->pass;
		}

		// fase di copia
		if (channels == 1 || channels == 2) {
			memcpy(m_rowBuffer, m_tempBuffer + ((m_y)*rowBytes * 4), rowBytes * 4);
		} else {
			memcpy(m_rowBuffer, m_tempBuffer + ((m_y)*rowBytes), rowBytes);
		}

		// fase di copia
		//memcpy(m_rowBuffer, m_tempBuffer+((m_y)*rowBytes), rowBytes);

		//tutto quello che segue lo metto in una funzione in cui scrivo il buffer di restituzione della readLine
		//è una funzione comune alle ReadLine
		writeRow(buffer);
	}
};

//=========================================================

Tiio::PngWriterProperties::PngWriterProperties()
	: m_matte("Alpha Channel", true)

{
	bind(m_matte);
}

//=========================================================

class PngWriter : public Tiio::Writer
{
	png_structp m_png_ptr;
	png_infop m_info_ptr;
	FILE *m_chan;
	bool m_matte;
	vector<TPixel> *m_colormap;

public:
	PngWriter();
	~PngWriter();

	void open(FILE *file, const TImageInfo &info);
	void writeLine(char *buffer);
	virtual void writeLine(short *buffer);

	Tiio::RowOrder getRowOrder() const
	{
		return Tiio::TOP2BOTTOM;
	}

	void flush();
	virtual bool write64bitSupported() const { return true; }
};

//---------------------------------------------------------

PngWriter::PngWriter()
	: m_png_ptr(0), m_info_ptr(0), m_matte(true), m_colormap(0)
{
}

//---------------------------------------------------------

PngWriter::~PngWriter()
{
	if (m_png_ptr) {
		png_destroy_write_struct(
			&m_png_ptr,
			&m_info_ptr);
	}
	if (m_chan) {
		fflush(m_chan);
		m_chan = 0;
	}
}

//---------------------------------------------------------

png_color palette[256];
png_byte alpha[1];

void PngWriter::open(FILE *file, const TImageInfo &info)
{
	m_info = info;
	m_png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
										(png_voidp)0, //user_error_ptr,
										0,			  //user_error_fn,
										0);			  //user_warning_fn);
	if (!m_png_ptr)
		return;
	m_info_ptr = png_create_info_struct(m_png_ptr);
	if (!m_info_ptr) {
		png_destroy_write_struct(&m_png_ptr, (png_infopp)0);
		return;
	}

	m_chan = file;
	png_init_io(m_png_ptr, m_chan);

	if (!m_properties)
		m_properties = new Tiio::PngWriterProperties();

	TBoolProperty *alphaProp = (TBoolProperty *)(m_properties->getProperty("Alpha Channel"));
	TPointerProperty *colormap = (TPointerProperty *)(m_properties->getProperty("Colormap"));
	m_matte = (alphaProp && alphaProp->getValue()) ? true : false;
	if (colormap)
		m_colormap = (vector<TPixel> *)colormap->getValue();

	TUINT32 x_pixels_per_meter = tround(m_info.m_dpix / 0.0254);
	TUINT32 y_pixels_per_meter = tround(m_info.m_dpiy / 0.0254);

	if (!m_colormap)
		png_set_IHDR(m_png_ptr, m_info_ptr,
					 m_info.m_lx, m_info.m_ly,
					 info.m_bitsPerSample, m_matte ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
					 PNG_INTERLACE_NONE,
					 PNG_COMPRESSION_TYPE_DEFAULT,
					 PNG_FILTER_TYPE_DEFAULT);
	else {
		png_set_IHDR(m_png_ptr, m_info_ptr,
					 m_info.m_lx, m_info.m_ly,
					 8, PNG_COLOR_TYPE_PALETTE,
					 PNG_INTERLACE_NONE,
					 PNG_COMPRESSION_TYPE_DEFAULT,
					 PNG_FILTER_TYPE_DEFAULT);
		for (unsigned int i = 0; i < m_colormap->size(); i++) {
			/*unsigned char red = (i>>5)&0x7;
			unsigned char green = (i>>2)&0x7;
			unsigned char blue = i&0x3;
			palette[i].red = (red>>1) | (red<<2) | (red<<5);
			palette[i].green = (green>>1) | (green<<2) | (green<<5);
			palette[i].blue = blue | (blue<<2) | (blue<<4) | (blue<<6);
			if (red==0 && green==0) alpha[i] = 0; else if (blue==0 && green==0) alpha[i] = 128; else alpha[i] = 255;*/
			palette[i].red = (*m_colormap)[i].r;
			palette[i].green = (*m_colormap)[i].g;
			palette[i].blue = (*m_colormap)[i].b;
		}

		png_set_PLTE(m_png_ptr, m_info_ptr, palette, m_colormap->size());
	}

//png_set_dither(m_png_ptr, palette, 256, 256, 0, 1);

#if defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
	png_set_bgr(m_png_ptr);
	png_set_swap_alpha(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
	png_set_bgr(m_png_ptr);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
	png_set_swap_alpha(m_png_ptr);
#elif !defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
	@unknow channel order
#endif

	png_write_info(m_png_ptr, m_info_ptr);
	png_write_pHYs(m_png_ptr, x_pixels_per_meter, y_pixels_per_meter, 1);

	if (m_colormap && m_matte) {
		alpha[0] = 0;
		png_write_tRNS(m_png_ptr, alpha, 0, 1, PNG_COLOR_TYPE_PALETTE);
	}
}

//---------------------------------------------------------

void PngWriter::flush()
{
	png_write_end(m_png_ptr, m_info_ptr);
	fflush(m_chan);
}

//---------------------------------------------------------

void PngWriter::writeLine(short *buffer)
{

	{
		TPixel64 *pix = (TPixel64 *)buffer;
		unsigned short *tmp;
		tmp = (unsigned short *)malloc((m_info.m_lx + 1) * 3);
		//unsigned short tmp[10000];
		int k = 0;
		for (int j = 0; j < m_info.m_lx; j++) {
#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
			tmp[k++] = mySwap(pix->r);
			tmp[k++] = mySwap(pix->g);
			tmp[k++] = mySwap(pix->b);
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
			tmp[k++] = mySwap(pix->b);
			tmp[k++] = mySwap(pix->g);
			tmp[k++] = mySwap(pix->r);
#else
			@unknow channel order
#endif
			if (m_matte)
				tmp[k++] = mySwap(pix->m);
			++pix;
		}
		png_write_row(m_png_ptr, (unsigned char *)tmp);
	}
}

//=========================================================

void PngWriter::writeLine(char *buffer)
{
	//TBoolProperty* alphaProp = (TBoolProperty*)(m_properties->getProperty("Alpha Channel"));
	if (m_matte || m_colormap) {
		png_bytep row_pointer = (unsigned char *)buffer;
		png_write_row(m_png_ptr, row_pointer);
	} else {
		unsigned char *tmp = 0;
		TPixel32 *pix = (TPixel32 *)buffer;
		tmp = (unsigned char *)malloc((m_info.m_lx + 1) * 3);

		//     unsigned char tmp[10000];
		int k = 0;
		for (int j = 0; j < m_info.m_lx; j++) {

//tmp = (pix->r&0xe0)|((pix->g&0xe0)>>3) | ((pix->b&0xc0)>>6);

#if defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB) || defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
			tmp[k++] = pix->r;
			tmp[k++] = pix->g;
			tmp[k++] = pix->b;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR) || defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
			tmp[k++] = pix->b;
			tmp[k++] = pix->g;
			tmp[k++] = pix->r;
#else
			@unknow channel order
#endif

			++pix;
		}
		png_write_row(m_png_ptr, tmp);
	}
}

//=========================================================

Tiio::Reader *Tiio::makePngReader()
{
	return new PngReader();
}

Tiio::Writer *Tiio::makePngWriter()
{
	return new PngWriter();
}
