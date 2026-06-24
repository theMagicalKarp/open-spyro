SECTIONS
{
    __romPos = 0;
    level_3_artisans_town_square_ROM_START = __romPos;
    level_3_artisans_town_square_VRAM = ADDR(.level_3_artisans_town_square);
    .level_3_artisans_town_square 0x8007AA38 : AT(level_3_artisans_town_square_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_3_artisans_town_square_RODATA_START = .;
        build/overlays/level_3_artisans_town_square/asm/overlays/level_3_artisans_town_square/data/rodata.rodata.o(.rodata);
        build/overlays/level_3_artisans_town_square/asm/overlays/level_3_artisans_town_square/text.o(.rodata);
        level_3_artisans_town_square_RODATA_END = .;
        level_3_artisans_town_square_RODATA_SIZE = ABSOLUTE(level_3_artisans_town_square_RODATA_END - level_3_artisans_town_square_RODATA_START);
        level_3_artisans_town_square_TEXT_START = .;
        build/overlays/level_3_artisans_town_square/asm/overlays/level_3_artisans_town_square/text.o(.text);
        level_3_artisans_town_square_TEXT_END = .;
        level_3_artisans_town_square_TEXT_SIZE = ABSOLUTE(level_3_artisans_town_square_TEXT_END - level_3_artisans_town_square_TEXT_START);
        level_3_artisans_town_square_DATA_START = .;
        build/overlays/level_3_artisans_town_square/asm/overlays/level_3_artisans_town_square/data/rodata.rodata.o(.data);
        build/overlays/level_3_artisans_town_square/asm/overlays/level_3_artisans_town_square/text.o(.data);
        build/overlays/level_3_artisans_town_square/asm/overlays/level_3_artisans_town_square/data/data.data.o(.data);
        level_3_artisans_town_square_DATA_END = .;
        level_3_artisans_town_square_DATA_SIZE = ABSOLUTE(level_3_artisans_town_square_DATA_END - level_3_artisans_town_square_DATA_START);
        level_3_artisans_town_square_BSS_START = .;
        build/overlays/level_3_artisans_town_square/asm/overlays/level_3_artisans_town_square/data/rodata.rodata.o(.bss);
        build/overlays/level_3_artisans_town_square/asm/overlays/level_3_artisans_town_square/text.o(.bss);
        level_3_artisans_town_square_BSS_END = .;
        level_3_artisans_town_square_BSS_SIZE = ABSOLUTE(level_3_artisans_town_square_BSS_END - level_3_artisans_town_square_BSS_START);
    }
    __romPos += SIZEOF(.level_3_artisans_town_square);
    __romPos = ALIGN(__romPos, 16);
    level_3_artisans_town_square_ROM_END = __romPos;
    level_3_artisans_town_square_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
