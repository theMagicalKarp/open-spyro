SECTIONS
{
    __romPos = 0;
    level_22_beast_makers_metalhead_ROM_START = __romPos;
    level_22_beast_makers_metalhead_VRAM = ADDR(.level_22_beast_makers_metalhead);
    .level_22_beast_makers_metalhead 0x8007AA38 : AT(level_22_beast_makers_metalhead_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_22_beast_makers_metalhead_RODATA_START = .;
        build/overlays/level_22_beast_makers_metalhead/asm/overlays/level_22_beast_makers_metalhead/data/rodata.rodata.o(.rodata);
        build/overlays/level_22_beast_makers_metalhead/asm/overlays/level_22_beast_makers_metalhead/text.o(.rodata);
        level_22_beast_makers_metalhead_RODATA_END = .;
        level_22_beast_makers_metalhead_RODATA_SIZE = ABSOLUTE(level_22_beast_makers_metalhead_RODATA_END - level_22_beast_makers_metalhead_RODATA_START);
        level_22_beast_makers_metalhead_TEXT_START = .;
        build/overlays/level_22_beast_makers_metalhead/asm/overlays/level_22_beast_makers_metalhead/text.o(.text);
        level_22_beast_makers_metalhead_TEXT_END = .;
        level_22_beast_makers_metalhead_TEXT_SIZE = ABSOLUTE(level_22_beast_makers_metalhead_TEXT_END - level_22_beast_makers_metalhead_TEXT_START);
        level_22_beast_makers_metalhead_DATA_START = .;
        build/overlays/level_22_beast_makers_metalhead/asm/overlays/level_22_beast_makers_metalhead/data/rodata.rodata.o(.data);
        build/overlays/level_22_beast_makers_metalhead/asm/overlays/level_22_beast_makers_metalhead/text.o(.data);
        build/overlays/level_22_beast_makers_metalhead/asm/overlays/level_22_beast_makers_metalhead/data/data.data.o(.data);
        level_22_beast_makers_metalhead_DATA_END = .;
        level_22_beast_makers_metalhead_DATA_SIZE = ABSOLUTE(level_22_beast_makers_metalhead_DATA_END - level_22_beast_makers_metalhead_DATA_START);
        level_22_beast_makers_metalhead_BSS_START = .;
        build/overlays/level_22_beast_makers_metalhead/asm/overlays/level_22_beast_makers_metalhead/data/rodata.rodata.o(.bss);
        build/overlays/level_22_beast_makers_metalhead/asm/overlays/level_22_beast_makers_metalhead/text.o(.bss);
        level_22_beast_makers_metalhead_BSS_END = .;
        level_22_beast_makers_metalhead_BSS_SIZE = ABSOLUTE(level_22_beast_makers_metalhead_BSS_END - level_22_beast_makers_metalhead_BSS_START);
    }
    __romPos += SIZEOF(.level_22_beast_makers_metalhead);
    __romPos = ALIGN(__romPos, 16);
    level_22_beast_makers_metalhead_ROM_END = __romPos;
    level_22_beast_makers_metalhead_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
