#include "Texture.h"
#include <glad/glad.h>
#include <libs/stb_image/stb_image.h> // здесь нужны только объявления (stbi_image_free); реализация stb подключена в TextureLoader.cpp

// Освобождение пикселей. Память выделена внутри stbi_load, поэтому и освобождать её нужно
// функцией stbi_image_free (раньше было delete[] — несовпадение аллокатора, неопределённое
// поведение). Важно для многопоточности: данные декодируются в воркере, а освобождаются
// в другом потоке, и ошибка в типе освобождения проявлялась бы случайно.
void TextureData::cleanup()
{
	if (pixels)
	{
		stbi_image_free(pixels);
		pixels = nullptr;
	}
}

uint32_t TextureData::getInternalFormat() const
{
	if (channels == 1) return GL_R8;
	if (channels == 2) return GL_RG8;
	if (channels == 3) return GL_RGB8;
	if (channels == 4) return GL_RGBA8;

	return 0;
}

uint32_t TextureData::getPixelFormat() const
{
	if (channels == 1) return GL_RED;
	if (channels == 2) return GL_RG;
	if (channels == 3) return GL_RGB;
	if (channels == 4) return GL_RGBA;

	return 0;
}

void Texture::bind(int slot) const
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, m_handle);
}

void Texture::unbind() const
{
	glBindTexture(GL_TEXTURE_2D, 0);
}
