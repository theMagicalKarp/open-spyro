SECTIONS
{
    __romPos = 0;
    level_33_gnastys_world_gnasty_gnorc_ROM_START = __romPos;
    level_33_gnastys_world_gnasty_gnorc_VRAM = ADDR(.level_33_gnastys_world_gnasty_gnorc);
    .level_33_gnastys_world_gnasty_gnorc 0x8007AA38 : AT(level_33_gnastys_world_gnasty_gnorc_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_33_gnastys_world_gnasty_gnorc_RODATA_START = .;
        build/overlays/level_33_gnastys_world_gnasty_gnorc/asm/overlays/level_33_gnastys_world_gnasty_gnorc/data/rodata.rodata.o(.rodata);
        build/overlays/level_33_gnastys_world_gnasty_gnorc/asm/overlays/level_33_gnastys_world_gnasty_gnorc/text.o(.rodata);
        level_33_gnastys_world_gnasty_gnorc_RODATA_END = .;
        level_33_gnastys_world_gnasty_gnorc_RODATA_SIZE = ABSOLUTE(level_33_gnastys_world_gnasty_gnorc_RODATA_END - level_33_gnastys_world_gnasty_gnorc_RODATA_START);
        level_33_gnastys_world_gnasty_gnorc_TEXT_START = .;
        build/overlays/level_33_gnastys_world_gnasty_gnorc/asm/overlays/level_33_gnastys_world_gnasty_gnorc/text.o(.text);
        level_33_gnastys_world_gnasty_gnorc_TEXT_END = .;
        level_33_gnastys_world_gnasty_gnorc_TEXT_SIZE = ABSOLUTE(level_33_gnastys_world_gnasty_gnorc_TEXT_END - level_33_gnastys_world_gnasty_gnorc_TEXT_START);
        level_33_gnastys_world_gnasty_gnorc_DATA_START = .;
        build/overlays/level_33_gnastys_world_gnasty_gnorc/asm/overlays/level_33_gnastys_world_gnasty_gnorc/data/rodata.rodata.o(.data);
        build/overlays/level_33_gnastys_world_gnasty_gnorc/asm/overlays/level_33_gnastys_world_gnasty_gnorc/text.o(.data);
        build/overlays/level_33_gnastys_world_gnasty_gnorc/asm/overlays/level_33_gnastys_world_gnasty_gnorc/data/data.data.o(.data);
        level_33_gnastys_world_gnasty_gnorc_DATA_END = .;
        level_33_gnastys_world_gnasty_gnorc_DATA_SIZE = ABSOLUTE(level_33_gnastys_world_gnasty_gnorc_DATA_END - level_33_gnastys_world_gnasty_gnorc_DATA_START);
        level_33_gnastys_world_gnasty_gnorc_BSS_START = .;
        build/overlays/level_33_gnastys_world_gnasty_gnorc/asm/overlays/level_33_gnastys_world_gnasty_gnorc/data/rodata.rodata.o(.bss);
        build/overlays/level_33_gnastys_world_gnasty_gnorc/asm/overlays/level_33_gnastys_world_gnasty_gnorc/text.o(.bss);
        level_33_gnastys_world_gnasty_gnorc_BSS_END = .;
        level_33_gnastys_world_gnasty_gnorc_BSS_SIZE = ABSOLUTE(level_33_gnastys_world_gnasty_gnorc_BSS_END - level_33_gnastys_world_gnasty_gnorc_BSS_START);
    }
    __romPos += SIZEOF(.level_33_gnastys_world_gnasty_gnorc);
    __romPos = ALIGN(__romPos, 16);
    level_33_gnastys_world_gnasty_gnorc_ROM_END = __romPos;
    level_33_gnastys_world_gnasty_gnorc_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
