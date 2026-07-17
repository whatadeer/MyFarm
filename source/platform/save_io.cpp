#include "platform/save_io.h"

#include <cstdint>
#include <cstdio>
#include <vector>

#include <sys/stat.h>

#include "core/save.h"

namespace platform {

namespace {

// mkdir() fails (harmlessly) if a path segment already exists - fopen()
// won't create missing parent directories itself, so this has to run
// before the first write.
void ensureSaveDir() {
    mkdir("sdmc:/3ds", 0777);
    mkdir(kSaveDir, 0777);
}

} // namespace

bool saveToDisk(const core::GameState& state) {
    ensureSaveDir();

    std::vector<uint8_t> bytes = core::serializeSave(state);

    FILE* f = fopen(kSavePath, "wb");
    if (!f) return false;

    size_t written = fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    return written == bytes.size();
}

bool loadFromDisk(core::GameState* outState) {
    FILE* f = fopen(kSavePath, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return false;
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    size_t readBytes = fread(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    if (readBytes != bytes.size()) return false;

    return core::deserializeSave(bytes, outState);
}

} // namespace platform
