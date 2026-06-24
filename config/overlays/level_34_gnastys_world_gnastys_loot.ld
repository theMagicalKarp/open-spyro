SECTIONS
{
    __romPos = 0;
    level_34_gnastys_world_gnastys_loot_ROM_START = __romPos;
    level_34_gnastys_world_gnastys_loot_VRAM = ADDR(.level_34_gnastys_world_gnastys_loot);
    .level_34_gnastys_world_gnastys_loot 0x8007AA38 : AT(level_34_gnastys_world_gnastys_loot_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_34_gnastys_world_gnastys_loot_RODATA_START = .;
        build/overlays/level_34_gnastys_world_gnastys_loot/asm/overlays/level_34_gnastys_world_gnastys_loot/data/rodata.rodata.o(.rodata);
        build/overlays/level_34_gnastys_world_gnastys_loot/asm/overlays/level_34_gnastys_world_gnastys_loot/text.o(.rodata);
        level_34_gnastys_world_gnastys_loot_RODATA_END = .;
        level_34_gnastys_world_gnastys_loot_RODATA_SIZE = ABSOLUTE(level_34_gnastys_world_gnastys_loot_RODATA_END - level_34_gnastys_world_gnastys_loot_RODATA_START);
        level_34_gnastys_world_gnastys_loot_TEXT_START = .;
        build/overlays/level_34_gnastys_world_gnastys_loot/asm/overlays/level_34_gnastys_world_gnastys_loot/text.o(.text);
        level_34_gnastys_world_gnastys_loot_TEXT_END = .;
        level_34_gnastys_world_gnastys_loot_TEXT_SIZE = ABSOLUTE(level_34_gnastys_world_gnastys_loot_TEXT_END - level_34_gnastys_world_gnastys_loot_TEXT_START);
        level_34_gnastys_world_gnastys_loot_DATA_START = .;
        build/overlays/level_34_gnastys_world_gnastys_loot/asm/overlays/level_34_gnastys_world_gnastys_loot/data/rodata.rodata.o(.data);
        build/overlays/level_34_gnastys_world_gnastys_loot/asm/overlays/level_34_gnastys_world_gnastys_loot/text.o(.data);
        build/overlays/level_34_gnastys_world_gnastys_loot/asm/overlays/level_34_gnastys_world_gnastys_loot/data/data.data.o(.data);
        level_34_gnastys_world_gnastys_loot_DATA_END = .;
        level_34_gnastys_world_gnastys_loot_DATA_SIZE = ABSOLUTE(level_34_gnastys_world_gnastys_loot_DATA_END - level_34_gnastys_world_gnastys_loot_DATA_START);
        level_34_gnastys_world_gnastys_loot_BSS_START = .;
        build/overlays/level_34_gnastys_world_gnastys_loot/asm/overlays/level_34_gnastys_world_gnastys_loot/data/rodata.rodata.o(.bss);
        build/overlays/level_34_gnastys_world_gnastys_loot/asm/overlays/level_34_gnastys_world_gnastys_loot/text.o(.bss);
        level_34_gnastys_world_gnastys_loot_BSS_END = .;
        level_34_gnastys_world_gnastys_loot_BSS_SIZE = ABSOLUTE(level_34_gnastys_world_gnastys_loot_BSS_END - level_34_gnastys_world_gnastys_loot_BSS_START);
    }
    __romPos += SIZEOF(.level_34_gnastys_world_gnastys_loot);
    __romPos = ALIGN(__romPos, 16);
    level_34_gnastys_world_gnastys_loot_ROM_END = __romPos;
    level_34_gnastys_world_gnastys_loot_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
