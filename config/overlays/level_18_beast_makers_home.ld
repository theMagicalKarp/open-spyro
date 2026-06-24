SECTIONS
{
    __romPos = 0;
    level_18_beast_makers_home_ROM_START = __romPos;
    level_18_beast_makers_home_VRAM = ADDR(.level_18_beast_makers_home);
    .level_18_beast_makers_home 0x8007AA38 : AT(level_18_beast_makers_home_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_18_beast_makers_home_RODATA_START = .;
        build/overlays/level_18_beast_makers_home/asm/overlays/level_18_beast_makers_home/data/rodata.rodata.o(.rodata);
        build/overlays/level_18_beast_makers_home/asm/overlays/level_18_beast_makers_home/text.o(.rodata);
        level_18_beast_makers_home_RODATA_END = .;
        level_18_beast_makers_home_RODATA_SIZE = ABSOLUTE(level_18_beast_makers_home_RODATA_END - level_18_beast_makers_home_RODATA_START);
        level_18_beast_makers_home_TEXT_START = .;
        build/overlays/level_18_beast_makers_home/asm/overlays/level_18_beast_makers_home/text.o(.text);
        level_18_beast_makers_home_TEXT_END = .;
        level_18_beast_makers_home_TEXT_SIZE = ABSOLUTE(level_18_beast_makers_home_TEXT_END - level_18_beast_makers_home_TEXT_START);
        level_18_beast_makers_home_DATA_START = .;
        build/overlays/level_18_beast_makers_home/asm/overlays/level_18_beast_makers_home/data/rodata.rodata.o(.data);
        build/overlays/level_18_beast_makers_home/asm/overlays/level_18_beast_makers_home/text.o(.data);
        build/overlays/level_18_beast_makers_home/asm/overlays/level_18_beast_makers_home/data/data.data.o(.data);
        level_18_beast_makers_home_DATA_END = .;
        level_18_beast_makers_home_DATA_SIZE = ABSOLUTE(level_18_beast_makers_home_DATA_END - level_18_beast_makers_home_DATA_START);
        level_18_beast_makers_home_BSS_START = .;
        build/overlays/level_18_beast_makers_home/asm/overlays/level_18_beast_makers_home/data/rodata.rodata.o(.bss);
        build/overlays/level_18_beast_makers_home/asm/overlays/level_18_beast_makers_home/text.o(.bss);
        level_18_beast_makers_home_BSS_END = .;
        level_18_beast_makers_home_BSS_SIZE = ABSOLUTE(level_18_beast_makers_home_BSS_END - level_18_beast_makers_home_BSS_START);
    }
    __romPos += SIZEOF(.level_18_beast_makers_home);
    __romPos = ALIGN(__romPos, 16);
    level_18_beast_makers_home_ROM_END = __romPos;
    level_18_beast_makers_home_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
