#pragma once
#include <SDL3/SDL.h>
#include <string>

// Load a PNG (or any stb_image format) into a texture. Returns nullptr on failure.
// Textures use nearest-neighbour scaling, matching v4.43's SDL2 default.
SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path);

// Read a whole file into a string. Returns false if it can't be opened.
bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& data);
