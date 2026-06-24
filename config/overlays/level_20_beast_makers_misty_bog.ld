SECTIONS
{
    __romPos = 0;
    level_20_beast_makers_misty_bog_ROM_START = __romPos;
    level_20_beast_makers_misty_bog_VRAM = ADDR(.level_20_beast_makers_misty_bog);
    .level_20_beast_makers_misty_bog 0x8007AA38 : AT(level_20_beast_makers_misty_bog_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_20_beast_makers_misty_bog_RODATA_START = .;
        build/overlays/level_20_beast_makers_misty_bog/asm/overlays/level_20_beast_makers_misty_bog/data/rodata.rodata.o(.rodata);
        build/overlays/level_20_beast_makers_misty_bog/asm/overlays/level_20_beast_makers_misty_bog/text.o(.rodata);
        level_20_beast_makers_misty_bog_RODATA_END = .;
        level_20_beast_makers_misty_bog_RODATA_SIZE = ABSOLUTE(level_20_beast_makers_misty_bog_RODATA_END - level_20_beast_makers_misty_bog_RODATA_START);
        level_20_beast_makers_misty_bog_TEXT_START = .;
        build/overlays/level_20_beast_makers_misty_bog/asm/overlays/level_20_beast_makers_misty_bog/text.o(.text);
        level_20_beast_makers_misty_bog_TEXT_END = .;
        level_20_beast_makers_misty_bog_TEXT_SIZE = ABSOLUTE(level_20_beast_makers_misty_bog_TEXT_END - level_20_beast_makers_misty_bog_TEXT_START);
        level_20_beast_makers_misty_bog_DATA_START = .;
        build/overlays/level_20_beast_makers_misty_bog/asm/overlays/level_20_beast_makers_misty_bog/data/rodata.rodata.o(.data);
        build/overlays/level_20_beast_makers_misty_bog/asm/overlays/level_20_beast_makers_misty_bog/text.o(.data);
        build/overlays/level_20_beast_makers_misty_bog/asm/overlays/level_20_beast_makers_misty_bog/data/data.data.o(.data);
        level_20_beast_makers_misty_bog_DATA_END = .;
        level_20_beast_makers_misty_bog_DATA_SIZE = ABSOLUTE(level_20_beast_makers_misty_bog_DATA_END - level_20_beast_makers_misty_bog_DATA_START);
        level_20_beast_makers_misty_bog_BSS_START = .;
        build/overlays/level_20_beast_makers_misty_bog/asm/overlays/level_20_beast_makers_misty_bog/data/rodata.rodata.o(.bss);
        build/overlays/level_20_beast_makers_misty_bog/asm/overlays/level_20_beast_makers_misty_bog/text.o(.bss);
        level_20_beast_makers_misty_bog_BSS_END = .;
        level_20_beast_makers_misty_bog_BSS_SIZE = ABSOLUTE(level_20_beast_makers_misty_bog_BSS_END - level_20_beast_makers_misty_bog_BSS_START);
    }
    __romPos += SIZEOF(.level_20_beast_makers_misty_bog);
    __romPos = ALIGN(__romPos, 16);
    level_20_beast_makers_misty_bog_ROM_END = __romPos;
    level_20_beast_makers_misty_bog_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
