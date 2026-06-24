SECTIONS
{
    __romPos = 0;
    level_25_dream_weavers_dark_passage_ROM_START = __romPos;
    level_25_dream_weavers_dark_passage_VRAM = ADDR(.level_25_dream_weavers_dark_passage);
    .level_25_dream_weavers_dark_passage 0x8007AA38 : AT(level_25_dream_weavers_dark_passage_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_25_dream_weavers_dark_passage_RODATA_START = .;
        build/overlays/level_25_dream_weavers_dark_passage/asm/overlays/level_25_dream_weavers_dark_passage/data/rodata.rodata.o(.rodata);
        build/overlays/level_25_dream_weavers_dark_passage/asm/overlays/level_25_dream_weavers_dark_passage/text.o(.rodata);
        level_25_dream_weavers_dark_passage_RODATA_END = .;
        level_25_dream_weavers_dark_passage_RODATA_SIZE = ABSOLUTE(level_25_dream_weavers_dark_passage_RODATA_END - level_25_dream_weavers_dark_passage_RODATA_START);
        level_25_dream_weavers_dark_passage_TEXT_START = .;
        build/overlays/level_25_dream_weavers_dark_passage/asm/overlays/level_25_dream_weavers_dark_passage/text.o(.text);
        level_25_dream_weavers_dark_passage_TEXT_END = .;
        level_25_dream_weavers_dark_passage_TEXT_SIZE = ABSOLUTE(level_25_dream_weavers_dark_passage_TEXT_END - level_25_dream_weavers_dark_passage_TEXT_START);
        level_25_dream_weavers_dark_passage_DATA_START = .;
        build/overlays/level_25_dream_weavers_dark_passage/asm/overlays/level_25_dream_weavers_dark_passage/data/rodata.rodata.o(.data);
        build/overlays/level_25_dream_weavers_dark_passage/asm/overlays/level_25_dream_weavers_dark_passage/text.o(.data);
        build/overlays/level_25_dream_weavers_dark_passage/asm/overlays/level_25_dream_weavers_dark_passage/data/data.data.o(.data);
        level_25_dream_weavers_dark_passage_DATA_END = .;
        level_25_dream_weavers_dark_passage_DATA_SIZE = ABSOLUTE(level_25_dream_weavers_dark_passage_DATA_END - level_25_dream_weavers_dark_passage_DATA_START);
        level_25_dream_weavers_dark_passage_BSS_START = .;
        build/overlays/level_25_dream_weavers_dark_passage/asm/overlays/level_25_dream_weavers_dark_passage/data/rodata.rodata.o(.bss);
        build/overlays/level_25_dream_weavers_dark_passage/asm/overlays/level_25_dream_weavers_dark_passage/text.o(.bss);
        level_25_dream_weavers_dark_passage_BSS_END = .;
        level_25_dream_weavers_dark_passage_BSS_SIZE = ABSOLUTE(level_25_dream_weavers_dark_passage_BSS_END - level_25_dream_weavers_dark_passage_BSS_START);
    }
    __romPos += SIZEOF(.level_25_dream_weavers_dark_passage);
    __romPos = ALIGN(__romPos, 16);
    level_25_dream_weavers_dark_passage_ROM_END = __romPos;
    level_25_dream_weavers_dark_passage_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
