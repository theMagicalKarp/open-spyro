SECTIONS
{
    __romPos = 0;
    level_7_peace_keepers_dry_canyon_ROM_START = __romPos;
    level_7_peace_keepers_dry_canyon_VRAM = ADDR(.level_7_peace_keepers_dry_canyon);
    .level_7_peace_keepers_dry_canyon 0x8007AA38 : AT(level_7_peace_keepers_dry_canyon_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_7_peace_keepers_dry_canyon_RODATA_START = .;
        build/overlays/level_7_peace_keepers_dry_canyon/asm/overlays/level_7_peace_keepers_dry_canyon/data/rodata.rodata.o(.rodata);
        build/overlays/level_7_peace_keepers_dry_canyon/asm/overlays/level_7_peace_keepers_dry_canyon/text.o(.rodata);
        level_7_peace_keepers_dry_canyon_RODATA_END = .;
        level_7_peace_keepers_dry_canyon_RODATA_SIZE = ABSOLUTE(level_7_peace_keepers_dry_canyon_RODATA_END - level_7_peace_keepers_dry_canyon_RODATA_START);
        level_7_peace_keepers_dry_canyon_TEXT_START = .;
        build/overlays/level_7_peace_keepers_dry_canyon/asm/overlays/level_7_peace_keepers_dry_canyon/text.o(.text);
        level_7_peace_keepers_dry_canyon_TEXT_END = .;
        level_7_peace_keepers_dry_canyon_TEXT_SIZE = ABSOLUTE(level_7_peace_keepers_dry_canyon_TEXT_END - level_7_peace_keepers_dry_canyon_TEXT_START);
        level_7_peace_keepers_dry_canyon_DATA_START = .;
        build/overlays/level_7_peace_keepers_dry_canyon/asm/overlays/level_7_peace_keepers_dry_canyon/data/rodata.rodata.o(.data);
        build/overlays/level_7_peace_keepers_dry_canyon/asm/overlays/level_7_peace_keepers_dry_canyon/text.o(.data);
        build/overlays/level_7_peace_keepers_dry_canyon/asm/overlays/level_7_peace_keepers_dry_canyon/data/data.data.o(.data);
        level_7_peace_keepers_dry_canyon_DATA_END = .;
        level_7_peace_keepers_dry_canyon_DATA_SIZE = ABSOLUTE(level_7_peace_keepers_dry_canyon_DATA_END - level_7_peace_keepers_dry_canyon_DATA_START);
        level_7_peace_keepers_dry_canyon_BSS_START = .;
        build/overlays/level_7_peace_keepers_dry_canyon/asm/overlays/level_7_peace_keepers_dry_canyon/data/rodata.rodata.o(.bss);
        build/overlays/level_7_peace_keepers_dry_canyon/asm/overlays/level_7_peace_keepers_dry_canyon/text.o(.bss);
        level_7_peace_keepers_dry_canyon_BSS_END = .;
        level_7_peace_keepers_dry_canyon_BSS_SIZE = ABSOLUTE(level_7_peace_keepers_dry_canyon_BSS_END - level_7_peace_keepers_dry_canyon_BSS_START);
    }
    __romPos += SIZEOF(.level_7_peace_keepers_dry_canyon);
    __romPos = ALIGN(__romPos, 16);
    level_7_peace_keepers_dry_canyon_ROM_END = __romPos;
    level_7_peace_keepers_dry_canyon_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
