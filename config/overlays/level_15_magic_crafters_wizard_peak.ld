SECTIONS
{
    __romPos = 0;
    level_15_magic_crafters_wizard_peak_ROM_START = __romPos;
    level_15_magic_crafters_wizard_peak_VRAM = ADDR(.level_15_magic_crafters_wizard_peak);
    .level_15_magic_crafters_wizard_peak 0x8007AA38 : AT(level_15_magic_crafters_wizard_peak_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_15_magic_crafters_wizard_peak_RODATA_START = .;
        build/overlays/level_15_magic_crafters_wizard_peak/asm/overlays/level_15_magic_crafters_wizard_peak/data/rodata.rodata.o(.rodata);
        build/overlays/level_15_magic_crafters_wizard_peak/asm/overlays/level_15_magic_crafters_wizard_peak/text.o(.rodata);
        level_15_magic_crafters_wizard_peak_RODATA_END = .;
        level_15_magic_crafters_wizard_peak_RODATA_SIZE = ABSOLUTE(level_15_magic_crafters_wizard_peak_RODATA_END - level_15_magic_crafters_wizard_peak_RODATA_START);
        level_15_magic_crafters_wizard_peak_TEXT_START = .;
        build/overlays/level_15_magic_crafters_wizard_peak/asm/overlays/level_15_magic_crafters_wizard_peak/text.o(.text);
        level_15_magic_crafters_wizard_peak_TEXT_END = .;
        level_15_magic_crafters_wizard_peak_TEXT_SIZE = ABSOLUTE(level_15_magic_crafters_wizard_peak_TEXT_END - level_15_magic_crafters_wizard_peak_TEXT_START);
        level_15_magic_crafters_wizard_peak_DATA_START = .;
        build/overlays/level_15_magic_crafters_wizard_peak/asm/overlays/level_15_magic_crafters_wizard_peak/data/rodata.rodata.o(.data);
        build/overlays/level_15_magic_crafters_wizard_peak/asm/overlays/level_15_magic_crafters_wizard_peak/text.o(.data);
        build/overlays/level_15_magic_crafters_wizard_peak/asm/overlays/level_15_magic_crafters_wizard_peak/data/data.data.o(.data);
        level_15_magic_crafters_wizard_peak_DATA_END = .;
        level_15_magic_crafters_wizard_peak_DATA_SIZE = ABSOLUTE(level_15_magic_crafters_wizard_peak_DATA_END - level_15_magic_crafters_wizard_peak_DATA_START);
        level_15_magic_crafters_wizard_peak_BSS_START = .;
        build/overlays/level_15_magic_crafters_wizard_peak/asm/overlays/level_15_magic_crafters_wizard_peak/data/rodata.rodata.o(.bss);
        build/overlays/level_15_magic_crafters_wizard_peak/asm/overlays/level_15_magic_crafters_wizard_peak/text.o(.bss);
        level_15_magic_crafters_wizard_peak_BSS_END = .;
        level_15_magic_crafters_wizard_peak_BSS_SIZE = ABSOLUTE(level_15_magic_crafters_wizard_peak_BSS_END - level_15_magic_crafters_wizard_peak_BSS_START);
    }
    __romPos += SIZEOF(.level_15_magic_crafters_wizard_peak);
    __romPos = ALIGN(__romPos, 16);
    level_15_magic_crafters_wizard_peak_ROM_END = __romPos;
    level_15_magic_crafters_wizard_peak_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
