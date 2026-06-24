SECTIONS
{
    __romPos = 0;
    level_10_peace_keepers_doctor_shemp_ROM_START = __romPos;
    level_10_peace_keepers_doctor_shemp_VRAM = ADDR(.level_10_peace_keepers_doctor_shemp);
    .level_10_peace_keepers_doctor_shemp 0x8007AA38 : AT(level_10_peace_keepers_doctor_shemp_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_10_peace_keepers_doctor_shemp_RODATA_START = .;
        build/overlays/level_10_peace_keepers_doctor_shemp/asm/overlays/level_10_peace_keepers_doctor_shemp/data/rodata.rodata.o(.rodata);
        build/overlays/level_10_peace_keepers_doctor_shemp/asm/overlays/level_10_peace_keepers_doctor_shemp/text.o(.rodata);
        level_10_peace_keepers_doctor_shemp_RODATA_END = .;
        level_10_peace_keepers_doctor_shemp_RODATA_SIZE = ABSOLUTE(level_10_peace_keepers_doctor_shemp_RODATA_END - level_10_peace_keepers_doctor_shemp_RODATA_START);
        level_10_peace_keepers_doctor_shemp_TEXT_START = .;
        build/overlays/level_10_peace_keepers_doctor_shemp/asm/overlays/level_10_peace_keepers_doctor_shemp/text.o(.text);
        level_10_peace_keepers_doctor_shemp_TEXT_END = .;
        level_10_peace_keepers_doctor_shemp_TEXT_SIZE = ABSOLUTE(level_10_peace_keepers_doctor_shemp_TEXT_END - level_10_peace_keepers_doctor_shemp_TEXT_START);
        level_10_peace_keepers_doctor_shemp_DATA_START = .;
        build/overlays/level_10_peace_keepers_doctor_shemp/asm/overlays/level_10_peace_keepers_doctor_shemp/data/rodata.rodata.o(.data);
        build/overlays/level_10_peace_keepers_doctor_shemp/asm/overlays/level_10_peace_keepers_doctor_shemp/text.o(.data);
        build/overlays/level_10_peace_keepers_doctor_shemp/asm/overlays/level_10_peace_keepers_doctor_shemp/data/data.data.o(.data);
        level_10_peace_keepers_doctor_shemp_DATA_END = .;
        level_10_peace_keepers_doctor_shemp_DATA_SIZE = ABSOLUTE(level_10_peace_keepers_doctor_shemp_DATA_END - level_10_peace_keepers_doctor_shemp_DATA_START);
        level_10_peace_keepers_doctor_shemp_BSS_START = .;
        build/overlays/level_10_peace_keepers_doctor_shemp/asm/overlays/level_10_peace_keepers_doctor_shemp/data/rodata.rodata.o(.bss);
        build/overlays/level_10_peace_keepers_doctor_shemp/asm/overlays/level_10_peace_keepers_doctor_shemp/text.o(.bss);
        level_10_peace_keepers_doctor_shemp_BSS_END = .;
        level_10_peace_keepers_doctor_shemp_BSS_SIZE = ABSOLUTE(level_10_peace_keepers_doctor_shemp_BSS_END - level_10_peace_keepers_doctor_shemp_BSS_START);
    }
    __romPos += SIZEOF(.level_10_peace_keepers_doctor_shemp);
    __romPos = ALIGN(__romPos, 16);
    level_10_peace_keepers_doctor_shemp_ROM_END = __romPos;
    level_10_peace_keepers_doctor_shemp_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
