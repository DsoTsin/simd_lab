#include "BitmapWriter.h"

#include <fstream>
#include <stdint.h>

__pragma(pack(push, 1)) struct BMPFileHeader {
  uint16_t file_type{0x4D42}; // File type always BM which is 0x4D42
  uint32_t file_size{0};      // Size of the file (in bytes)
  uint16_t reserved1{0};      // Reserved, always 0
  uint16_t reserved2{0};      // Reserved, always 0
  uint32_t offset_data{
      0}; // Start position of pixel data (bytes from the beginning of the file)
} __pragma(pack(pop));

static_assert(sizeof(BMPFileHeader) == 14, "");

__pragma(pack(push, 1)) struct BMPInfoHeader {
  uint32_t biSize{0};        // the size of BITMAPINFOHEADER
  uint32_t biWidth{0};       // width (pixels)
  uint32_t biHeight{0};      // height (pixels)
  uint16_t biPlanes{0};      // color planes
  uint16_t biBitCount{0};    // bits per pixel
  uint32_t biCompression{0}; // type of compression (0 is no compression)
  uint32_t biSizeImage{
      0}; // the origin size of the bitmap data (before compression)
  uint32_t biXPelsPerMeter{0}; // horizontal pixels per meter
  uint32_t biYPelsPerMeter{0}; // vertical pixels per meter
  uint32_t biClrUsed{0};       // the number of colors used
  uint32_t biClrImportant{0};  // "important" colors, usually 0
} __pragma(pack(pop));

static_assert(sizeof(BMPInfoHeader) == 40, "");

class __BitmapWriterPrivate {
public:
  __BitmapWriterPrivate(int width, int height, const char *fileName);
  ~__BitmapWriterPrivate();

  void setPixel(int x, int y, const BmPixel &p) {
    pixels_[y * info_.biWidth + x] = p;
  }

  void flush();

private:
  friend class BitmapWriter;
  BMPFileHeader header_;
  BMPInfoHeader info_;
  BmPixel *pixels_;
  std::ofstream f_;
};

BitmapWriter::BitmapWriter(int width, int height, const char *fileName)
    : impl_(new __BitmapWriterPrivate(width, height, fileName)) {}

BitmapWriter::~BitmapWriter() { delete impl_; }

unsigned char *BitmapWriter::cache() { return (unsigned char*)impl_->pixels_; }

void BitmapWriter::setPixel(int x, int y, const BmPixel &p) {
  impl_->setPixel(x, y, p);
}

void BitmapWriter::flush() { impl_->flush(); }

BmPixel BmPixel::fromFloat(float illum) { return BmPixel(); }

__BitmapWriterPrivate::__BitmapWriterPrivate(int width, int height,
                                             const char *fileName) {
  pixels_ = new BmPixel[width * height];

  info_.biWidth = width;
  info_.biHeight = height;
  info_.biBitCount = 24;
  info_.biSizeImage = 3 * width * height;
  info_.biSize = sizeof(BMPInfoHeader);

  header_.file_size = 54 + info_.biSizeImage;

  f_.open(fileName, std::ios::binary | std::ios::out);
}

__BitmapWriterPrivate::~__BitmapWriterPrivate() {
  delete[] pixels_;
}

void __BitmapWriterPrivate::flush() {
  f_.write((const char *)&header_, sizeof(header_));
  f_.write((const char *)&info_, sizeof(info_));
  f_.write((const char *)pixels_, info_.biSizeImage);
  f_.close();
}
