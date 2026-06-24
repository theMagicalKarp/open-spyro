SECTIONS
{
    __romPos = 0;
    level_26_dream_weavers_lofty_castle_ROM_START = __romPos;
    level_26_dream_weavers_lofty_castle_VRAM = ADDR(.level_26_dream_weavers_lofty_castle);
    .level_26_dream_weavers_lofty_castle 0x8007AA38 : AT(level_26_dream_weavers_lofty_castle_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_26_dream_weavers_lofty_castle_RODATA_START = .;
        build/overlays/level_26_dream_weavers_lofty_castle/asm/overlays/level_26_dream_weavers_lofty_castle/data/rodata.rodata.o(.rodata);
        build/overlays/level_26_dream_weavers_lofty_castle/asm/overlays/level_26_dream_weavers_lofty_castle/text.o(.rodata);
        level_26_dream_weavers_lofty_castle_RODATA_END = .;
        level_26_dream_weavers_lofty_castle_RODATA_SIZE = ABSOLUTE(level_26_dream_weavers_lofty_castle_RODATA_END - level_26_dream_weavers_lofty_castle_RODATA_START);
        level_26_dream_weavers_lofty_castle_TEXT_START = .;
        build/overlays/level_26_dream_weavers_lofty_castle/asm/overlays/level_26_dream_weavers_lofty_castle/text.o(.text);
        level_26_dream_weavers_lofty_castle_TEXT_END = .;
        level_26_dream_weavers_lofty_castle_TEXT_SIZE = ABSOLUTE(level_26_dream_weavers_lofty_castle_TEXT_END - level_26_dream_weavers_lofty_castle_TEXT_START);
        level_26_dream_weavers_lofty_castle_DATA_START = .;
        build/overlays/level_26_dream_weavers_lofty_castle/asm/overlays/level_26_dream_weavers_lofty_castle/data/rodata.rodata.o(.data);
        build/overlays/level_26_dream_weavers_lofty_castle/asm/overlays/level_26_dream_weavers_lofty_castle/text.o(.data);
        build/overlays/level_26_dream_weavers_lofty_castle/asm/overlays/level_26_dream_weavers_lofty_castle/data/data.data.o(.data);
        level_26_dream_weavers_lofty_castle_DATA_END = .;
        level_26_dream_weavers_lofty_castle_DATA_SIZE = ABSOLUTE(level_26_dream_weavers_lofty_castle_DATA_END - level_26_dream_weavers_lofty_castle_DATA_START);
        level_26_dream_weavers_lofty_castle_BSS_START = .;
        build/overlays/level_26_dream_weavers_lofty_castle/asm/overlays/level_26_dream_weavers_lofty_castle/data/rodata.rodata.o(.bss);
        build/overlays/level_26_dream_weavers_lofty_castle/asm/overlays/level_26_dream_weavers_lofty_castle/text.o(.bss);
        level_26_dream_weavers_lofty_castle_BSS_END = .;
        level_26_dream_weavers_lofty_castle_BSS_SIZE = ABSOLUTE(level_26_dream_weavers_lofty_castle_BSS_END - level_26_dream_weavers_lofty_castle_BSS_START);
    }
    __romPos += SIZEOF(.level_26_dream_weavers_lofty_castle);
    __romPos = ALIGN(__romPos, 16);
    level_26_dream_weavers_lofty_castle_ROM_END = __romPos;
    level_26_dream_weavers_lofty_castle_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
