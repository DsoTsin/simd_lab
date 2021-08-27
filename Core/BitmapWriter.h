#pragma once

class __BitmapWriterPrivate;

struct BmPixel
{
	unsigned char R;
	unsigned char G;
	unsigned char B;

	static BmPixel fromFloat(float illum);
};

class BitmapWriter {
public:
	BitmapWriter(int width, int height, const char* fileName);
	~BitmapWriter();

	unsigned char* cache();

	void setPixel(int x, int y, const BmPixel& p);
	void flush();

private:
  __BitmapWriterPrivate *impl_;
};