SECTIONS
{
    __romPos = 0;
    level_32_gnastys_world_twilight_harbor_ROM_START = __romPos;
    level_32_gnastys_world_twilight_harbor_VRAM = ADDR(.level_32_gnastys_world_twilight_harbor);
    .level_32_gnastys_world_twilight_harbor 0x8007AA38 : AT(level_32_gnastys_world_twilight_harbor_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_32_gnastys_world_twilight_harbor_RODATA_START = .;
        build/overlays/level_32_gnastys_world_twilight_harbor/asm/overlays/level_32_gnastys_world_twilight_harbor/data/rodata.rodata.o(.rodata);
        build/overlays/level_32_gnastys_world_twilight_harbor/asm/overlays/level_32_gnastys_world_twilight_harbor/text.o(.rodata);
        level_32_gnastys_world_twilight_harbor_RODATA_END = .;
        level_32_gnastys_world_twilight_harbor_RODATA_SIZE = ABSOLUTE(level_32_gnastys_world_twilight_harbor_RODATA_END - level_32_gnastys_world_twilight_harbor_RODATA_START);
        level_32_gnastys_world_twilight_harbor_TEXT_START = .;
        build/overlays/level_32_gnastys_world_twilight_harbor/asm/overlays/level_32_gnastys_world_twilight_harbor/text.o(.text);
        level_32_gnastys_world_twilight_harbor_TEXT_END = .;
        level_32_gnastys_world_twilight_harbor_TEXT_SIZE = ABSOLUTE(level_32_gnastys_world_twilight_harbor_TEXT_END - level_32_gnastys_world_twilight_harbor_TEXT_START);
        level_32_gnastys_world_twilight_harbor_DATA_START = .;
        build/overlays/level_32_gnastys_world_twilight_harbor/asm/overlays/level_32_gnastys_world_twilight_harbor/data/rodata.rodata.o(.data);
        build/overlays/level_32_gnastys_world_twilight_harbor/asm/overlays/level_32_gnastys_world_twilight_harbor/text.o(.data);
        build/overlays/level_32_gnastys_world_twilight_harbor/asm/overlays/level_32_gnastys_world_twilight_harbor/data/data.data.o(.data);
        level_32_gnastys_world_twilight_harbor_DATA_END = .;
        level_32_gnastys_world_twilight_harbor_DATA_SIZE = ABSOLUTE(level_32_gnastys_world_twilight_harbor_DATA_END - level_32_gnastys_world_twilight_harbor_DATA_START);
        level_32_gnastys_world_twilight_harbor_BSS_START = .;
        build/overlays/level_32_gnastys_world_twilight_harbor/asm/overlays/level_32_gnastys_world_twilight_harbor/data/rodata.rodata.o(.bss);
        build/overlays/level_32_gnastys_world_twilight_harbor/asm/overlays/level_32_gnastys_world_twilight_harbor/text.o(.bss);
        level_32_gnastys_world_twilight_harbor_BSS_END = .;
        level_32_gnastys_world_twilight_harbor_BSS_SIZE = ABSOLUTE(level_32_gnastys_world_twilight_harbor_BSS_END - level_32_gnastys_world_twilight_harbor_BSS_START);
    }
    __romPos += SIZEOF(.level_32_gnastys_world_twilight_harbor);
    __romPos = ALIGN(__romPos, 16);
    level_32_gnastys_world_twilight_harbor_ROM_END = __romPos;
    level_32_gnastys_world_twilight_harbor_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
