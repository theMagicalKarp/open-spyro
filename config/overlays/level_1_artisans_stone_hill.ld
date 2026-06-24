SECTIONS
{
    __romPos = 0;
    level_1_artisans_stone_hill_ROM_START = __romPos;
    level_1_artisans_stone_hill_VRAM = ADDR(.level_1_artisans_stone_hill);
    .level_1_artisans_stone_hill 0x8007AA38 : AT(level_1_artisans_stone_hill_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_1_artisans_stone_hill_RODATA_START = .;
        build/overlays/level_1_artisans_stone_hill/asm/overlays/level_1_artisans_stone_hill/data/rodata.rodata.o(.rodata);
        build/overlays/level_1_artisans_stone_hill/asm/overlays/level_1_artisans_stone_hill/text.o(.rodata);
        level_1_artisans_stone_hill_RODATA_END = .;
        level_1_artisans_stone_hill_RODATA_SIZE = ABSOLUTE(level_1_artisans_stone_hill_RODATA_END - level_1_artisans_stone_hill_RODATA_START);
        level_1_artisans_stone_hill_TEXT_START = .;
        build/overlays/level_1_artisans_stone_hill/asm/overlays/level_1_artisans_stone_hill/text.o(.text);
        level_1_artisans_stone_hill_TEXT_END = .;
        level_1_artisans_stone_hill_TEXT_SIZE = ABSOLUTE(level_1_artisans_stone_hill_TEXT_END - level_1_artisans_stone_hill_TEXT_START);
        level_1_artisans_stone_hill_DATA_START = .;
        build/overlays/level_1_artisans_stone_hill/asm/overlays/level_1_artisans_stone_hill/data/rodata.rodata.o(.data);
        build/overlays/level_1_artisans_stone_hill/asm/overlays/level_1_artisans_stone_hill/text.o(.data);
        build/overlays/level_1_artisans_stone_hill/asm/overlays/level_1_artisans_stone_hill/data/data.data.o(.data);
        level_1_artisans_stone_hill_DATA_END = .;
        level_1_artisans_stone_hill_DATA_SIZE = ABSOLUTE(level_1_artisans_stone_hill_DATA_END - level_1_artisans_stone_hill_DATA_START);
        level_1_artisans_stone_hill_BSS_START = .;
        build/overlays/level_1_artisans_stone_hill/asm/overlays/level_1_artisans_stone_hill/data/rodata.rodata.o(.bss);
        build/overlays/level_1_artisans_stone_hill/asm/overlays/level_1_artisans_stone_hill/text.o(.bss);
        level_1_artisans_stone_hill_BSS_END = .;
        level_1_artisans_stone_hill_BSS_SIZE = ABSOLUTE(level_1_artisans_stone_hill_BSS_END - level_1_artisans_stone_hill_BSS_START);
    }
    __romPos += SIZEOF(.level_1_artisans_stone_hill);
    __romPos = ALIGN(__romPos, 16);
    level_1_artisans_stone_hill_ROM_END = __romPos;
    level_1_artisans_stone_hill_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
