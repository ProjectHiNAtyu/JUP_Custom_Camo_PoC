
typedef int scr_string_t;
struct ParticleSystemDef_JUP;
struct MaterialAnimationDef_JUP;

enum GfxPixelFormat_JUP : std::uint8_t
{
	GFX_PF_JUP_RGBA8 = 6,
	GFX_PF_JUP_RGBA8_SRGB = 7,
	GFX_PF_JUP_BC1 = 36,
	GFX_PF_JUP_BC1_SRGB = 37,
	GFX_PF_JUP_BC2 = 38,
	GFX_PF_JUP_BC2_SRGB = 39,
	GFX_PF_JUP_BC3 = 40,
	GFX_PF_JUP_BC3_SRGB = 41,
	GFX_PF_JUP_BC4 = 42,
	GFX_PF_JUP_BC5 = 43,
	GFX_PF_JUP_BC5_SNORM = 44,
	GFX_PF_JUP_BC6H = 45,
	GFX_PF_JUP_BC6H_SF16 = 46,
	GFX_PF_JUP_BC7 = 47,
	GFX_PF_JUP_BC7_SRGB = 48,
};

struct streamer_handle_t
{
	unsigned __int64 data;
};

union GfxImagePixels
{
	streamer_handle_t streamedDataHandle;
	unsigned __int8* residentData;
};

enum GfxTextureId : __int32
{
	NULLID = 0x0,
};

struct GfxImageAtlasSize
{
	unsigned __int8 rowCount;
	unsigned __int8 colCount;
};

union GfxImageSemanticSpecific
{
	float atlasFps;
	unsigned int albedoMapScaleBias;
	unsigned int normalMapScaleBias;
	unsigned int maxMipMap;
};

union GfxImageAtlasInfo
{
	GfxImageAtlasSize atlasSize;
	unsigned __int16 packedAtlasDataSize;
};

struct GfxImageStreamData_JUP
{
	std::uint8_t unknown00[0x10];
	// ja : 幅 bits0..14、高さ bits15..29、mip数 bits59..62。残りの意味は未確定。
	std::uint64_t packedDimensionsAndLevelCount;
};
static_assert(sizeof(GfxImageStreamData_JUP) == 0x18);

struct GfxImage_JUP
{
	std::uint64_t name;                        // +0x00 hashed asset name
	std::uint8_t* packedAtlasData;             // +0x08
	GfxTextureId textureId;                    // +0x10
	std::uint32_t flags;                       // +0x14 GfxImageFlags_JUP bits
	unsigned int totalSize;                    // +0x18
	GfxImageSemanticSpecific semanticSpecific; // +0x1C
	std::uint16_t width;                       // +0x20
	std::uint16_t height;                      // +0x22
	std::uint16_t depth;                       // +0x24
	std::uint16_t numElements;                 // +0x26
	GfxImageAtlasInfo atlasInfo;               // +0x28
	std::uint8_t format;                       // +0x2A GfxPixelFormat_JUP
	union
	{
		std::uint8_t semanticCategory;         // +0x2B Image_AllocProg @ 0x7FF747BECAD0
		struct
		{
			std::uint8_t semantic : 4;
			std::uint8_t category : 4;
		};
	};
	std::uint8_t levelCount;                   // +0x2C
	std::uint8_t unknown2D;                    // +0x2D NOT category
	std::uint8_t streamedPartCount;            // +0x2E
	std::uint8_t unknown2F;                    // +0x2F meaning unverified
	GfxImageStreamData_JUP* streams;           // +0x30
	GfxImagePixels pixels;                     // +0x38
};
static_assert(sizeof(GfxImage_JUP) == 0x40);

struct GfxCamo_JUP
{
	std::uint64_t name;                     // +0x00 hashed asset name
	scr_string_t internalName;              // +0x08
	std::uint8_t unknown0C[4];              // +0x0C meaning unverified
	GfxImage_JUP* textureGroup0[3];         // +0x10
	GfxImage_JUP* textureGroup1[3];         // +0x28
	GfxImage_JUP* textureGroup2[3];         // +0x40
	GfxImage_JUP* textureGroup3[3];         // +0x58
	GfxImage_JUP* singleImage;              // +0x70
	std::uint8_t unknown78[0x128];          // +0x78 .. +0x19F scalar fields unverified
	ParticleSystemDef_JUP* vehVfxTailLight; // +0x1A0 pointer type confirmed; semantic name from IW8
	std::uint8_t unknown1A8[0x18];          // +0x1A8 .. +0x1BF, NOT confirmed padding
	ParticleSystemDef_JUP* vehVfxDoorSmoke; // +0x1C0 pointer type confirmed; semantic name from IW8
	MaterialAnimationDef_JUP* materialAnim; // +0x1C8
};
static_assert(sizeof(GfxCamo_JUP) == 0x1D0);

struct Image_SetupData
{
	const char* data[16][1536];
};

struct Image_SetupParams_JUP
{
	int width;                              // +0x00
	int height;                             // +0x04
	int depth;                              // +0x08
	unsigned int numElements;               // +0x0C
	unsigned int maxLevelCount;             // +0x10 (0 = full mip chain)
	std::uint32_t flags;                    // +0x14 GfxImageFlags_JUP bits
	std::uint8_t format;                    // +0x18 GfxPixelFormat_JUP
	std::uint8_t padding19[3];              // +0x19
	unsigned int unknown1C;                 // +0x1C default 0; meaning unverified
	unsigned int unknown20;                 // +0x20 default 0; NOT an allocator pointer
	unsigned int initialResourceState;      // +0x24 default 0x8C0 for ordinary images
	std::uint8_t allowCrossAdapter;          // +0x28 adds D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER
	std::uint8_t padding29[7];              // +0x29
	std::uint64_t resourceMemory;            // +0x30 opaque allocator handle, low bit = valid
	std::uint8_t allocationFlag38;           // +0x38 allocator option; exact meaning unverified
	std::uint8_t allocationFlag39;           // +0x39 allocator option; exact meaning unverified
	std::uint8_t padding3A[6];              // +0x3A
};
static_assert(sizeof(Image_SetupParams_JUP) == 0x40);

//++++++++++++++++++++++++++++++
// en : XAsset header
// ja : XAssetヘッダー
//++++++++++++++++++++++++++++++
union XAssetHeader_JUP
{
	GfxImage_JUP* image;
	GfxCamo_JUP* camo;
	void* data;
};