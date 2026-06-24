SECTIONS
{
    __romPos = 0;
    level_14_magic_crafters_high_caves_ROM_START = __romPos;
    level_14_magic_crafters_high_caves_VRAM = ADDR(.level_14_magic_crafters_high_caves);
    .level_14_magic_crafters_high_caves 0x8007AA38 : AT(level_14_magic_crafters_high_caves_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_14_magic_crafters_high_caves_RODATA_START = .;
        build/overlays/level_14_magic_crafters_high_caves/asm/overlays/level_14_magic_crafters_high_caves/data/rodata.rodata.o(.rodata);
        build/overlays/level_14_magic_crafters_high_caves/asm/overlays/level_14_magic_crafters_high_caves/text.o(.rodata);
        level_14_magic_crafters_high_caves_RODATA_END = .;
        level_14_magic_crafters_high_caves_RODATA_SIZE = ABSOLUTE(level_14_magic_crafters_high_caves_RODATA_END - level_14_magic_crafters_high_caves_RODATA_START);
        level_14_magic_crafters_high_caves_TEXT_START = .;
        build/overlays/level_14_magic_crafters_high_caves/asm/overlays/level_14_magic_crafters_high_caves/text.o(.text);
        level_14_magic_crafters_high_caves_TEXT_END = .;
        level_14_magic_crafters_high_caves_TEXT_SIZE = ABSOLUTE(level_14_magic_crafters_high_caves_TEXT_END - level_14_magic_crafters_high_caves_TEXT_START);
        level_14_magic_crafters_high_caves_DATA_START = .;
        build/overlays/level_14_magic_crafters_high_caves/asm/overlays/level_14_magic_crafters_high_caves/data/rodata.rodata.o(.data);
        build/overlays/level_14_magic_crafters_high_caves/asm/overlays/level_14_magic_crafters_high_caves/text.o(.data);
        build/overlays/level_14_magic_crafters_high_caves/asm/overlays/level_14_magic_crafters_high_caves/data/data.data.o(.data);
        level_14_magic_crafters_high_caves_DATA_END = .;
        level_14_magic_crafters_high_caves_DATA_SIZE = ABSOLUTE(level_14_magic_crafters_high_caves_DATA_END - level_14_magic_crafters_high_caves_DATA_START);
        level_14_magic_crafters_high_caves_BSS_START = .;
        build/overlays/level_14_magic_crafters_high_caves/asm/overlays/level_14_magic_crafters_high_caves/data/rodata.rodata.o(.bss);
        build/overlays/level_14_magic_crafters_high_caves/asm/overlays/level_14_magic_crafters_high_caves/text.o(.bss);
        level_14_magic_crafters_high_caves_BSS_END = .;
        level_14_magic_crafters_high_caves_BSS_SIZE = ABSOLUTE(level_14_magic_crafters_high_caves_BSS_END - level_14_magic_crafters_high_caves_BSS_START);
    }
    __romPos += SIZEOF(.level_14_magic_crafters_high_caves);
    __romPos = ALIGN(__romPos, 16);
    level_14_magic_crafters_high_caves_ROM_END = __romPos;
    level_14_magic_crafters_high_caves_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
