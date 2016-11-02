#pragma once

#include <cstdint>

struct FONT_CHAR_INFO 
{
	int WidthInBits;
	int Offset;
};

struct FONT_INFO
{
	int CharacterHeight; //  Character height
	char StartCharacter;  //  Start character
	char EndCharacter; //  End character
	int WidthInPixelsOfSpace; //  Width, in pixels, of space character
	const FONT_CHAR_INFO* Descriptor; //  Character descriptor array
	const uint8_t* Bitmaps; //  Character bitmap array
};

// Font data for DejaVu Sans Mono 8pt
extern const uint8_t dejaVuSansMono_8ptBitmaps[];
extern const FONT_INFO dejaVuSansMono_8ptFontInfo;
extern const FONT_CHAR_INFO dejaVuSansMono_8ptDescriptors[];
