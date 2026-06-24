SECTIONS
{
    __romPos = 0;
    level_4_artisans_toasty_ROM_START = __romPos;
    level_4_artisans_toasty_VRAM = ADDR(.level_4_artisans_toasty);
    .level_4_artisans_toasty 0x8007AA38 : AT(level_4_artisans_toasty_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_4_artisans_toasty_RODATA_START = .;
        build/overlays/level_4_artisans_toasty/asm/overlays/level_4_artisans_toasty/data/rodata.rodata.o(.rodata);
        build/overlays/level_4_artisans_toasty/asm/overlays/level_4_artisans_toasty/text.o(.rodata);
        level_4_artisans_toasty_RODATA_END = .;
        level_4_artisans_toasty_RODATA_SIZE = ABSOLUTE(level_4_artisans_toasty_RODATA_END - level_4_artisans_toasty_RODATA_START);
        level_4_artisans_toasty_TEXT_START = .;
        build/overlays/level_4_artisans_toasty/asm/overlays/level_4_artisans_toasty/text.o(.text);
        level_4_artisans_toasty_TEXT_END = .;
        level_4_artisans_toasty_TEXT_SIZE = ABSOLUTE(level_4_artisans_toasty_TEXT_END - level_4_artisans_toasty_TEXT_START);
        level_4_artisans_toasty_DATA_START = .;
        build/overlays/level_4_artisans_toasty/asm/overlays/level_4_artisans_toasty/data/rodata.rodata.o(.data);
        build/overlays/level_4_artisans_toasty/asm/overlays/level_4_artisans_toasty/text.o(.data);
        build/overlays/level_4_artisans_toasty/asm/overlays/level_4_artisans_toasty/data/data.data.o(.data);
        level_4_artisans_toasty_DATA_END = .;
        level_4_artisans_toasty_DATA_SIZE = ABSOLUTE(level_4_artisans_toasty_DATA_END - level_4_artisans_toasty_DATA_START);
        level_4_artisans_toasty_BSS_START = .;
        build/overlays/level_4_artisans_toasty/asm/overlays/level_4_artisans_toasty/data/rodata.rodata.o(.bss);
        build/overlays/level_4_artisans_toasty/asm/overlays/level_4_artisans_toasty/text.o(.bss);
        level_4_artisans_toasty_BSS_END = .;
        level_4_artisans_toasty_BSS_SIZE = ABSOLUTE(level_4_artisans_toasty_BSS_END - level_4_artisans_toasty_BSS_START);
    }
    __romPos += SIZEOF(.level_4_artisans_toasty);
    __romPos = ALIGN(__romPos, 16);
    level_4_artisans_toasty_ROM_END = __romPos;
    level_4_artisans_toasty_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
