SECTIONS
{
    __romPos = 0;
    level_28_dream_weavers_jacques_ROM_START = __romPos;
    level_28_dream_weavers_jacques_VRAM = ADDR(.level_28_dream_weavers_jacques);
    .level_28_dream_weavers_jacques 0x8007AA38 : AT(level_28_dream_weavers_jacques_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_28_dream_weavers_jacques_RODATA_START = .;
        build/overlays/level_28_dream_weavers_jacques/asm/overlays/level_28_dream_weavers_jacques/data/rodata.rodata.o(.rodata);
        build/overlays/level_28_dream_weavers_jacques/asm/overlays/level_28_dream_weavers_jacques/text.o(.rodata);
        level_28_dream_weavers_jacques_RODATA_END = .;
        level_28_dream_weavers_jacques_RODATA_SIZE = ABSOLUTE(level_28_dream_weavers_jacques_RODATA_END - level_28_dream_weavers_jacques_RODATA_START);
        level_28_dream_weavers_jacques_TEXT_START = .;
        build/overlays/level_28_dream_weavers_jacques/asm/overlays/level_28_dream_weavers_jacques/text.o(.text);
        level_28_dream_weavers_jacques_TEXT_END = .;
        level_28_dream_weavers_jacques_TEXT_SIZE = ABSOLUTE(level_28_dream_weavers_jacques_TEXT_END - level_28_dream_weavers_jacques_TEXT_START);
        level_28_dream_weavers_jacques_DATA_START = .;
        build/overlays/level_28_dream_weavers_jacques/asm/overlays/level_28_dream_weavers_jacques/data/rodata.rodata.o(.data);
        build/overlays/level_28_dream_weavers_jacques/asm/overlays/level_28_dream_weavers_jacques/text.o(.data);
        build/overlays/level_28_dream_weavers_jacques/asm/overlays/level_28_dream_weavers_jacques/data/data.data.o(.data);
        level_28_dream_weavers_jacques_DATA_END = .;
        level_28_dream_weavers_jacques_DATA_SIZE = ABSOLUTE(level_28_dream_weavers_jacques_DATA_END - level_28_dream_weavers_jacques_DATA_START);
        level_28_dream_weavers_jacques_BSS_START = .;
        build/overlays/level_28_dream_weavers_jacques/asm/overlays/level_28_dream_weavers_jacques/data/rodata.rodata.o(.bss);
        build/overlays/level_28_dream_weavers_jacques/asm/overlays/level_28_dream_weavers_jacques/text.o(.bss);
        level_28_dream_weavers_jacques_BSS_END = .;
        level_28_dream_weavers_jacques_BSS_SIZE = ABSOLUTE(level_28_dream_weavers_jacques_BSS_END - level_28_dream_weavers_jacques_BSS_START);
    }
    __romPos += SIZEOF(.level_28_dream_weavers_jacques);
    __romPos = ALIGN(__romPos, 16);
    level_28_dream_weavers_jacques_ROM_END = __romPos;
    level_28_dream_weavers_jacques_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
