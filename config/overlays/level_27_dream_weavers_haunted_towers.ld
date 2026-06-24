SECTIONS
{
    __romPos = 0;
    level_27_dream_weavers_haunted_towers_ROM_START = __romPos;
    level_27_dream_weavers_haunted_towers_VRAM = ADDR(.level_27_dream_weavers_haunted_towers);
    .level_27_dream_weavers_haunted_towers 0x8007AA38 : AT(level_27_dream_weavers_haunted_towers_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_27_dream_weavers_haunted_towers_RODATA_START = .;
        build/overlays/level_27_dream_weavers_haunted_towers/asm/overlays/level_27_dream_weavers_haunted_towers/data/rodata.rodata.o(.rodata);
        build/overlays/level_27_dream_weavers_haunted_towers/asm/overlays/level_27_dream_weavers_haunted_towers/text.o(.rodata);
        level_27_dream_weavers_haunted_towers_RODATA_END = .;
        level_27_dream_weavers_haunted_towers_RODATA_SIZE = ABSOLUTE(level_27_dream_weavers_haunted_towers_RODATA_END - level_27_dream_weavers_haunted_towers_RODATA_START);
        level_27_dream_weavers_haunted_towers_TEXT_START = .;
        build/overlays/level_27_dream_weavers_haunted_towers/asm/overlays/level_27_dream_weavers_haunted_towers/text.o(.text);
        level_27_dream_weavers_haunted_towers_TEXT_END = .;
        level_27_dream_weavers_haunted_towers_TEXT_SIZE = ABSOLUTE(level_27_dream_weavers_haunted_towers_TEXT_END - level_27_dream_weavers_haunted_towers_TEXT_START);
        level_27_dream_weavers_haunted_towers_DATA_START = .;
        build/overlays/level_27_dream_weavers_haunted_towers/asm/overlays/level_27_dream_weavers_haunted_towers/data/rodata.rodata.o(.data);
        build/overlays/level_27_dream_weavers_haunted_towers/asm/overlays/level_27_dream_weavers_haunted_towers/text.o(.data);
        build/overlays/level_27_dream_weavers_haunted_towers/asm/overlays/level_27_dream_weavers_haunted_towers/data/data.data.o(.data);
        level_27_dream_weavers_haunted_towers_DATA_END = .;
        level_27_dream_weavers_haunted_towers_DATA_SIZE = ABSOLUTE(level_27_dream_weavers_haunted_towers_DATA_END - level_27_dream_weavers_haunted_towers_DATA_START);
        level_27_dream_weavers_haunted_towers_BSS_START = .;
        build/overlays/level_27_dream_weavers_haunted_towers/asm/overlays/level_27_dream_weavers_haunted_towers/data/rodata.rodata.o(.bss);
        build/overlays/level_27_dream_weavers_haunted_towers/asm/overlays/level_27_dream_weavers_haunted_towers/text.o(.bss);
        level_27_dream_weavers_haunted_towers_BSS_END = .;
        level_27_dream_weavers_haunted_towers_BSS_SIZE = ABSOLUTE(level_27_dream_weavers_haunted_towers_BSS_END - level_27_dream_weavers_haunted_towers_BSS_START);
    }
    __romPos += SIZEOF(.level_27_dream_weavers_haunted_towers);
    __romPos = ALIGN(__romPos, 16);
    level_27_dream_weavers_haunted_towers_ROM_END = __romPos;
    level_27_dream_weavers_haunted_towers_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
