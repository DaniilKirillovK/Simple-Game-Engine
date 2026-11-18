#include "Texture.h"
#include <libs/stb_image/stb_image.h>

void TextureData::cleanup()
{
	if (pixels)
	{
		stbi_image_free(pixels);
		pixels = nullptr;
	}
}
