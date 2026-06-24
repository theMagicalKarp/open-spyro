SECTIONS
{
    __romPos = 0;
    level_9_peace_keepers_ice_cavern_ROM_START = __romPos;
    level_9_peace_keepers_ice_cavern_VRAM = ADDR(.level_9_peace_keepers_ice_cavern);
    .level_9_peace_keepers_ice_cavern 0x8007AA38 : AT(level_9_peace_keepers_ice_cavern_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_9_peace_keepers_ice_cavern_RODATA_START = .;
        build/overlays/level_9_peace_keepers_ice_cavern/asm/overlays/level_9_peace_keepers_ice_cavern/data/rodata.rodata.o(.rodata);
        build/overlays/level_9_peace_keepers_ice_cavern/asm/overlays/level_9_peace_keepers_ice_cavern/text.o(.rodata);
        level_9_peace_keepers_ice_cavern_RODATA_END = .;
        level_9_peace_keepers_ice_cavern_RODATA_SIZE = ABSOLUTE(level_9_peace_keepers_ice_cavern_RODATA_END - level_9_peace_keepers_ice_cavern_RODATA_START);
        level_9_peace_keepers_ice_cavern_TEXT_START = .;
        build/overlays/level_9_peace_keepers_ice_cavern/asm/overlays/level_9_peace_keepers_ice_cavern/text.o(.text);
        level_9_peace_keepers_ice_cavern_TEXT_END = .;
        level_9_peace_keepers_ice_cavern_TEXT_SIZE = ABSOLUTE(level_9_peace_keepers_ice_cavern_TEXT_END - level_9_peace_keepers_ice_cavern_TEXT_START);
        level_9_peace_keepers_ice_cavern_DATA_START = .;
        build/overlays/level_9_peace_keepers_ice_cavern/asm/overlays/level_9_peace_keepers_ice_cavern/data/rodata.rodata.o(.data);
        build/overlays/level_9_peace_keepers_ice_cavern/asm/overlays/level_9_peace_keepers_ice_cavern/text.o(.data);
        build/overlays/level_9_peace_keepers_ice_cavern/asm/overlays/level_9_peace_keepers_ice_cavern/data/data.data.o(.data);
        level_9_peace_keepers_ice_cavern_DATA_END = .;
        level_9_peace_keepers_ice_cavern_DATA_SIZE = ABSOLUTE(level_9_peace_keepers_ice_cavern_DATA_END - level_9_peace_keepers_ice_cavern_DATA_START);
        level_9_peace_keepers_ice_cavern_BSS_START = .;
        build/overlays/level_9_peace_keepers_ice_cavern/asm/overlays/level_9_peace_keepers_ice_cavern/data/rodata.rodata.o(.bss);
        build/overlays/level_9_peace_keepers_ice_cavern/asm/overlays/level_9_peace_keepers_ice_cavern/text.o(.bss);
        level_9_peace_keepers_ice_cavern_BSS_END = .;
        level_9_peace_keepers_ice_cavern_BSS_SIZE = ABSOLUTE(level_9_peace_keepers_ice_cavern_BSS_END - level_9_peace_keepers_ice_cavern_BSS_START);
    }
    __romPos += SIZEOF(.level_9_peace_keepers_ice_cavern);
    __romPos = ALIGN(__romPos, 16);
    level_9_peace_keepers_ice_cavern_ROM_END = __romPos;
    level_9_peace_keepers_ice_cavern_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
