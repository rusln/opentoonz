#if defined _WIN32 && !defined NOMINMAX
#define NOMINMAX
#endif

#include <sstream> /* std::ostringstream */
#include "tfxparam.h"
#include "stdfx.h"

#include "ino_common.h"
//------------------------------------------------------------
class ino_median_filter : public TStandardRasterFx
{
	FX_PLUGIN_DECLARATION(ino_median_filter)
	TRasterFxPort m_input;
	TRasterFxPort m_refer;

	TDoubleParamP m_radius;
	TIntEnumParamP m_channel;

	TIntEnumParamP m_ref_mode;

public:
	ino_median_filter()
		: m_radius(1.7), m_channel(new TIntEnumParam(0, "Red")), m_ref_mode(new TIntEnumParam(0, "Red"))
	{
		addInputPort("Source", this->m_input);
		addInputPort("Reference", this->m_refer);

		bindParam(this, "radius", this->m_radius);
		bindParam(this, "channel", this->m_channel);
		bindParam(this, "reference", this->m_ref_mode);

		this->m_radius->setValueRange(0, 1000.0);
		this->m_channel->addItem(1, "Green");
		this->m_channel->addItem(2, "Blue");
		this->m_channel->addItem(3, "Alpha");
		this->m_channel->addItem(4, "All");

		this->m_ref_mode->addItem(1, "Green");
		this->m_ref_mode->addItem(2, "Blue");
		this->m_ref_mode->addItem(3, "Alpha");
		this->m_ref_mode->addItem(4, "Luminance");
		this->m_ref_mode->addItem(-1, "Nothing");
	}
	bool doGetBBox(
		double frame, TRectD &bBox, const TRenderSettings &info)
	{
		if (this->m_input.isConnected()) {
			const bool ret = this->m_input->doGetBBox(frame, bBox, info);
			const double margin = ceil(this->m_radius->getValue(frame));
			if (0.0 < margin) {
				bBox = bBox.enlarge(margin);
			}
			return ret;
		} else {
			bBox = TRectD();
			return false;
		}
	}
	bool canHandle(const TRenderSettings &rend_sets, double frame)
	{
		return true;
	}
	int getMemoryRequirement(
		const TRectD &rect, double frame, const TRenderSettings &info)
	{
		const double radius = this->m_radius->getValue(frame);
		return TRasterFx::memorySize(
			rect.enlarge(ceil(radius) + 0.5), info.m_bpp);
	}
	void doCompute(
		TTile &tile, double frame, const TRenderSettings &rend_sets);
};
FX_PLUGIN_IDENTIFIER(ino_median_filter, "inoMedianFilterFx")
//------------------------------------------------------------
#include "igs_median_filter.h"
namespace
{
void fx_(
	const TRasterP in_ras // with margin
	,
	const int margin, TRasterP out_ras // no margin

	,
	const TRasterP refer_ras, const int ref_mode

	,
	const double radius, const int channel)
{
	TRasterGR8P out_gr8(
		in_ras->getLy(), in_ras->getLx() * ino::channels() *
							 ((TRaster64P)in_ras ? sizeof(unsigned short) : sizeof(unsigned char)));
	out_gr8->lock();

	igs::median_filter::convert(
		in_ras->getRawData() // BGRA
		,
		out_gr8->getRawData()

			,
		in_ras->getLy(), in_ras->getLx(), ino::channels(), ino::bits(in_ras)

															   ,
		(((0 <= ref_mode) && refer_ras) ? refer_ras->getRawData() : 0) // BGRA
		,
		(((0 <= ref_mode) && refer_ras) ? ino::bits(refer_ras) : 0), ref_mode

		,
		channel, radius, 0 /* 0=Spread:外は淵のピクセル値が続いているとする */
		);

	ino::arr_to_ras(
		out_gr8->getRawData(), ino::channels(), out_ras, margin);
	out_gr8->unlock();
}
}
//------------------------------------------------------------
void ino_median_filter::doCompute(
	TTile &tile, double frame, const TRenderSettings &rend_sets)
{
	/* ------ 接続していなければ処理しない -------------------- */
	if (!this->m_input.isConnected()) {
		tile.getRaster()->clear(); /* 塗りつぶしクリア */
		return;
	}

	/* ------ サポートしていないPixelタイプはエラーを投げる --- */
	if (!((TRaster32P)tile.getRaster()) &&
		!((TRaster64P)tile.getRaster())) {
		throw TRopException("unsupported input pixel type");
	}

	/* ------ Pixel単位で動作パラメータを得る ----------------- */
	/* 動作パラメータを得る */
	const double radius = this->m_radius->getValue(frame);
	const int channel = this->m_channel->getValue();
	const int ref_mode = this->m_ref_mode->getValue();

	/* ------ 参照マージン含めた画像生成 ---------------------- */
	/* Rendering画像のBBox値 --> Pixel単位のdouble値 */
	/*	typedef TRectT<double> TRectD;
		inline TRectT<double>::TRectT(
			const TPointT<double> &bottomLeft
			, const TDimensionT<double> &d
		)	: x0(bottomLeft.x)
			, y0(bottomLeft.y)
			, x1(bottomLeft.x+d.lx)
			, y1(bottomLeft.y+d.ly)
		{}
	*/
	TRectD enlarge_rect = TRectD(
		tile.m_pos /* TPointD */
		,
		TDimensionD(/* int --> doubleにしてセット */
					tile.getRaster()->getLx(), tile.getRaster()->getLy()));
	/* BBoxの拡大 Pixel単位 */
	const int enlarge_pixel = (int)(ceil(radius) + 0.5);
	enlarge_rect = enlarge_rect.enlarge(enlarge_pixel);

	/*	void TRasterFx::allocateAndCompute(
			TTile &tile, 
			const TPointD &pos, const TDimension &size, 
			const TRasterP &templateRas, double frame,
			const TRenderSettings &info
		) {
			if (templateRas) {
		tile.getRaster() = templateRas->create(size.lx, size.ly);
			} else {
		tile.getRaster() = TRaster32P(size.lx, size.ly);
			}
		tile.m_pos = pos;
			compute(tile, frame, info);
		}
	*/
	TTile enlarge_tile;
	this->m_input->allocateAndCompute(
		enlarge_tile, enlarge_rect.getP00(), TDimensionI(/* Pixel単位のdouble -> intにしてセット */
														 (int)(enlarge_rect.getLx() + 0.5), (int)(enlarge_rect.getLy() + 0.5)),
		tile.getRaster(), frame, rend_sets);

	/*------ 参照画像生成 --------------------------------------*/
	TTile reference_tile;
	bool reference_sw = false;
	if (this->m_refer.isConnected()) {
		reference_sw = true;
		this->m_refer->allocateAndCompute(
			reference_tile, enlarge_tile.m_pos, TDimensionI(/* Pixel単位 */
															enlarge_tile.getRaster()->getLx(), enlarge_tile.getRaster()->getLy()),
			enlarge_tile.getRaster(), frame, rend_sets);
	}
	/* ------ 保存すべき画像メモリを塗りつぶしクリア ---------- */
	tile.getRaster()->clear(); /* 塗りつぶしクリア */

	/* ------ (app_begin)log記憶 ------------------------------ */
	const bool log_sw = ino::log_enable_sw();

	if (log_sw) {
		std::ostringstream os;
		os << "params"
		   << "  radius " << radius
		   << "  channel " << channel
		   << "  ref_mode " << ref_mode
		   << "   tile w " << tile.getRaster()->getLx()
		   << "  h " << tile.getRaster()->getLy()
		   << "  pixbits " << ino::pixel_bits(tile.getRaster())
		   << "   frame " << frame
		   << "   rand_sets affine_det " << rend_sets.m_affine.det()
		   << "  shrink x " << rend_sets.m_shrinkX
		   << "  y " << rend_sets.m_shrinkY;
		if (reference_sw) {
			os
				<< "  reference_tile.m_pos " << reference_tile.m_pos
				<< "  reference_tile_getLx " << reference_tile.getRaster()->getLx()
				<< "  y " << reference_tile.getRaster()->getLy();
		}
	}
	/* ------ fx処理 ------------------------------------------ */
	try {
		tile.getRaster()->lock();
		const TRasterP refer_ras = (reference_sw ? reference_tile.getRaster() : nullptr);
		fx_(
			enlarge_tile.getRaster() // in with margin
			,
			enlarge_pixel // margin
			,
			tile.getRaster() // out with no margin

			,
			refer_ras, ref_mode

			,
			radius, channel);
		tile.getRaster()->unlock();
	}
	/* ------ error処理 --------------------------------------- */
	catch (std::bad_alloc &e) {
		tile.getRaster()->unlock();
		if (log_sw) {
			std::string str("std::bad_alloc <");
			str += e.what();
			str += '>';
		}
		throw;
	} catch (std::exception &e) {
		tile.getRaster()->unlock();
		if (log_sw) {
			std::string str("exception <");
			str += e.what();
			str += '>';
		}
		throw;
	} catch (...) {
		tile.getRaster()->unlock();
		if (log_sw) {
			std::string str("other exception");
		}
		throw;
	}
}
