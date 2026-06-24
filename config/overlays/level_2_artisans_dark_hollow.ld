SECTIONS
{
    __romPos = 0;
    level_2_artisans_dark_hollow_ROM_START = __romPos;
    level_2_artisans_dark_hollow_VRAM = ADDR(.level_2_artisans_dark_hollow);
    .level_2_artisans_dark_hollow 0x8007AA38 : AT(level_2_artisans_dark_hollow_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_2_artisans_dark_hollow_RODATA_START = .;
        build/overlays/level_2_artisans_dark_hollow/asm/overlays/level_2_artisans_dark_hollow/data/rodata.rodata.o(.rodata);
        build/overlays/level_2_artisans_dark_hollow/asm/overlays/level_2_artisans_dark_hollow/text.o(.rodata);
        level_2_artisans_dark_hollow_RODATA_END = .;
        level_2_artisans_dark_hollow_RODATA_SIZE = ABSOLUTE(level_2_artisans_dark_hollow_RODATA_END - level_2_artisans_dark_hollow_RODATA_START);
        level_2_artisans_dark_hollow_TEXT_START = .;
        build/overlays/level_2_artisans_dark_hollow/asm/overlays/level_2_artisans_dark_hollow/text.o(.text);
        level_2_artisans_dark_hollow_TEXT_END = .;
        level_2_artisans_dark_hollow_TEXT_SIZE = ABSOLUTE(level_2_artisans_dark_hollow_TEXT_END - level_2_artisans_dark_hollow_TEXT_START);
        level_2_artisans_dark_hollow_DATA_START = .;
        build/overlays/level_2_artisans_dark_hollow/asm/overlays/level_2_artisans_dark_hollow/data/rodata.rodata.o(.data);
        build/overlays/level_2_artisans_dark_hollow/asm/overlays/level_2_artisans_dark_hollow/text.o(.data);
        build/overlays/level_2_artisans_dark_hollow/asm/overlays/level_2_artisans_dark_hollow/data/data.data.o(.data);
        level_2_artisans_dark_hollow_DATA_END = .;
        level_2_artisans_dark_hollow_DATA_SIZE = ABSOLUTE(level_2_artisans_dark_hollow_DATA_END - level_2_artisans_dark_hollow_DATA_START);
        level_2_artisans_dark_hollow_BSS_START = .;
        build/overlays/level_2_artisans_dark_hollow/asm/overlays/level_2_artisans_dark_hollow/data/rodata.rodata.o(.bss);
        build/overlays/level_2_artisans_dark_hollow/asm/overlays/level_2_artisans_dark_hollow/text.o(.bss);
        level_2_artisans_dark_hollow_BSS_END = .;
        level_2_artisans_dark_hollow_BSS_SIZE = ABSOLUTE(level_2_artisans_dark_hollow_BSS_END - level_2_artisans_dark_hollow_BSS_START);
    }
    __romPos += SIZEOF(.level_2_artisans_dark_hollow);
    __romPos = ALIGN(__romPos, 16);
    level_2_artisans_dark_hollow_ROM_END = __romPos;
    level_2_artisans_dark_hollow_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
