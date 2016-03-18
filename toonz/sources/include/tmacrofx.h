

#ifndef TMACROFX_INCLUDED
#define TMACROFX_INCLUDED

#include "trasterfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef WIN32
#pragma warning(disable : 4251)
#endif

//===================================================================

class DVAPI TMacroFx : public TRasterFx
{

	FX_DECLARATION(TMacroFx)

	TRasterFxP m_root;
	vector<TFxP> m_fxs;
	bool m_isEditing;

	bool isaLeaf(TFx *fx) const;

public:
	static bool analyze(const vector<TFxP> &fxs,
						TFxP &root,
						vector<TFxP> &roots,
						vector<TFxP> &leafs);

	static bool analyze(const vector<TFxP> &fxs);

	static TMacroFx *create(const vector<TFxP> &fxs);

	TMacroFx();
	~TMacroFx();

	TFx *clone(bool recursive = true) const;

	bool doGetBBox(double frame, TRectD &bBox, const TRenderSettings &info);
	void doDryCompute(TRectD &rect,
					  double frame,
					  const TRenderSettings &info);
	void doCompute(TTile &tile, double frame, const TRenderSettings &ri);

	TFxTimeRegion getTimeRegion() const;

	string getPluginId() const;

	void setRoot(TFx *root);
	TFx *getRoot() const;
	//!Returns the Fx identified by \b id.
	//!Returns 0 if the macro does not contains an Fx with fxId equals ti \b id.
	TFx *getFxById(const wstring &id) const;

	// restituisce un riferimento al vettore contenente gli effetti contenuti nel macroFx
	const vector<TFxP> &getFxs() const;

	string getMacroFxType() const;

	bool canHandle(const TRenderSettings &info, double frame);

	string getAlias(double frame, const TRenderSettings &info) const;

	void loadData(TIStream &is);
	void saveData(TOStream &os);
	bool isEditing() { return m_isEditing; }
	void editMacro(bool edit) { m_isEditing = edit; }

	void compatibilityTranslatePort(int majorVersion, int minorVersion, std::string &portName);

private:
	// non implementati
	TMacroFx(const TMacroFx &);
	void operator=(const TMacroFx &);
};

//-------------------------------------------------------------------

#endif
