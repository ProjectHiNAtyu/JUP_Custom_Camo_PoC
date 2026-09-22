#include <asmjit/core/operand.h>
#include <asmjit/x86/x86operand.h>
#define PHNT_VERSION PHNT_WIN10_22H2
#include <phnt_windows.h>
#include <phnt.h>
#include <ntexapi.h>
#include <ntpsapi.h>
#include <minidumpapiset.h>

#include <TlHelp32.h>
#include <mmeapi.h>

#include <filesystem>
#include <string.h>
#include <stdio.h>
#include <intrin.h>
#include <filesystem>

#include "libs/minhook/include/MinHook.h"

#include <dxgi1_4.h>
#include "kiero/kiero.h"

#include "imgutils.hpp"
#include "restorentdll.h"
#include "utils.h"
#include "hookutil.h"
#include "memoryutils.h"
#include "systemhooks.h"
#include "exceptions.h"
#include "arxan.h"
#include "instrumentationCallbacks.h"
#include "paths.h"
#include "syscalls.h"
#include "json.hpp"
#include "enumstruct.h"



#include <DbgHelp.h>
#include <fstream>
#include <sstream>
#include <map>
#include <shlobj.h>
#include "zlib.h"
#include <mutex>
#include <vector>
#include <unordered_map>
#include <functional>
#include <random>
#include <chrono>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <regex>
#include <Psapi.h>
#include <memory>

#include <sys/stat.h>

#include "gsc_helper.hpp"


#pragma intrinsic(_ReturnAddress)

#pragma comment(lib, "DbgHelp.lib")



// en : Image Base Address Reference
// ja : イメージベースアドレス参照
const auto _ImageBase = (uintptr_t)GetModuleHandle(nullptr);


// en : Hook source function pointer for various MinHooks (for storage)
// ja : 各種MinHook用フック元関数ポインター（保持用）
hookutil::detour Load_GfxImage_JUP_h;



//++++++++++++++++++++++++++++++
// en : In-game address calculation
// ja : ゲーム内アドレス算出
//++++++++++++++++++++++++++++++
size_t CalcAdr(const size_t val) { return _ImageBase + (val - _adr.DumpBase); }



//++++++++++++++++++++++++++++++
// en : In-game address calculation
// ja : ゲーム内アドレス算出
//++++++++++++++++++++++++++++++
size_t CalcPtr(const size_t val)
{
	if (_adr.DumpBase == 0x140000000)
		return CalcAdr(val);

	return _ImageBase + val;
}


//++++++++++++++++++++++++++++++
// en : Replaces all occurrences of a specified string within a string
// ja : 文字列内に含まれている、指定された文字列を全て置き換える
//++++++++++++++++++++++++++++++
void ReplaceAll(std::string& stringreplace, const std::string& origin, const std::string& dest)
{
	size_t pos = 0;
	size_t offset = 0;
	size_t len = origin.length();
	while ((pos = stringreplace.find(origin, offset)) != std::string::npos)
	{
		stringreplace.replace(pos, len, dest);
		offset = pos + dest.length();
	}
}


//++++++++++++++++++++++++++++++
// en : Check if a specified file exists
// ja : 指定したファイルが存在するかチェックする
//++++++++++++++++++++++++++++++
inline bool file_exists(const char* name)
{
	struct stat buffer;
	return (stat(name, &buffer) == 0);
}


//++++++++++++++++++++++++++++++
// en : Read PNG and DDS images from the target path.
// ja : 対象パスのpng, dds画像を読み取る
//++++++++++++++++++++++++++++++
imgutils::image LoadRawImageFromFile(std::string path, bool isDdsFile)
{
	if (!isDdsFile)
		return imgutils::image::from_file(path);
	else
		return imgutils::image::from_ddsfile(path);
}



//++++++++++++++++++++++++++++++
// en : Find the XAsset header in the database (execute the function)
// ja : データベースからXAssetのヘッダーを探す ( 関数を実行する )
//++++++++++++++++++++++++++++++
XAssetHeader_JUP DB_FindXAssetHeader_JUP_f(int type, const char* given_name, int allow_create_default)
{
	auto func = reinterpret_cast<XAssetHeader_JUP(*)(int type, const char* given_name, int allow_create_default)>(CalcPtr(_adr.DB_FindXAssetHeader));
	return func(type, given_name, allow_create_default);
}



// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //
// Custom camo - START
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //




// en : bit 63 of a hashed asset name only records whether the source name carried a ','
//      prefix; the engine masks it off before comparing, so this code does the same
// ja : ハッシュ名の bit63 は元の名前が ',' 接頭辞を持つかを示すだけ。エンジンは比較前に
//      落とすので、ここでも同じように扱う
constexpr std::uint64_t	kJupAssetNameMask = 0x7FFFFFFFFFFFFFFFull;

// en : GfxImageCategory IMG_CATEGORY_LOAD_FROM_FILE
// ja : GfxImageCategory の IMG_CATEGORY_LOAD_FROM_FILE
constexpr std::uint8_t	kJupImageCategoryLoadFromFile = 3;

// en : JUP S6 R_Texture_Destroy
constexpr std::uintptr_t	kJupRTextureDestroy = 0x3771100;

// en : User-editable image override table, read from <_assetPathTextureLoad>camolist.json
// ja : ユーザーが編集する画像差し替え表。<_assetPathTextureLoad>camolist.json から読む
//
//   { "camoTable": [
//       { "hashed": "39ff902af11bb715", "imgpath": "camo_c_07/0_0" },
//       { "hashed": "10b02f1eb9640c83", "imgpath": "camo_c_07/0_0" }
//   ] }
//
// en : several hashes may share one imgpath, which is the reason this table exists - without it
//      every image needs its own file named after its hash
// ja : 複数のハッシュが同じ imgpath を指せる。この表を作った理由がこれで、無い場合は
//      ハッシュ名ごとに同じ画像を用意する必要がある
//
// en : "hashed" accepts a string ("39ff...", "0x39ff...") or a JSON number; bit 63 is masked off
//      both sides, the way the engine compares asset names
// ja : "hashed" は文字列でも JSON 数値でも良い。比較時は両側の bit63 を落とす（エンジンと同じ）
//
// en : write '/' in imgpath - it is converted to a backslash after the path is built
// ja : imgpath の区切りは '/' で書く。パス構築後にバックスラッシュへ変換される
std::unordered_map<std::uint64_t, std::string>	g_jupCamoTable;
bool											g_jupCamoTableLoaded = false;



// en : Reads the replacement pixels and fills in the size/format they imply.
//
//      A DDS is taken as-is: the compressed block data is handed to the engine untouched, only
//      the top mip is read (the caller sets maxLevelCount = 1), and the format is translated
//      rather than decoded. A PNG is always RGBA8.
//
//      Everything here is a validation of the source file, not of the engine - an input this
//      function accepts is one Image_SetupInternal can upload without further checks.
//
// ja : 差し替え用のピクセルを読み、そこから決まるサイズと形式を埋める。
//
//      DDS は展開せずそのまま渡す。読むのは先頭 mip だけで（呼び出し側が maxLevelCount = 1 を
//      設定する）、形式はデコードせず変換するだけ。PNG は常に RGBA8 として扱う。
//
//      ここでの検査は全てソースファイルに対するもので、エンジン側の検査ではない。この関数が
//      受理した入力は、Image_SetupInternal がそのままアップロードできる
static bool LoadJUPCustomCamoPixels(const std::string& path, bool isDds, std::string& pixels, Image_SetupParams_JUP& params, std::string& error)
{
	// en : PNG path - the loader always hands back RGBA8, so only the size has to be checked
	// ja : PNG の場合。ローダーは常に RGBA8 を返すので、確認するのはサイズだけ
	if (!isDds)
	{
		auto raw = LoadRawImageFromFile(path, false);
		params.width = raw.get_width();
		params.height = raw.get_height();
		params.format = GFX_PF_JUP_RGBA8;
		if (params.width <= 0 || params.height <= 0 || params.width > 16384 || params.height > 16384)
		{
			error = "PNG dimensions must be within 1..16384";
			return false;
		}

		// en : a short buffer would make the upload read past the end of the decoded image
		// ja : バッファが足りないと、アップロード時にデコード済み画像の末尾を超えて読む
		const size_t required = static_cast<size_t>(params.width) * params.height * 4;
		if (!raw.get_buffer() || raw.get_size() < required)
		{
			error = "truncated RGBA8 pixel buffer";
			return false;
		}
		pixels.assign(static_cast<const char*>(raw.get_buffer()), required);
		return true;
	}

	// en : DDS path. The buffer is 148 bytes so the optional 20-byte DX10 header fits after the
	//      128-byte base header
	// ja : DDS の場合。148 バイト確保しているのは、128 バイトの基本ヘッダーの後ろに
	//      任意の DX10 ヘッダー 20 バイトが入るため
	std::ifstream file(path, std::ios::binary);
	unsigned char header[148]{};
	if (!file.read(reinterpret_cast<char*>(header), 128))
	{
		error = "DDS header is missing or truncated";
		return false;
	}

	// en : DDS stores every field little-endian, so it is assembled by hand rather than cast
	// ja : DDS の各フィールドはリトルエンディアンなので、キャストせず手で組み立てる
	const auto read32 = [&header](size_t offset) -> std::uint32_t
	{
		return static_cast<std::uint32_t>(header[offset])
			| (static_cast<std::uint32_t>(header[offset + 1]) << 8)
			| (static_cast<std::uint32_t>(header[offset + 2]) << 16)
			| (static_cast<std::uint32_t>(header[offset + 3]) << 24);
	};

	// en : +0 magic "DDS ", +4 dwSize 124, +76 ddspf.dwSize 32, +8 dwFlags must carry
	//      CAPS|HEIGHT|WIDTH|PIXELFORMAT (0x1007)
	// ja : +0 マジック "DDS "、+4 dwSize は 124、+76 ddspf.dwSize は 32、
	//      +8 dwFlags に CAPS|HEIGHT|WIDTH|PIXELFORMAT (0x1007) が立っていること
	if (read32(0) != 0x20534444 || read32(4) != 124 || read32(76) != 32
		|| (read32(8) & 0x1007) != 0x1007)
	{
		error = "invalid DDS header";
		return false;
	}

	// en : +12 dwHeight, +16 dwWidth. +24 dwDepth > 1 is a volume texture and +112 dwCaps2
	//      0x20FE00 covers the cubemap faces and the volume bit - none of which a camo layer is
	// ja : +12 dwHeight、+16 dwWidth。+24 dwDepth が 1 より大きければボリュームテクスチャで、
	//      +112 dwCaps2 の 0x20FE00 はキューブマップの各面とボリュームのビット。
	//      迷彩レイヤーはそのいずれでもない
	const auto width = read32(16);
	const auto height = read32(12);
	if (!width || !height || width > 16384 || height > 16384
		|| read32(24) > 1 || (read32(112) & 0x20FE00) != 0)
	{
		error = "DDS must be a single 2D surface within 1..16384";
		return false;
	}

	// en : non-zero for a block-compressed format, and then the byte size of one 4x4 block
	// ja : ブロック圧縮形式なら非ゼロで、4x4 ブロック 1 個のバイト数が入る
	unsigned int bytesPerBlock = 0;
	const auto fourCC = read32(84);

	// en : ddspf.dwFlags +80 bit 0x4 = DDPF_FOURCC. FourCC "DX10" means a 20-byte extended
	//      header follows, which is where modern formats (BC6H/BC7) are declared
	// ja : ddspf.dwFlags(+80) の 0x4 は DDPF_FOURCC。FourCC が "DX10" の場合は
	//      20 バイトの拡張ヘッダーが続き、新しい形式（BC6H/BC7）はそちらで宣言される
	if ((read32(80) & 4) != 0 && fourCC == 0x30315844) // DX10
	{
		if (!file.read(reinterpret_cast<char*>(header + 128), 20))
		{
			error = "truncated DDS DX10 header";
			return false;
		}

		// en : +132 resourceDimension 3 = TEXTURE2D, +140 arraySize 1, +136 miscFlag 0x4 = cube
		// ja : +132 resourceDimension が 3 なら TEXTURE2D、+140 arraySize は 1、
		//      +136 miscFlag の 0x4 はキューブマップ
		if (read32(132) != 3 || read32(140) != 1 || (read32(136) & 4) != 0)
		{
			error = "DDS arrays, cubemaps and volumes are not supported";
			return false;
		}

		// en : +128 dxgiFormat -> the JUP pixel format. Checked against the engine's own DXGI
		//      table at 0x7FF74B695090
		// ja : +128 dxgiFormat から JUP のピクセル形式へ。エンジン内の DXGI テーブル
		//      0x7FF74B695090 と照合済み
		switch (read32(128))
		{
		case 28: params.format = GFX_PF_JUP_RGBA8; break;
		case 29: params.format = GFX_PF_JUP_RGBA8_SRGB; break;
		case 71: params.format = GFX_PF_JUP_BC1; bytesPerBlock = 8; break;
		case 72: params.format = GFX_PF_JUP_BC1_SRGB; bytesPerBlock = 8; break;
		case 74: params.format = GFX_PF_JUP_BC2; bytesPerBlock = 16; break;
		case 75: params.format = GFX_PF_JUP_BC2_SRGB; bytesPerBlock = 16; break;
		case 77: params.format = GFX_PF_JUP_BC3; bytesPerBlock = 16; break;
		case 78: params.format = GFX_PF_JUP_BC3_SRGB; bytesPerBlock = 16; break;
		case 80: params.format = GFX_PF_JUP_BC4; bytesPerBlock = 8; break;
		case 83: params.format = GFX_PF_JUP_BC5; bytesPerBlock = 16; break;
		case 84: params.format = GFX_PF_JUP_BC5_SNORM; bytesPerBlock = 16; break;
		case 95: params.format = GFX_PF_JUP_BC6H; bytesPerBlock = 16; break;
		case 96: params.format = GFX_PF_JUP_BC6H_SF16; bytesPerBlock = 16; break;
		case 98: params.format = GFX_PF_JUP_BC7; bytesPerBlock = 16; break;
		case 99: params.format = GFX_PF_JUP_BC7_SRGB; bytesPerBlock = 16; break;
		default: error = "unsupported DDS DXGI format"; return false;
		}
	}
	// en : legacy FourCC. These predate the DX10 header and carry no sRGB information, so they
	//      all map to the linear variant
	// ja : 旧来の FourCC。DX10 ヘッダー以前の形式で sRGB 情報を持たないため、
	//      すべてリニア側の形式に対応させる
	else if ((read32(80) & 4) != 0)
	{
		switch (fourCC)
		{
		case 0x31545844: params.format = GFX_PF_JUP_BC1; bytesPerBlock = 8; break;  // DXT1
		case 0x33545844: params.format = GFX_PF_JUP_BC2; bytesPerBlock = 16; break; // DXT3
		case 0x35545844: params.format = GFX_PF_JUP_BC3; bytesPerBlock = 16; break; // DXT5
		case 0x31495441: // ATI1
		case 0x55344342: params.format = GFX_PF_JUP_BC4; bytesPerBlock = 8; break; // BC4U
		case 0x32495441: // ATI2
		case 0x55354342: params.format = GFX_PF_JUP_BC5; bytesPerBlock = 16; break; // BC5U
		case 0x53354342: params.format = GFX_PF_JUP_BC5_SNORM; bytesPerBlock = 16; break;
		default: error = "unsupported DDS FourCC"; return false;
		}
	}
	// en : uncompressed. Only the exact 32-bit BGRA/RGBA channel layout is taken
	//      (+80 flags RGB|ALPHAPIXELS, +88 bit count 32, +92..+104 the four masks)
	// ja : 非圧縮。32bit で各チャンネルのマスクが一致する並びだけを受理する
	//      （+80 の RGB|ALPHAPIXELS、+88 のビット数 32、+92..+104 の 4 つのマスク）
	else if ((read32(80) & 0x41) == 0x41 && read32(88) == 32
		&& read32(92) == 0xFF && read32(96) == 0xFF00
		&& read32(100) == 0xFF0000 && read32(104) == 0xFF000000)
	{
		params.format = GFX_PF_JUP_RGBA8;
	}
	else
	{
		error = "unsupported DDS channel masks (expected RGBA8)";
		return false;
	}

	// en : a block-compressed surface is stored as whole 4x4 blocks, so a top mip that is not a
	//      multiple of 4 cannot be uploaded as-is
	// ja : ブロック圧縮は 4x4 ブロック単位で格納されるため、先頭 mip の辺が 4 の倍数でないと
	//      そのままアップロードできない
	if (bytesPerBlock && ((width & 3) || (height & 3)))
	{
		error = "compressed DDS top-level dimensions must be multiples of 4";
		return false;
	}

	// en : +8 dwFlags bit 0x8 = DDSD_PITCH, and +20 then holds the row pitch. A pitch wider than
	//      the row itself means padded rows, which this straight copy would misread
	// ja : +8 dwFlags の 0x8 は DDSD_PITCH で、+20 に行ピッチが入る。ピッチが行幅より大きい
	//      場合は行ごとのパディングがあり、このまま一括コピーすると読み違える
	if (!bytesPerBlock && (read32(8) & 8) != 0 && read32(20) != width * 4)
	{
		error = "padded DDS rows are not supported";
		return false;
	}

	// en : size of the top mip only - the mip chain after it is not read
	// ja : 先頭 mip のみのサイズ。以降の mip チェーンは読まない
	const size_t required = bytesPerBlock
		? static_cast<size_t>((width + 3) / 4) * ((height + 3) / 4) * bytesPerBlock
		: static_cast<size_t>(width) * height * 4;

	// en : the payload starts wherever the header reads left off, which differs by 20 bytes
	//      depending on whether a DX10 header was consumed
	// ja : ピクセルデータはヘッダーを読み終えた位置から始まる。DX10 ヘッダーを読んだかどうかで
	//      20 バイトずれる
	const auto payloadStart = file.tellg();
	file.seekg(0, std::ios::end);
	const auto payloadEnd = file.tellg();
	if (payloadStart < 0 || payloadEnd < payloadStart
		|| static_cast<std::uint64_t>(payloadEnd - payloadStart) < required)
	{
		error = "truncated DDS top-level pixel data";
		return false;
	}

	file.seekg(payloadStart);
	pixels.resize(required);
	if (!file.read(&pixels[0], static_cast<std::streamsize>(required)))
	{
		error = "failed to read DDS pixel data";
		return false;
	}

	params.width = static_cast<int>(width);
	params.height = static_cast<int>(height);
	return true;
}



// en : drops the cached table so the next lookup re-reads camolist.json
// ja : キャッシュを破棄し、次回の参照で camolist.json を読み直させる
void ResetJUPCamoTable()
{
	g_jupCamoTable.clear();
	g_jupCamoTableLoaded = false;
}



// en : parses camolist.json once and caches it. Load_GfxImage runs for EVERY image in a zone, so
//      re-reading the file per image would put file I/O on the DB load thread thousands of times
// ja : camolist.json を一度だけ解析してキャッシュする。Load_GfxImage はゾーン内の全画像に対して
//      呼ばれるため、毎回読み直すと DB ロードスレッドで大量のファイル I/O が発生する
const std::unordered_map<std::uint64_t, std::string>& GetJUPCamoTable()
{
	if (g_jupCamoTableLoaded)
		return g_jupCamoTable;

	// en : marked loaded even on failure, so a zone without a table does not retry every image
	// ja : 失敗時も読み込み済みにする。表が無いゾーンで画像ごとに再試行しないため
	g_jupCamoTableLoaded = true;

	std::string path = _assetPathTextureLoad + "camolist.json";
	ReplaceAll(path, "/", "\\");
	if (!file_exists(path.c_str()))
	{
		NotifyMsg(MsgLevel::Lv_Debug, "GetJUPCamoTable", "no camolist.json at %s\n", path.c_str());
		return g_jupCamoTable;
	}

	try
	{
		// en : the last argument allows // and /* */ comments in the file
		// ja : 最後の引数で // と /* */ のコメントを許可する
		std::ifstream jsonFile(path);
		nlohmann::json camoJson = nlohmann::json::parse(jsonFile, nullptr, true, true);

		const auto tableIt = camoJson.find("camoTable");
		if (tableIt == camoJson.end() || !tableIt->is_array())
		{
			NotifyMsg(MsgLevel::Lv_Failed, "GetJUPCamoTable", "camolist.json has no 'camoTable' array: %s\n", path.c_str());
			return g_jupCamoTable;
		}

		for (const auto& entry : *tableIt)
		{
			if (!entry.is_object())
				continue;

			const auto hashedIt = entry.find("hashed");
			const auto imgPathIt = entry.find("imgpath");
			if (hashedIt == entry.end() || imgPathIt == entry.end() || !imgPathIt->is_string())
				continue;

			std::uint64_t hashed = 0;
			if (hashedIt->is_string())
			{
				std::string text = hashedIt->get<std::string>();
				if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
					text = text.substr(2);
				if (text.empty())
					continue;

				try
				{
					hashed = std::stoull(text, nullptr, 16);
				}
				catch (const std::exception&)
				{
					NotifyMsg(MsgLevel::Lv_Failed, "GetJUPCamoTable", "'hashed' is not a hex value: %s\n", text.c_str());
					continue;
				}
			}
			else if (hashedIt->is_number_unsigned())
			{
				hashed = hashedIt->get<std::uint64_t>();
			}
			else
			{
				continue;
			}

			g_jupCamoTable[hashed & kJupAssetNameMask] = imgPathIt->get<std::string>();
		}

		NotifyMsg(MsgLevel::Lv_Success, "GetJUPCamoTable", "%zu entr(ies) loaded from %s\n", g_jupCamoTable.size(), path.c_str());
	}
	catch (const std::exception& e)
	{
		g_jupCamoTable.clear();
		NotifyMsg(MsgLevel::Lv_Failed, "GetJUPCamoTable", "%s: %s\n", e.what(), path.c_str());
	}

	return g_jupCamoTable;
}



// en : the imgpath a hashed image name maps to, or an empty string when it is not listed
// ja : ハッシュ名に対応する imgpath。未登録なら空文字列を返す
std::string FindJUPCamoImagePath(std::uint64_t hashedName)
{
	const auto& table = GetJUPCamoTable();
	const auto it = table.find(hashedName & kJupAssetNameMask);
	return (it != table.end()) ? it->second : std::string{};
}



// en : builds the source path for an imgpath, preferring .dds over .png. Returns false when
//      neither exists
// ja : imgpath から実ファイルのパスを作る。.dds を優先し、無ければ .png を見る
//      どちらも無ければ false
bool ResolveJUPCamoImageFile(const std::string& imgpath, std::string& pathOut, bool& isDdsOut)
{
	isDdsOut = true;
	pathOut = _assetPathTextureLoad + imgpath + ".dds";
	ReplaceAll(pathOut, "/", "\\");
	if (file_exists(pathOut.c_str()))
		return true;

	isDdsOut = false;
	pathOut = _assetPathTextureLoad + imgpath + ".png";
	ReplaceAll(pathOut, "/", "\\");
	return file_exists(pathOut.c_str());
}



// en : fills the setup params for a replacement texture from the image it replaces
// ja : 差し替え先テクスチャの生成パラメーターを、元画像を基に埋める
void FillJUPReplacementParams(const GfxImage_JUP* image, Image_SetupParams_JUP& params)
{
	params.depth = 1;
	params.numElements = 1;
	params.maxLevelCount = 1;

	// en : STREAMED must be cleared - R_Texture_CreateInternal reads params.flags bit 2 and tags
	//      the new id 0xC0000000 (streamed) instead of 0x40000000, handing it back to the streamer.
	//      PACKED_ATLAS goes too: the replacement is a standalone texture, not an atlas region
	// ja : STREAMED は必ず落とす。R_Texture_CreateInternal が params.flags の bit2 を見て
	//      0x40000000 ではなく 0xC0000000（ストリーミング）を付け、ストリーマーの管理下に戻るため
	//      PACKED_ATLAS も落とす。差し替え後はアトラスの一部ではなく単独テクスチャになる
	params.flags = (image->flags | IMG_JUP_NOPICMIP | IMG_JUP_NOMIPMAPS)
		& ~static_cast<std::uint32_t>(IMG_JUP_STREAMED | IMG_JUP_PACKED_ATLAS);

	if (params.format == GFX_PF_JUP_RGBA8_SRGB || params.format == GFX_PF_JUP_BC1_SRGB
		|| params.format == GFX_PF_JUP_BC2_SRGB || params.format == GFX_PF_JUP_BC3_SRGB
		|| params.format == GFX_PF_JUP_BC7_SRGB)
		params.flags |= IMG_JUP_SRGB;

	// en : Image_GetSetupParams' default for a resident image
	// ja : Image_GetSetupParams が常駐画像に使う既定値
	params.initialResourceState = 0x8C0;
}



// en : Runtime replacement, for an image the game keeps resident.
//
//      A STREAMED image cannot be taken over here: the streaming worker owns it through a global
//      queue rather than through the image struct, so no field on the image unregisters it. It
//      walks the image every tick (sub_7FF7466AD190 loops to image->streamedPartCount and
//      dereferences image->streams) and re-uploads the shipped mips over anything injected, so the
//      swap is either silently undone or faults. Such an image has to be taken over at load time
//      instead - see TryInjectJUPImageAtLoad.
//
// ja : ランタイムでの差し替え。常駐画像専用。
//
//      STREAMED 画像はここでは奪えない。ストリーミングワーカーは画像構造体ではなくグローバルな
//      キューで管理しているため、構造体側に登録解除のスイッチが無い。ワーカーは毎tick画像を辿り
//      （sub_7FF7466AD190 が image->streamedPartCount までループし image->streams を参照）、
//      元の mip を上書きし直すので、差し替えは無効化されるか落ちる。
//      その種の画像はロード時に奪う（TryInjectJUPImageAtLoad を参照）
bool TryInjectJUPCustomCamoNew(GfxImage_JUP* image, std::string pngpath, GfxPixelFormat_JUP format)
{
	if (!image)
		return false;

	// en : pngpath comes from camolist.json - the caller has already resolved which file to use
	// ja : pngpath は camolist.json 由来。どのファイルを使うかは呼び出し側で解決済み
	bool isDds = false;
	std::string path;
	if (!ResolveJUPCamoImageFile(pngpath, path, isDds))
		return false;

	const auto oldId = static_cast<std::uint32_t>(image->textureId);
	if ((image->flags & IMG_JUP_STREAMED) != 0 || (oldId & 0x80000000u) != 0)
	{
		NotifyMsg(MsgLevel::Lv_Warning, "TryInjectJUPCustomCamoNew", "Skipped streamed image (flags=0x%08X texId=0x%08X): %s\n", image->flags, oldId, path.c_str());
		return false;
	}

	try
	{
		Image_SetupParams_JUP params{};
		std::string pixels;
		std::string error;
		if (!LoadJUPCustomCamoPixels(path, isDds, pixels, params, error))
		{
			NotifyMsg(MsgLevel::Lv_Failed, "TryInjectJUPCustomCamoNew", "%s: %s\n", error.c_str(), path.c_str());
			return false;
		}
		FillJUPReplacementParams(image, params);

		// en : Image_SetupInternal only writes format/size/flags/levelCount/totalSize/textureId, so
		//      the rest is set here. streamedPartCount and streams are deliberately left alone:
		//      clearing them does not unregister the image, it only makes the streaming worker read
		//      a null pointer
		// ja : Image_SetupInternal が書くのは format/size/flags/levelCount/totalSize/textureId だけ
		//      なので残りはここで設定する。streamedPartCount と streams は意図的に触らない。
		//      消しても登録解除にはならず、ストリーミングワーカーが null を参照して落ちるだけ
		GfxImage_JUP replacement = *image;
		replacement.textureId = GfxTextureId::NULLID;
		replacement.flags = params.flags;
		replacement.levelCount = 1;
		replacement.width = static_cast<std::uint16_t>(params.width);
		replacement.height = static_cast<std::uint16_t>(params.height);
		replacement.depth = 1;
		replacement.numElements = 1;
		replacement.format = params.format;
		replacement.category = kJupImageCategoryLoadFromFile;

		// en : the upload copies the source data before it returns, so a heap buffer is enough
		// ja : アップロードは復帰前に元データをコピーするので、ヒープ上のバッファで足りる
		auto setupData = std::make_unique<Image_SetupData>();
		setupData->data[0][0] = pixels.data();

		auto setup = reinterpret_cast<void (*)(GfxImage_JUP*, Image_SetupParams_JUP*, const Image_SetupData*)>(CalcPtr(_adr.Image_SetupInternal));
		auto destroy = reinterpret_cast<void (*)(GfxTextureId)>(CalcPtr(kJupRTextureDestroy));
		setup(&replacement, &params, setupData.get());

		// en : R_Texture_CreateInternal tags the id by the STREAMED flag it was given:
		//      0x40000000 = ordinary resident texture, 0xC0000000 = streamed. Anything else means
		//      the texture was not created the way this path assumes, so it is released instead of
		//      being published into the DB pool
		// ja : R_Texture_CreateInternal は渡された STREAMED フラグで id にタグを付ける。
		//      0x40000000 = 常駐テクスチャ、0xC0000000 = ストリーミング。それ以外はこの経路が
		//      想定した作られ方ではないので、DB プールへ公開せず解放する
		const auto newId = static_cast<std::uint32_t>(replacement.textureId);
		if ((newId & 0xC0000000u) != 0x40000000u || replacement.textureId == image->textureId)
		{
			NotifyMsg(MsgLevel::Lv_Failed, "TryInjectJUPCustomCamoNew", "Unexpected texture ID 0x%08X: %s\n", newId, path.c_str());
			if ((newId & 0x40000000u) != 0 && replacement.textureId != image->textureId)
				destroy(replacement.textureId);
			return false;
		}

		// en : the image address inside the DB pool is kept; only its contents are swapped.
		//
		//      The previous texture is deliberately NOT destroyed. Freeing slot X writes X into the
		//      pool's allocation cursor (sub_7FF745D92BE0), and GetFreeIndex starts its search
		//      there, so the next create hands back the slot just released - two GfxImages end up
		//      sharing one pool entry. R_Texture_Destroy also releases the D3D resource immediately,
		//      with no fence, while the GPU may still be reading it. Leaking one slot per swap out
		//      of 0x42000 is the safe trade.
		// ja : DB プール内の画像アドレスは維持し、中身だけ差し替える。
		//
		//      直前のテクスチャは意図的に破棄しない。スロット X を解放するとプールの確保カーソルに
		//      X が書かれ（sub_7FF745D92BE0）、GetFreeIndex はそこから探すため、次の生成で同じ
		//      スロットが返る。結果として 2 つの GfxImage が 1 スロットを共有する。さらに
		//      R_Texture_Destroy は D3D リソースをフェンス無しで即座に解放するため、GPU がまだ
		//      読んでいる可能性がある。0x42000 中 1 スロットのリークの方が安全
		*image = replacement;

		NotifyMsg(MsgLevel::Lv_Success, "TryInjectJUPCustomCamoNew", "Created resident texture %ux%u, format %u: %s\n", image->width, image->height, image->format, path.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		NotifyMsg(MsgLevel::Lv_Failed, "TryInjectJUPCustomCamoNew", "%s: %s\n", e.what(), path.c_str());
		return false;
	}
}



// en : Debug helper: reports one camo image and replaces it if camolist.json lists it.
//      Used by the injctex command to find out which hashed image name sits behind each camo
//      slot, so the logging is the point here and is kept even when no override exists.
// ja : デバッグ用。迷彩の画像 1 枚を報告し、camolist.json に登録があれば差し替える。
//      injctex コマンドから各迷彩スロットのハッシュ名を調べるために使うので、ログ出力自体が
//      目的であり、差し替え対象が無くても出力する
void TryLoadJupCustomGfxImage_impl(std::string str, std::string fieldname, GfxCamo_JUP* camo, GfxImage_JUP* img)
{
	if (!img)
	{
		NotifyMsg(MsgLevel::Lv_Failed, "TryLoadJupCustomGfxImage_impl", "not found texture data = %s - %s\n", str.c_str(), fieldname.c_str());
		return;
	}

	try
	{
		const std::uint64_t	name = img->name;

		// en : the hashed name is what camolist.json is keyed by, so it is printed in the same
		//      form the user has to write into the file
		// ja : ハッシュ名は camolist.json の検索キーなので、ユーザーがファイルに書くのと
		//      同じ形式で出力する
		char imageid[256];
		sprintf_s(imageid, "%llX", name);
		const std::string imgname = imageid;

		if (_showDebugLogs)
		{
			char flagBuf[256];
			gfx_image_flags_to_string_v2(static_cast<unsigned int>(img->flags), flagBuf, sizeof(flagBuf));

			NotifyMsg(MsgLevel::Lv_Debug, "TryLoadJupCustomGfxImage_impl",
				"%s %s: texId=0x%llx | format=%u(%s) | flags=0x%08x[%s] | MAP=%s | semantic=%d(%s)"
				" | category=%d(%s) | depth=%u | elems=%u | levels=%u | streamed=%u | size=%ux%u"
				" | totalSize=%u | imagename='%llx' \n",
				str.c_str(), fieldname.c_str(),
				static_cast<unsigned long long>(img->textureId),
				static_cast<unsigned int>(img->format), gfx_pixel_format_name_v2(static_cast<int>(img->format)),
				static_cast<unsigned int>(img->flags), flagBuf,
				gfx_image_maptype_name_v2(static_cast<unsigned int>(img->flags)),
				static_cast<int>(img->semantic), texture_semantic_name_v2(static_cast<int>(img->semantic)),
				static_cast<int>(img->category), gfx_image_category_name_v2(static_cast<int>(img->category)),
				static_cast<unsigned int>(img->depth), static_cast<unsigned int>(img->numElements),
				static_cast<unsigned int>(img->levelCount), static_cast<unsigned int>(img->streamedPartCount),
				static_cast<unsigned int>(img->width), static_cast<unsigned int>(img->height),
				static_cast<unsigned int>(img->totalSize), name);
		}

		// en : camolist.json decides which file this image uses, looked up by its hashed name.
		//      Not being listed is normal - most images in a camo have no override - so it is
		//      reported and skipped rather than treated as an error
		// ja : どのファイルを使うかは camolist.json がハッシュ名で決める。未登録は異常ではなく
		//      普通の状態（大半の画像は差し替え対象ではない）なので、報告して飛ばすだけにする
		const std::string imgpath = FindJUPCamoImagePath(name);
		if (imgpath.empty())
		{
			NotifyMsg(MsgLevel::Lv_Debug, "TryLoadJupCustomGfxImage_impl", "'%s' is not listed in camolist.json, left as loaded\n", imgname.c_str());
			return;
		}

		NotifyMsg(MsgLevel::Lv_Debug, "TryLoadJupCustomGfxImage_impl", "%s %s: '%s' -> '%s'\n", str.c_str(), fieldname.c_str(), imgname.c_str(), imgpath.c_str());
		TryInjectJUPCustomCamoNew(img, imgpath, GfxPixelFormat_JUP::GFX_PF_JUP_RGBA8);
	}
	catch (std::exception& e)
	{
		NotifyMsg(MsgLevel::Lv_Failed, "TryLoadJupCustomGfxImage_impl", "Failed to load texture %s - %s - %s \n", str.c_str(), fieldname.c_str(), e.what());
	}
}



// en : Debug entry point for the injctex command: walks every image slot of one camo.
//
//      Note this only reaches images the camo asset points at, so the weapon carrying the camo
//      has to be equipped for the runtime swap to matter. A streamed slot is skipped by
//      TryInjectJUPCustomCamoNew and has to be taken over at load time instead.
//
// ja : injctex コマンド用のデバッグ入口。指定した迷彩の全画像スロットを走査する。
//
//      辿れるのは迷彩アセットが指す画像だけなので、ランタイム差し替えを効かせるには
//      その迷彩を付けた武器を所持している必要がある。ストリーミング画像のスロットは
//      TryInjectJUPCustomCamoNew 側で弾かれるため、ロード時に奪う必要がある
void TryLoadJupCustomGfxImage(std::string str)
{
	GfxCamo_JUP* camo = DB_FindXAssetHeader_JUP_f(118, str.c_str(), 0).camo;
	if (!camo || !camo->name)
	{
		NotifyMsg(MsgLevel::Lv_Failed, "TryLoadJupCustomGfxImage", "not found camo ptr = %s\n", str.c_str());
		return;
	}

	// en : one slot of the camo. Reports a missing slot rather than skipping quietly, because the
	//      point of this command is to show what the camo actually holds
	// ja : 迷彩のスロット 1 つ分。空のスロットも黙って飛ばさず報告する。このコマンドの目的は
	//      迷彩が実際に何を持っているかを見ることだから
	const auto visitSlot = [&str, camo](GfxImage_JUP* img, const char* fieldname)
	{
		if (img)
			TryLoadJupCustomGfxImage_impl(str, fieldname, camo, img);
		else
			NotifyMsg(MsgLevel::Lv_Failed, "TryLoadJupCustomGfxImage", "not found camo %s = %s\n", fieldname, str.c_str());
	};

	visitSlot(camo->singleImage, "singleImage");

	// en : the four texture groups, three images each, in the order Load_Camo fixes them up
	// ja : 4 つのテクスチャグループ（各 3 枚）を、Load_Camo が解決するのと同じ順で見る
	GfxImage_JUP* const* const groups[4] =
	{
		camo->textureGroup0, camo->textureGroup1, camo->textureGroup2, camo->textureGroup3
	};
	for (int group = 0; group < 4; group++)
	{
		for (int slot = 0; slot < 3; slot++)
		{
			char fieldname[32];
			sprintf_s(fieldname, "textureGroup%d_%d", group, slot);
			visitSlot(groups[group][slot], fieldname);
		}
	}
}



// en : Load-time replacement, run from the Load_GfxImage hook.
//
//      This is the only point where a STREAMED image can be taken over. Once Load_GfxImage
//      returns, the streaming worker owns the image and re-uploads the shipped mips over anything
//      injected afterwards. Here the image has only just been fixed up and no streaming request
//      exists for it yet, so clearing STREAMED actually prevents the worker from ever asking for
//      it - the same field writes are what crash the worker if done later.
//
//      Note that Load_GfxImage skips the pixel load entirely for a STREAMED image
//      (if ((flags & 4) == 0) { ... Load_ImagePixels ... }), which is why hooking Load_ImagePixels
//      never fires for a streamed camo layer, and why the pixels have to come from our own file.
//
// ja : ロード時の差し替え。Load_GfxImage フックから実行される。
//
//      STREAMED 画像を奪えるのはここだけ。Load_GfxImage を抜けるとストリーミングワーカーが
//      画像を所有し、差し替えた内容の上に元の mip を再アップロードする。この時点ではまだ
//      ストリーミング要求がキューに入っていないため、STREAMED を落とせばワーカーはそもそも
//      この画像を要求しない。同じ書き換えを後から行うとワーカーが落ちる。
//
//      なお Load_GfxImage は STREAMED 画像のピクセル読み込みを丸ごと飛ばす
//      （if ((flags & 4) == 0) { ... Load_ImagePixels ... }）。Load_ImagePixels にフックしても
//      発火しないのはこのためで、ピクセルは自前のファイルから用意する必要がある
bool TryInjectJUPImageAtLoad(GfxImage_JUP* image)
{
	if (!image || !image->name)
		return false;

	// en : the user's table decides which file this image gets. This runs for every image in the
	//      zone, so "not listed" has to be the cheap, silent path
	// ja : どのファイルを使うかはユーザーの表が決める。ゾーン内の全画像に対して呼ばれるため、
	//      「未登録」は軽く静かに抜ける経路にする
	const std::string imgpath = FindJUPCamoImagePath(image->name);
	if (imgpath.empty())
		return false;

	char namebuf[32];
	sprintf_s(namebuf, "%llx", static_cast<unsigned long long>(image->name & kJupAssetNameMask));

	bool isDds = false;
	std::string path;
	if (!ResolveJUPCamoImageFile(imgpath, path, isDds))
	{
		NotifyMsg(MsgLevel::Lv_Failed, "TryInjectJUPImageAtLoad", "'%s' is listed as '%s' but no .dds/.png was found there\n", namebuf, imgpath.c_str());
		return false;
	}

	try
	{
		Image_SetupParams_JUP params{};
		std::string pixels;
		std::string error;
		if (!LoadJUPCustomCamoPixels(path, isDds, pixels, params, error))
		{
			NotifyMsg(MsgLevel::Lv_Failed, "TryInjectJUPImageAtLoad", "%s: %s\n", error.c_str(), path.c_str());
			return false;
		}
		FillJUPReplacementParams(image, params);

		// en : the image is no longer streamed, so the stream table goes with it. Safe HERE
		//      precisely because no request has been queued yet
		// ja : もうストリーミング画像ではないので stream テーブルも落とす。まだ要求が
		//      キューに入っていないこの時点だからこそ安全
		image->streamedPartCount = 0;
		image->streams = nullptr;
		image->packedAtlasData = nullptr;
		image->levelCount = 1;
		image->depth = 1;
		image->numElements = 1;
		image->category = kJupImageCategoryLoadFromFile;
		image->textureId = GfxTextureId::NULLID;
		image->flags = params.flags;
		// en : pixels is left to Image_SetupInternal, which zeroes it itself (image[7] = 0)
		// ja : pixels は Image_SetupInternal が自前でゼロ化する（image[7] = 0）ので触らない

		auto setupData = std::make_unique<Image_SetupData>();
		setupData->data[0][0] = pixels.data();
		auto setup = reinterpret_cast<void (*)(GfxImage_JUP*, Image_SetupParams_JUP*, const Image_SetupData*)>(CalcPtr(_adr.Image_SetupInternal));
		setup(image, &params, setupData.get());

		// en : nothing of ours to destroy - the id the image held on entry was built by the zone,
		//      not by this path
		// ja : こちらが破棄すべきものは無い。入口で持っていた id はゾーンが作ったものであり、
		//      この経路が作ったものではない
		const auto newId = static_cast<std::uint32_t>(image->textureId);
		if ((newId & 0xC0000000u) != 0x40000000u)
		{
			NotifyMsg(MsgLevel::Lv_Failed, "TryInjectJUPImageAtLoad", "Unexpected texture ID 0x%08X: %s\n", newId, path.c_str());
			return false;
		}

		NotifyMsg(MsgLevel::Lv_Success, "TryInjectJUPImageAtLoad", "'%s' replaced at load: %ux%u format=%u texId=0x%08X <- %s\n", namebuf, image->width, image->height, image->format, newId, path.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		NotifyMsg(MsgLevel::Lv_Failed, "TryInjectJUPImageAtLoad", "%s: %s\n", e.what(), path.c_str());
		return false;
	}
}



// en : A hook into the GfxImage loading function that executes while fast files are being decompressed and loaded.
// ja : ファストファイルを解凍して読み込んでいるときに実行されるGfxImage読み込み関数へのフック
void  Load_GfxImage_JUP_d(DBStreamStart streamStart, GfxImage_JUP* gfxImage)
{
	// en : let the zone finish building the image first - name, flags, dimensions and the stream
	//      table are only valid afterwards - then take it over before the streaming worker sees it
	// ja : まずゾーンに画像を作り切らせる。name / flags / 寸法 / stream テーブルはその後でないと
	//      有効にならない。そのうえでストリーミングワーカーが見る前に奪う
	Load_GfxImage_JUP_h.stub<void>(streamStart, gfxImage);

	if (!gfxImage)
		return;

	TryInjectJUPImageAtLoad(gfxImage);
}



// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //
// Custom camo - END
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ //



void SetupHook( )
{
	_adr.DB_FindXAssetHeader	= 0x3BD5900;
	_adr.Image_SetupInternal	= 0x55D0560;
	_adr.Load_GfxImage			= 0x3B0B730;
			
	Load_GfxImage_JUP_h.create( CalcPtr(_adr.Load_GfxImage) , Load_GfxImage_JUP_d);
}