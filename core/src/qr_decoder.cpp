#include "lumi_core.h"
#include "base32.h"
#include "uuid.h"
#include <string>
#include <vector>
#include <cstring>
#include <filesystem>

// ─── stb_image for QR image loading ──────────────────────────────
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "ReadBarcode.h"
#include "ImageView.h"
#include "ReaderOptions.h"

std::string decode_qr_from_file(const std::filesystem::path& image_path) {
    int w, h, ch;
    uint8_t* pixels = stbi_load(image_path.string().c_str(), &w, &h, &ch, 1);
    if (!pixels) {
        throw std::runtime_error("Cannot load image: " + image_path.string());
    }

    ZXing::ImageView view(pixels, w, h, ZXing::ImageFormat::Lum);

    ZXing::ReaderOptions opts;
    opts.setFormats(ZXing::BarcodeFormat::QRCode);
    opts.setTryHarder(true);

    auto result = ZXing::ReadBarcode(view, opts);
    stbi_image_free(pixels);

    if (!result.isValid()) {
        throw std::runtime_error("No QR code found in image");
    }

    return result.text();
}
