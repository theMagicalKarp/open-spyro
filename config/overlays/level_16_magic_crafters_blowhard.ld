SECTIONS
{
    __romPos = 0;
    level_16_magic_crafters_blowhard_ROM_START = __romPos;
    level_16_magic_crafters_blowhard_VRAM = ADDR(.level_16_magic_crafters_blowhard);
    .level_16_magic_crafters_blowhard 0x8007AA38 : AT(level_16_magic_crafters_blowhard_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_16_magic_crafters_blowhard_RODATA_START = .;
        build/overlays/level_16_magic_crafters_blowhard/asm/overlays/level_16_magic_crafters_blowhard/data/rodata.rodata.o(.rodata);
        build/overlays/level_16_magic_crafters_blowhard/asm/overlays/level_16_magic_crafters_blowhard/text.o(.rodata);
        level_16_magic_crafters_blowhard_RODATA_END = .;
        level_16_magic_crafters_blowhard_RODATA_SIZE = ABSOLUTE(level_16_magic_crafters_blowhard_RODATA_END - level_16_magic_crafters_blowhard_RODATA_START);
        level_16_magic_crafters_blowhard_TEXT_START = .;
        build/overlays/level_16_magic_crafters_blowhard/asm/overlays/level_16_magic_crafters_blowhard/text.o(.text);
        level_16_magic_crafters_blowhard_TEXT_END = .;
        level_16_magic_crafters_blowhard_TEXT_SIZE = ABSOLUTE(level_16_magic_crafters_blowhard_TEXT_END - level_16_magic_crafters_blowhard_TEXT_START);
        level_16_magic_crafters_blowhard_DATA_START = .;
        build/overlays/level_16_magic_crafters_blowhard/asm/overlays/level_16_magic_crafters_blowhard/data/rodata.rodata.o(.data);
        build/overlays/level_16_magic_crafters_blowhard/asm/overlays/level_16_magic_crafters_blowhard/text.o(.data);
        build/overlays/level_16_magic_crafters_blowhard/asm/overlays/level_16_magic_crafters_blowhard/data/data.data.o(.data);
        level_16_magic_crafters_blowhard_DATA_END = .;
        level_16_magic_crafters_blowhard_DATA_SIZE = ABSOLUTE(level_16_magic_crafters_blowhard_DATA_END - level_16_magic_crafters_blowhard_DATA_START);
        level_16_magic_crafters_blowhard_BSS_START = .;
        build/overlays/level_16_magic_crafters_blowhard/asm/overlays/level_16_magic_crafters_blowhard/data/rodata.rodata.o(.bss);
        build/overlays/level_16_magic_crafters_blowhard/asm/overlays/level_16_magic_crafters_blowhard/text.o(.bss);
        level_16_magic_crafters_blowhard_BSS_END = .;
        level_16_magic_crafters_blowhard_BSS_SIZE = ABSOLUTE(level_16_magic_crafters_blowhard_BSS_END - level_16_magic_crafters_blowhard_BSS_START);
    }
    __romPos += SIZEOF(.level_16_magic_crafters_blowhard);
    __romPos = ALIGN(__romPos, 16);
    level_16_magic_crafters_blowhard_ROM_END = __romPos;
    level_16_magic_crafters_blowhard_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
