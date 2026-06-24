SECTIONS
{
    __romPos = 0;
    level_21_beast_makers_tree_tops_ROM_START = __romPos;
    level_21_beast_makers_tree_tops_VRAM = ADDR(.level_21_beast_makers_tree_tops);
    .level_21_beast_makers_tree_tops 0x8007AA38 : AT(level_21_beast_makers_tree_tops_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_21_beast_makers_tree_tops_RODATA_START = .;
        build/overlays/level_21_beast_makers_tree_tops/asm/overlays/level_21_beast_makers_tree_tops/data/rodata.rodata.o(.rodata);
        build/overlays/level_21_beast_makers_tree_tops/asm/overlays/level_21_beast_makers_tree_tops/text.o(.rodata);
        level_21_beast_makers_tree_tops_RODATA_END = .;
        level_21_beast_makers_tree_tops_RODATA_SIZE = ABSOLUTE(level_21_beast_makers_tree_tops_RODATA_END - level_21_beast_makers_tree_tops_RODATA_START);
        level_21_beast_makers_tree_tops_TEXT_START = .;
        build/overlays/level_21_beast_makers_tree_tops/asm/overlays/level_21_beast_makers_tree_tops/text.o(.text);
        level_21_beast_makers_tree_tops_TEXT_END = .;
        level_21_beast_makers_tree_tops_TEXT_SIZE = ABSOLUTE(level_21_beast_makers_tree_tops_TEXT_END - level_21_beast_makers_tree_tops_TEXT_START);
        level_21_beast_makers_tree_tops_DATA_START = .;
        build/overlays/level_21_beast_makers_tree_tops/asm/overlays/level_21_beast_makers_tree_tops/data/rodata.rodata.o(.data);
        build/overlays/level_21_beast_makers_tree_tops/asm/overlays/level_21_beast_makers_tree_tops/text.o(.data);
        build/overlays/level_21_beast_makers_tree_tops/asm/overlays/level_21_beast_makers_tree_tops/data/data.data.o(.data);
        level_21_beast_makers_tree_tops_DATA_END = .;
        level_21_beast_makers_tree_tops_DATA_SIZE = ABSOLUTE(level_21_beast_makers_tree_tops_DATA_END - level_21_beast_makers_tree_tops_DATA_START);
        level_21_beast_makers_tree_tops_BSS_START = .;
        build/overlays/level_21_beast_makers_tree_tops/asm/overlays/level_21_beast_makers_tree_tops/data/rodata.rodata.o(.bss);
        build/overlays/level_21_beast_makers_tree_tops/asm/overlays/level_21_beast_makers_tree_tops/text.o(.bss);
        level_21_beast_makers_tree_tops_BSS_END = .;
        level_21_beast_makers_tree_tops_BSS_SIZE = ABSOLUTE(level_21_beast_makers_tree_tops_BSS_END - level_21_beast_makers_tree_tops_BSS_START);
    }
    __romPos += SIZEOF(.level_21_beast_makers_tree_tops);
    __romPos = ALIGN(__romPos, 16);
    level_21_beast_makers_tree_tops_ROM_END = __romPos;
    level_21_beast_makers_tree_tops_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
