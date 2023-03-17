// for some reason, Citra doesn't like ifstream and 
// claims to be reading oob when we read files cpp way
// doesn't crash though, so it should be fine?..

#include "fileUtils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

// we like warnings
// but only when they're for our own code
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wsign-compare"
#define INI_IMPLEMENTATION
#include "ini.h"
#pragma GCC diagnostic pop

#define CONFIG_INI_MAXSIZE 2048

namespace FileUtils {

bool iniReadBool(ini_t* ini, const char* name, bool defaultVal) {
	int idx = ini_find_property(ini, INI_GLOBAL_SECTION, name, 0);
	if (idx == INI_NOT_FOUND) 
		return defaultVal;

	char const* val = ini_property_value(ini, INI_GLOBAL_SECTION, idx);
	return val[0] != '0';
}

std::string iniReadString(ini_t* ini, const char* name, std::string defaultVal) {
	int idx = ini_find_property(ini, INI_GLOBAL_SECTION, name, 0);
	if (idx == INI_NOT_FOUND) 
		return defaultVal;

	char const* val = ini_property_value(ini, INI_GLOBAL_SECTION, idx);
	return std::string(val);
}

void Config::parse(u8* data) {
	ini_t* ini = ini_load((char*)data, NULL);

	hintsEnabled = iniReadBool(ini, "hintsEnabled", true);
	holdBumpers = iniReadBool(ini, "holdBumpers", true);
	keepBackups = iniReadBool(ini, "keepBackups", true);
	remoteSendEnabled = iniReadBool(ini, "remoteSendEnabled", false);
	remoteSendIP = iniReadString(ini, "remoteSendIP", "0.0.0.0");

	lastArtwork = iniReadString(ini, "lastArtwork", "");

	ini_destroy(ini);
};

int Config::serialize(u8* buf, int size) {
	ini_t* ini = ini_create(NULL);

	ini_property_add(ini, INI_GLOBAL_SECTION, "hintsEnabled", 0, std::to_string((int)hintsEnabled).c_str(), 0);
	ini_property_add(ini, INI_GLOBAL_SECTION, "holdBumpers", 0, std::to_string((int)holdBumpers).c_str(), 0);
	ini_property_add(ini, INI_GLOBAL_SECTION, "keepBackups", 0, std::to_string((int)keepBackups).c_str(), 0);
	ini_property_add(ini, INI_GLOBAL_SECTION, "remoteSendEnabled", 0, std::to_string((int)remoteSendEnabled).c_str(), 0);
	ini_property_add(ini, INI_GLOBAL_SECTION, "remoteSendIP", 0, remoteSendIP.c_str(), 0);

	ini_property_add(ini, INI_GLOBAL_SECTION, "lastArtwork", 0, lastArtwork.c_str(), 0);

	int len = ini_save(ini, (char*)buf, size);
	ini_destroy(ini);

	return len;
}

std::unique_ptr<std::vector<std::string>> listFilesWithExt(std::string path, std::string_view ext) {
	auto out = std::make_unique<std::vector<std::string>>();
	
	DIR* dir;
	dir = opendir(path.c_str());
	if (dir == NULL) 
		return out;
	
	struct dirent* ent;
	while ((ent = readdir(dir)) != NULL) {
		std::string fileName(ent->d_name);
		std::size_t extPos = fileName.find_last_of('.');

		if (extPos == std::string::npos) 
			continue;
		if (fileName.substr(extPos + 1).compare(ext) != 0) 
			continue;

		out.get()->push_back(path + fileName);
	}

	closedir(dir);

	return out;
}

// TODO: stbi has load_from_file, probably should use it or ifstream instead
bool loadTexFile(std::string path, C3D_Tex* tex, u16* width, u16* height) {
	FILE* file = fopen(path.c_str(), "rb");
	if (file == NULL) 
		return false;

	fseek(file, 0, SEEK_END);
	off_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	auto data = std::make_unique<u8[]>(size);
	fread(data.get(), 1, size, file);
	fclose(file);

	int w, h, n;
	u8* pxls = stbi_load_from_memory(data.get(), size, &w, &h, &n, 4);
	*width = (u16)w;
	*height = (u16)h;

	int po2W;
	int po2H;

	long long log = std::log2(w);
	if (std::pow(2, log) == w) {
		po2W = w;
	} else {
		po2W = std::pow(2, log + 1);
	}
	log = std::log2(h);
	if (std::pow(2, log) == h) {
		po2H = h;
	} else {
		po2H = std::pow(2, log + 1);
	}
	
	u8* gpuPxls = (u8*)linearAlloc(po2W * po2H * 4);
	GSPGPU_FlushDataCache(gpuPxls, po2W * po2H * 4);

	u8* src = pxls;
	u8* dst = gpuPxls;
	for (int y = 0; y < po2H; ++y) {
		for (int x = 0; x < po2W; ++x) {
			if (x >= w || y >= h) {
				*dst++ = 0;
				*dst++ = 0;
				*dst++ = 0;
				*dst++ = 0;
			} else {
				u8 r = *src++;
				u8 g = *src++;
				u8 b = *src++;
				u8 a = *src++;

				*dst++ = a;
				*dst++ = b;
				*dst++ = g;
				*dst++ = r;
			}
		}
	}
	// for (int i = 0; i < w * h; ++i) {
	// 	u8 r = *src++;
	// 	u8 g = *src++;
	// 	u8 b = *src++;
	// 	u8 a = *src++;

	// 	*dst++ = a;
	// 	*dst++ = b;
	// 	*dst++ = g;
	// 	*dst++ = r;
	// }

	stbi_image_free(pxls);

	C3D_TexInit(tex, po2W, po2H, GPU_RGBA8);
	C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexFlush(tex);

	// for some reason, transfering (w, h) sized buffer into (po2W, po2H)
	// doesnt work on a real console (but works fine on Citra)
	// it's dumb, but for now we'll just store data 
	// into a (po2W, po2H) buffer before transfering it
	C3D_SyncDisplayTransfer((u32*)gpuPxls, GX_BUFFER_DIM(po2W, po2H), (u32*)tex->data, GX_BUFFER_DIM(po2W, po2H),
		GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
		GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
		GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
	);

	linearFree(gpuPxls);

	return true;
}

// TODO: same as loadTexFile's
ASCII::Palette loadPaletteFile(std::string path) {
	ASCII::Palette out;

	FILE* file = fopen(path.c_str(), "rb");
	if (file == NULL) 
		return out;

	fseek(file, 0, SEEK_END);
	off_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	auto data = std::make_unique<u8[]>(size);
	fread(data.get(), 1, size, file);
	fclose(file);

	int w, h, n;
	u8* pxls = stbi_load_from_memory(data.get(), size, &w, &h, &n, 4);
	
	// we need an opaque color regardles of if palette provides one
	out.push({
		r: 0,
		g: 0,
		b: 0,
		a: 0
	});
	
	u8* pxl = pxls;
	for (int i = 0; i < w * h; i++) {
		out.push({
			r: *pxl++,
			g: *pxl++,
			b: *pxl++,
			a: *pxl++
		});
	}

	stbi_image_free(pxls);

	return out;
}

ASCII::Charset loadCharsetFile(std::string path) {
	ASCII::Charset out;

	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open()) 
		return out;

	int state = 0;
	std::string line;
	while (!file.eof() && state < 2) {
		std::getline(file, line);
		if (line.substr(0, 2).compare("//") == 0) 
			continue;
		
		switch(state) {
			case 0:
				out.tilesetName = line;
				state++;
			break;
			case 1: {
				out.width = atoi(line.c_str());
				out.height = atoi(line.substr(line.find(',') + 1).c_str());
				state++;
			}
			break;
			case 2: 
				// we don't currently need to parse characters for anything
			break;
		}
	}

	return out;
}

bool loadArtworkFile(std::string path, ASCII::Data* asciiData) {
	std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) 
		return false;

  file.seekg(0, file.end);
  int size = file.tellg();
  file.seekg(0, file.beg);

  u8* data = (u8*)malloc(size);
  file.read((char*)data, size);
  file.close();

	bool res = asciiData->parse(data, size);
  free(data);

	return res;
}

bool saveArtworkFile(std::string path, ASCII::Data* asciiData, bool makeBackup) {
	if (makeBackup) {
		size_t lastSlash = path.find_last_of('/');
		if (lastSlash == path.npos)
			return false;
		
		std::string name = path.substr(lastSlash + 1);
		std::string folder = path.substr(0, lastSlash + 1);
		
		std::string bakName = name;
    bakName += ".bak";
  	int lastBak = 0;

		if (std::filesystem::exists(path.c_str())) {
			DIR* dir;
			dir = opendir(folder.c_str());
			if (dir == NULL) 
				return false;
			
			struct dirent* ent;
			struct stat entStat;
			while ((ent = readdir(dir)) != NULL) {
				std::string fileName(ent->d_name);
				if (fileName.substr(0, bakName.length()).compare(bakName) != 0)
					continue;
				
				stat(ent->d_name, &entStat);
				if (S_ISDIR(entStat.st_mode))
					continue;
				
				int bak = atoi(fileName.substr(bakName.length()).c_str());
				if (bak > lastBak)
					lastBak = bak;
			}

			closedir(dir);

			std::string bakPath = folder;
			bakPath += name;
			bakPath += ".bak";
			bakPath += std::to_string(lastBak + 1);
			std::rename(path.c_str(), bakPath.c_str());
		}
	}

	u8* data;
  int len = asciiData->serialize(data);

  std::ofstream file(path, std::ios::binary);
  file.write((char*)data, len);
  file.close();
  free(data);
	
	return true;
}

bool loadConfigFile(std::string path, Config* cfg) {
	std::ifstream file(path.c_str());
	if (!file.is_open()) 
		return false;

	file.seekg(0, file.end);
  int size = file.tellg();
  file.seekg(0, file.beg);

	u8* data = (u8*)malloc(size);
	file.read((char*)data, size);
  file.close();

	cfg->parse(data);
	free(data);

	return true;
}

bool saveConfigFile(std::string path, Config* cfg) {
	u8* data = (u8*)malloc(CONFIG_INI_MAXSIZE);
	// last symbol is the null terminator, and we don't need it in our .ini file
	int len = cfg->serialize(data, CONFIG_INI_MAXSIZE) - 1;

  std::ofstream file(path);
  file.write((char*)data, len);
  file.close();
  free(data);

	return true;
}

}