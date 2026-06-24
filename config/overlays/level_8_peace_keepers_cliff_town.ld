SECTIONS
{
    __romPos = 0;
    level_8_peace_keepers_cliff_town_ROM_START = __romPos;
    level_8_peace_keepers_cliff_town_VRAM = ADDR(.level_8_peace_keepers_cliff_town);
    .level_8_peace_keepers_cliff_town 0x8007AA38 : AT(level_8_peace_keepers_cliff_town_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_8_peace_keepers_cliff_town_RODATA_START = .;
        build/overlays/level_8_peace_keepers_cliff_town/asm/overlays/level_8_peace_keepers_cliff_town/data/rodata.rodata.o(.rodata);
        build/overlays/level_8_peace_keepers_cliff_town/asm/overlays/level_8_peace_keepers_cliff_town/text.o(.rodata);
        level_8_peace_keepers_cliff_town_RODATA_END = .;
        level_8_peace_keepers_cliff_town_RODATA_SIZE = ABSOLUTE(level_8_peace_keepers_cliff_town_RODATA_END - level_8_peace_keepers_cliff_town_RODATA_START);
        level_8_peace_keepers_cliff_town_TEXT_START = .;
        build/overlays/level_8_peace_keepers_cliff_town/asm/overlays/level_8_peace_keepers_cliff_town/text.o(.text);
        level_8_peace_keepers_cliff_town_TEXT_END = .;
        level_8_peace_keepers_cliff_town_TEXT_SIZE = ABSOLUTE(level_8_peace_keepers_cliff_town_TEXT_END - level_8_peace_keepers_cliff_town_TEXT_START);
        level_8_peace_keepers_cliff_town_DATA_START = .;
        build/overlays/level_8_peace_keepers_cliff_town/asm/overlays/level_8_peace_keepers_cliff_town/data/rodata.rodata.o(.data);
        build/overlays/level_8_peace_keepers_cliff_town/asm/overlays/level_8_peace_keepers_cliff_town/text.o(.data);
        build/overlays/level_8_peace_keepers_cliff_town/asm/overlays/level_8_peace_keepers_cliff_town/data/data.data.o(.data);
        level_8_peace_keepers_cliff_town_DATA_END = .;
        level_8_peace_keepers_cliff_town_DATA_SIZE = ABSOLUTE(level_8_peace_keepers_cliff_town_DATA_END - level_8_peace_keepers_cliff_town_DATA_START);
        level_8_peace_keepers_cliff_town_BSS_START = .;
        build/overlays/level_8_peace_keepers_cliff_town/asm/overlays/level_8_peace_keepers_cliff_town/data/rodata.rodata.o(.bss);
        build/overlays/level_8_peace_keepers_cliff_town/asm/overlays/level_8_peace_keepers_cliff_town/text.o(.bss);
        level_8_peace_keepers_cliff_town_BSS_END = .;
        level_8_peace_keepers_cliff_town_BSS_SIZE = ABSOLUTE(level_8_peace_keepers_cliff_town_BSS_END - level_8_peace_keepers_cliff_town_BSS_START);
    }
    __romPos += SIZEOF(.level_8_peace_keepers_cliff_town);
    __romPos = ALIGN(__romPos, 16);
    level_8_peace_keepers_cliff_town_ROM_END = __romPos;
    level_8_peace_keepers_cliff_town_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
