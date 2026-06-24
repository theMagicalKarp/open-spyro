SECTIONS
{
    __romPos = 0;
    level_30_gnastys_world_gnorc_gnexus_ROM_START = __romPos;
    level_30_gnastys_world_gnorc_gnexus_VRAM = ADDR(.level_30_gnastys_world_gnorc_gnexus);
    .level_30_gnastys_world_gnorc_gnexus 0x8007AA38 : AT(level_30_gnastys_world_gnorc_gnexus_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_30_gnastys_world_gnorc_gnexus_RODATA_START = .;
        build/overlays/level_30_gnastys_world_gnorc_gnexus/asm/overlays/level_30_gnastys_world_gnorc_gnexus/data/rodata.rodata.o(.rodata);
        build/overlays/level_30_gnastys_world_gnorc_gnexus/asm/overlays/level_30_gnastys_world_gnorc_gnexus/text.o(.rodata);
        level_30_gnastys_world_gnorc_gnexus_RODATA_END = .;
        level_30_gnastys_world_gnorc_gnexus_RODATA_SIZE = ABSOLUTE(level_30_gnastys_world_gnorc_gnexus_RODATA_END - level_30_gnastys_world_gnorc_gnexus_RODATA_START);
        level_30_gnastys_world_gnorc_gnexus_TEXT_START = .;
        build/overlays/level_30_gnastys_world_gnorc_gnexus/asm/overlays/level_30_gnastys_world_gnorc_gnexus/text.o(.text);
        level_30_gnastys_world_gnorc_gnexus_TEXT_END = .;
        level_30_gnastys_world_gnorc_gnexus_TEXT_SIZE = ABSOLUTE(level_30_gnastys_world_gnorc_gnexus_TEXT_END - level_30_gnastys_world_gnorc_gnexus_TEXT_START);
        level_30_gnastys_world_gnorc_gnexus_DATA_START = .;
        build/overlays/level_30_gnastys_world_gnorc_gnexus/asm/overlays/level_30_gnastys_world_gnorc_gnexus/data/rodata.rodata.o(.data);
        build/overlays/level_30_gnastys_world_gnorc_gnexus/asm/overlays/level_30_gnastys_world_gnorc_gnexus/text.o(.data);
        build/overlays/level_30_gnastys_world_gnorc_gnexus/asm/overlays/level_30_gnastys_world_gnorc_gnexus/data/data.data.o(.data);
        level_30_gnastys_world_gnorc_gnexus_DATA_END = .;
        level_30_gnastys_world_gnorc_gnexus_DATA_SIZE = ABSOLUTE(level_30_gnastys_world_gnorc_gnexus_DATA_END - level_30_gnastys_world_gnorc_gnexus_DATA_START);
        level_30_gnastys_world_gnorc_gnexus_BSS_START = .;
        build/overlays/level_30_gnastys_world_gnorc_gnexus/asm/overlays/level_30_gnastys_world_gnorc_gnexus/data/rodata.rodata.o(.bss);
        build/overlays/level_30_gnastys_world_gnorc_gnexus/asm/overlays/level_30_gnastys_world_gnorc_gnexus/text.o(.bss);
        level_30_gnastys_world_gnorc_gnexus_BSS_END = .;
        level_30_gnastys_world_gnorc_gnexus_BSS_SIZE = ABSOLUTE(level_30_gnastys_world_gnorc_gnexus_BSS_END - level_30_gnastys_world_gnorc_gnexus_BSS_START);
    }
    __romPos += SIZEOF(.level_30_gnastys_world_gnorc_gnexus);
    __romPos = ALIGN(__romPos, 16);
    level_30_gnastys_world_gnorc_gnexus_ROM_END = __romPos;
    level_30_gnastys_world_gnorc_gnexus_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
