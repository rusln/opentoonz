

#include "toonz/txshpalettelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzscene.h"
#include "tconvert.h"
#include "tstream.h"
#include "tfilepath_io.h"
#include "tsystem.h"

//-----------------------------------------------------------------------------

DEFINE_CLASS_CODE(TXshPaletteLevel, 52)

PERSIST_IDENTIFIER(TXshPaletteLevel, "paletteLevel")

//=============================================================================
// TXshPaletteLevel

TXshPaletteLevel::TXshPaletteLevel(wstring name)
	: TXshLevel(m_classCode, name), m_palette(0)
{
	m_type = PLT_XSHLEVEL;
}

//-----------------------------------------------------------------------------

TXshPaletteLevel::~TXshPaletteLevel()
{
}

//-----------------------------------------------------------------------------

TPalette *TXshPaletteLevel::getPalette() const
{
	return m_palette;
}

//-----------------------------------------------------------------------------

void TXshPaletteLevel::setPalette(TPalette *palette)
{
	if (m_palette != palette) {
		if (m_palette)
			m_palette->release();
		m_palette = palette;
		if (m_palette)
			m_palette->addRef();
	}
}

//-----------------------------------------------------------------------------

void TXshPaletteLevel::setPath(const TFilePath &path)
{
	m_path = path;
}

//-----------------------------------------------------------------------------

void TXshPaletteLevel::loadData(TIStream &is)
{
	string tagName;
	while (is.matchTag(tagName)) {
		if (tagName == "name") {
			wstring name;
			is >> name;
			setName(name);
		} else if (tagName == "path") {
			is >> m_path;
		} else {
			throw TException("TXshPaletteLevel, unknown tag: " + tagName);
		}
		is.closeChild();
	}
}

//-----------------------------------------------------------------------------

void TXshPaletteLevel::saveData(TOStream &os)
{
	os.child("path") << m_path;
	os.child("name") << getName();
}

//-----------------------------------------------------------------------------

void TXshPaletteLevel::load()
{
	TFilePath path = getScene()->decodeFilePath(m_path);
	if (TSystem::doesExistFileOrLevel(path)) {
		TFileStatus fs(path);
		TPersist *p = 0;
		TIStream is(path);

		if (is && fs.doesExist()) {
			is >> p;
			TPalette *palette = dynamic_cast<TPalette *>(p);
			palette->setPaletteName(path.getWideName());
			setPalette(palette);
		}
		assert(m_palette);
	}
}

//-----------------------------------------------------------------------------

void TXshPaletteLevel::save()
{
	TFilePath path = getScene()->decodeFilePath(m_path);
	if (TSystem::doesExistFileOrLevel(path) && m_palette) {
		TOStream os(path);
		os << m_palette;
	}
}

//-----------------------------------------------------------------------------

int TXshPaletteLevel::getFrameCount() const
{
	return 0;
}
