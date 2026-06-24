SECTIONS
{
    __romPos = 0;
    level_13_magic_crafters_alpine_ridge_ROM_START = __romPos;
    level_13_magic_crafters_alpine_ridge_VRAM = ADDR(.level_13_magic_crafters_alpine_ridge);
    .level_13_magic_crafters_alpine_ridge 0x8007AA38 : AT(level_13_magic_crafters_alpine_ridge_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_13_magic_crafters_alpine_ridge_RODATA_START = .;
        build/overlays/level_13_magic_crafters_alpine_ridge/asm/overlays/level_13_magic_crafters_alpine_ridge/data/rodata.rodata.o(.rodata);
        build/overlays/level_13_magic_crafters_alpine_ridge/asm/overlays/level_13_magic_crafters_alpine_ridge/text.o(.rodata);
        level_13_magic_crafters_alpine_ridge_RODATA_END = .;
        level_13_magic_crafters_alpine_ridge_RODATA_SIZE = ABSOLUTE(level_13_magic_crafters_alpine_ridge_RODATA_END - level_13_magic_crafters_alpine_ridge_RODATA_START);
        level_13_magic_crafters_alpine_ridge_TEXT_START = .;
        build/overlays/level_13_magic_crafters_alpine_ridge/asm/overlays/level_13_magic_crafters_alpine_ridge/text.o(.text);
        level_13_magic_crafters_alpine_ridge_TEXT_END = .;
        level_13_magic_crafters_alpine_ridge_TEXT_SIZE = ABSOLUTE(level_13_magic_crafters_alpine_ridge_TEXT_END - level_13_magic_crafters_alpine_ridge_TEXT_START);
        level_13_magic_crafters_alpine_ridge_DATA_START = .;
        build/overlays/level_13_magic_crafters_alpine_ridge/asm/overlays/level_13_magic_crafters_alpine_ridge/data/rodata.rodata.o(.data);
        build/overlays/level_13_magic_crafters_alpine_ridge/asm/overlays/level_13_magic_crafters_alpine_ridge/text.o(.data);
        build/overlays/level_13_magic_crafters_alpine_ridge/asm/overlays/level_13_magic_crafters_alpine_ridge/data/data.data.o(.data);
        level_13_magic_crafters_alpine_ridge_DATA_END = .;
        level_13_magic_crafters_alpine_ridge_DATA_SIZE = ABSOLUTE(level_13_magic_crafters_alpine_ridge_DATA_END - level_13_magic_crafters_alpine_ridge_DATA_START);
        level_13_magic_crafters_alpine_ridge_BSS_START = .;
        build/overlays/level_13_magic_crafters_alpine_ridge/asm/overlays/level_13_magic_crafters_alpine_ridge/data/rodata.rodata.o(.bss);
        build/overlays/level_13_magic_crafters_alpine_ridge/asm/overlays/level_13_magic_crafters_alpine_ridge/text.o(.bss);
        level_13_magic_crafters_alpine_ridge_BSS_END = .;
        level_13_magic_crafters_alpine_ridge_BSS_SIZE = ABSOLUTE(level_13_magic_crafters_alpine_ridge_BSS_END - level_13_magic_crafters_alpine_ridge_BSS_START);
    }
    __romPos += SIZEOF(.level_13_magic_crafters_alpine_ridge);
    __romPos = ALIGN(__romPos, 16);
    level_13_magic_crafters_alpine_ridge_ROM_END = __romPos;
    level_13_magic_crafters_alpine_ridge_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
