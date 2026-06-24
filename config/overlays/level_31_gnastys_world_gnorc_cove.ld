SECTIONS
{
    __romPos = 0;
    level_31_gnastys_world_gnorc_cove_ROM_START = __romPos;
    level_31_gnastys_world_gnorc_cove_VRAM = ADDR(.level_31_gnastys_world_gnorc_cove);
    .level_31_gnastys_world_gnorc_cove 0x8007AA38 : AT(level_31_gnastys_world_gnorc_cove_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_31_gnastys_world_gnorc_cove_RODATA_START = .;
        build/overlays/level_31_gnastys_world_gnorc_cove/asm/overlays/level_31_gnastys_world_gnorc_cove/data/rodata.rodata.o(.rodata);
        build/overlays/level_31_gnastys_world_gnorc_cove/asm/overlays/level_31_gnastys_world_gnorc_cove/text.o(.rodata);
        level_31_gnastys_world_gnorc_cove_RODATA_END = .;
        level_31_gnastys_world_gnorc_cove_RODATA_SIZE = ABSOLUTE(level_31_gnastys_world_gnorc_cove_RODATA_END - level_31_gnastys_world_gnorc_cove_RODATA_START);
        level_31_gnastys_world_gnorc_cove_TEXT_START = .;
        build/overlays/level_31_gnastys_world_gnorc_cove/asm/overlays/level_31_gnastys_world_gnorc_cove/text.o(.text);
        level_31_gnastys_world_gnorc_cove_TEXT_END = .;
        level_31_gnastys_world_gnorc_cove_TEXT_SIZE = ABSOLUTE(level_31_gnastys_world_gnorc_cove_TEXT_END - level_31_gnastys_world_gnorc_cove_TEXT_START);
        level_31_gnastys_world_gnorc_cove_DATA_START = .;
        build/overlays/level_31_gnastys_world_gnorc_cove/asm/overlays/level_31_gnastys_world_gnorc_cove/data/rodata.rodata.o(.data);
        build/overlays/level_31_gnastys_world_gnorc_cove/asm/overlays/level_31_gnastys_world_gnorc_cove/text.o(.data);
        build/overlays/level_31_gnastys_world_gnorc_cove/asm/overlays/level_31_gnastys_world_gnorc_cove/data/data.data.o(.data);
        level_31_gnastys_world_gnorc_cove_DATA_END = .;
        level_31_gnastys_world_gnorc_cove_DATA_SIZE = ABSOLUTE(level_31_gnastys_world_gnorc_cove_DATA_END - level_31_gnastys_world_gnorc_cove_DATA_START);
        level_31_gnastys_world_gnorc_cove_BSS_START = .;
        build/overlays/level_31_gnastys_world_gnorc_cove/asm/overlays/level_31_gnastys_world_gnorc_cove/data/rodata.rodata.o(.bss);
        build/overlays/level_31_gnastys_world_gnorc_cove/asm/overlays/level_31_gnastys_world_gnorc_cove/text.o(.bss);
        level_31_gnastys_world_gnorc_cove_BSS_END = .;
        level_31_gnastys_world_gnorc_cove_BSS_SIZE = ABSOLUTE(level_31_gnastys_world_gnorc_cove_BSS_END - level_31_gnastys_world_gnorc_cove_BSS_START);
    }
    __romPos += SIZEOF(.level_31_gnastys_world_gnorc_cove);
    __romPos = ALIGN(__romPos, 16);
    level_31_gnastys_world_gnorc_cove_ROM_END = __romPos;
    level_31_gnastys_world_gnorc_cove_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
