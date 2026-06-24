SECTIONS
{
    __romPos = 0;
    titlescreen_ROM_START = __romPos;
    titlescreen_VRAM = ADDR(.titlescreen);
    .titlescreen 0x8007AA38 : AT(titlescreen_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        titlescreen_RODATA_START = .;
        build/overlays/titlescreen/asm/overlays/titlescreen/data/rodata.rodata.o(.rodata);
        build/overlays/titlescreen/asm/overlays/titlescreen/text.o(.rodata);
        titlescreen_RODATA_END = .;
        titlescreen_RODATA_SIZE = ABSOLUTE(titlescreen_RODATA_END - titlescreen_RODATA_START);
        titlescreen_TEXT_START = .;
        build/overlays/titlescreen/asm/overlays/titlescreen/text.o(.text);
        titlescreen_TEXT_END = .;
        titlescreen_TEXT_SIZE = ABSOLUTE(titlescreen_TEXT_END - titlescreen_TEXT_START);
        titlescreen_DATA_START = .;
        build/overlays/titlescreen/asm/overlays/titlescreen/data/rodata.rodata.o(.data);
        build/overlays/titlescreen/asm/overlays/titlescreen/text.o(.data);
        build/overlays/titlescreen/asm/overlays/titlescreen/data/data.data.o(.data);
        titlescreen_DATA_END = .;
        titlescreen_DATA_SIZE = ABSOLUTE(titlescreen_DATA_END - titlescreen_DATA_START);
        titlescreen_BSS_START = .;
        build/overlays/titlescreen/asm/overlays/titlescreen/data/rodata.rodata.o(.bss);
        build/overlays/titlescreen/asm/overlays/titlescreen/text.o(.bss);
        titlescreen_BSS_END = .;
        titlescreen_BSS_SIZE = ABSOLUTE(titlescreen_BSS_END - titlescreen_BSS_START);
    }
    __romPos += SIZEOF(.titlescreen);
    __romPos = ALIGN(__romPos, 16);
    titlescreen_ROM_END = __romPos;
    titlescreen_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
