#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "light3d/texture.h"

namespace light3d {

Image loadImage(const std::string& filepath, int desiredChannels) {
    Image img;
    unsigned char* pixels = stbi_load(filepath.c_str(), &img.width, &img.height,
                                      &img.channels, desiredChannels);
    if (!pixels) {
        img.width = 0;
        img.height = 0;
        img.channels = 0;
        return img;
    }

    int actualChannels = (desiredChannels > 0) ? desiredChannels : img.channels;
    size_t byteCount = static_cast<size_t>(img.width) * img.height * actualChannels;
    img.data.assign(pixels, pixels + byteCount);
    if (desiredChannels > 0) {
        img.channels = desiredChannels;
    }

    stbi_image_free(pixels);
    return img;
}

// --- TextureLibrary ---

int TextureLibrary::addTexture(Image image, const std::string& name) {
    int id = static_cast<int>(textures_.size());
    if (!name.empty()) {
        nameToIndex_[name] = id;
    }
    textures_.push_back(std::move(image));
    return id;
}

const Image* TextureLibrary::getTexture(int id) const {
    if (id < 0 || id >= static_cast<int>(textures_.size())) return nullptr;
    return &textures_[id];
}

Image* TextureLibrary::getTexture(int id) {
    if (id < 0 || id >= static_cast<int>(textures_.size())) return nullptr;
    return &textures_[id];
}

const Image* TextureLibrary::findByName(const std::string& name) const {
    auto it = nameToIndex_.find(name);
    if (it == nameToIndex_.end()) return nullptr;
    return &textures_[it->second];
}

int TextureLibrary::findIdByName(const std::string& name) const {
    auto it = nameToIndex_.find(name);
    if (it == nameToIndex_.end()) return -1;
    return it->second;
}

} // namespace light3d
