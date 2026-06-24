SECTIONS
{
    __romPos = 0;
    level_19_beast_makers_terrace_village_ROM_START = __romPos;
    level_19_beast_makers_terrace_village_VRAM = ADDR(.level_19_beast_makers_terrace_village);
    .level_19_beast_makers_terrace_village 0x8007AA38 : AT(level_19_beast_makers_terrace_village_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_19_beast_makers_terrace_village_RODATA_START = .;
        build/overlays/level_19_beast_makers_terrace_village/asm/overlays/level_19_beast_makers_terrace_village/data/rodata.rodata.o(.rodata);
        build/overlays/level_19_beast_makers_terrace_village/asm/overlays/level_19_beast_makers_terrace_village/text.o(.rodata);
        level_19_beast_makers_terrace_village_RODATA_END = .;
        level_19_beast_makers_terrace_village_RODATA_SIZE = ABSOLUTE(level_19_beast_makers_terrace_village_RODATA_END - level_19_beast_makers_terrace_village_RODATA_START);
        level_19_beast_makers_terrace_village_TEXT_START = .;
        build/overlays/level_19_beast_makers_terrace_village/asm/overlays/level_19_beast_makers_terrace_village/text.o(.text);
        level_19_beast_makers_terrace_village_TEXT_END = .;
        level_19_beast_makers_terrace_village_TEXT_SIZE = ABSOLUTE(level_19_beast_makers_terrace_village_TEXT_END - level_19_beast_makers_terrace_village_TEXT_START);
        level_19_beast_makers_terrace_village_DATA_START = .;
        build/overlays/level_19_beast_makers_terrace_village/asm/overlays/level_19_beast_makers_terrace_village/data/rodata.rodata.o(.data);
        build/overlays/level_19_beast_makers_terrace_village/asm/overlays/level_19_beast_makers_terrace_village/text.o(.data);
        build/overlays/level_19_beast_makers_terrace_village/asm/overlays/level_19_beast_makers_terrace_village/data/data.data.o(.data);
        level_19_beast_makers_terrace_village_DATA_END = .;
        level_19_beast_makers_terrace_village_DATA_SIZE = ABSOLUTE(level_19_beast_makers_terrace_village_DATA_END - level_19_beast_makers_terrace_village_DATA_START);
        level_19_beast_makers_terrace_village_BSS_START = .;
        build/overlays/level_19_beast_makers_terrace_village/asm/overlays/level_19_beast_makers_terrace_village/data/rodata.rodata.o(.bss);
        build/overlays/level_19_beast_makers_terrace_village/asm/overlays/level_19_beast_makers_terrace_village/text.o(.bss);
        level_19_beast_makers_terrace_village_BSS_END = .;
        level_19_beast_makers_terrace_village_BSS_SIZE = ABSOLUTE(level_19_beast_makers_terrace_village_BSS_END - level_19_beast_makers_terrace_village_BSS_START);
    }
    __romPos += SIZEOF(.level_19_beast_makers_terrace_village);
    __romPos = ALIGN(__romPos, 16);
    level_19_beast_makers_terrace_village_ROM_END = __romPos;
    level_19_beast_makers_terrace_village_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
