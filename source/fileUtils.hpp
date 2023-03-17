#pragma once

#include <3ds.h>
#include <citro3d.h>
#include <dirent.h>
#include <stdio.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>

#include "ascii.hpp"

namespace FileUtils {

// sdmc:3dscii/config.ini file
class Config {
public:
  //TODO: language
  bool hintsEnabled = true;
  bool holdBumpers = true;
  bool keepBackups = true;
  bool remoteSendEnabled = false;
  std::string remoteSendIP = "0.0.0.0";

  std::string lastArtwork = "";

  void parse(u8* data);
  int serialize(u8* buf, int size);
};

std::unique_ptr<std::vector<std::string>> listFilesWithExt(std::string path, std::string_view ext);

// initializes the texture and loads image file into it
bool loadTexFile(std::string path, C3D_Tex* tex, u16* width, u16* height);
// reads image file into a sequence of unique colors, in order of appearance (order should be the same as Playscii's)
ASCII::Palette loadPaletteFile(std::string path);
// loads the charset from a Playscii-compatible .char file
ASCII::Charset loadCharsetFile(std::string path);

// loads the artwork from a .3dscii file
bool loadArtworkFile(std::string path, ASCII::Data* asciiData);
// saves the artwork into a .3dscii file
// if makeBackup is true and file with the same name is found, renames it into .3dscii.bakN before writing
bool saveArtworkFile(std::string path, ASCII::Data* asciiData, bool makeBackup);

// loads the config from a .ini file
bool loadConfigFile(std::string path, Config* cfg);
// saves the config into a .ini file
bool saveConfigFile(std::string path, Config* cfg);

};
