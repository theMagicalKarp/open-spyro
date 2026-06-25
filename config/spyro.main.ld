SECTIONS
{
    __romPos = 0;
    _gp = 0x80075264;
    header_ROM_START = __romPos;
    header_VRAM = ADDR(.header);
    .header : AT(header_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        header_DATA_START = .;
        build/main/asm/header.o(.data);
        header_DATA_END = .;
        header_DATA_SIZE = ABSOLUTE(header_DATA_END - header_DATA_START);
    }
    __romPos += SIZEOF(.header);
    header_ROM_END = __romPos;
    header_VRAM_END = .;

    main_ROM_START = __romPos;
    main_VRAM = ADDR(.main);
    .main 0x80010000 : AT(main_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        main_RODATA_START = .;
        build/main/asm/data/rodata_pre.rodata.o(.rodata);
        build/main/asm/text.o(.rodata);
        main_RODATA_END = .;
        main_RODATA_SIZE = ABSOLUTE(main_RODATA_END - main_RODATA_START);
        main_TEXT_START = .;
        /* Per-function fixed-VMA slots (`open-spyro gen-slots-ld`). Replaces the old
           monolithic text.o(.text) so individual functions can be flipped to C. */
        INCLUDE config/spyro.main.slots.ld
        build/main/asm/text.o(.text);
        main_TEXT_END = .;
        main_TEXT_SIZE = ABSOLUTE(main_TEXT_END - main_TEXT_START);
        main_DATA_START = .;
        build/main/asm/data/rodata_pre.rodata.o(.data);
        build/main/asm/text.o(.data);
        build/main/asm/data/data.data.o(.data);
        main_DATA_END = .;
        main_DATA_SIZE = ABSOLUTE(main_DATA_END - main_DATA_START);
        main_BSS_START = .;
        build/main/asm/data/rodata_pre.rodata.o(.bss);
        build/main/asm/text.o(.bss);
        main_BSS_END = .;
        main_BSS_SIZE = ABSOLUTE(main_BSS_END - main_BSS_START);
    }
    __romPos += SIZEOF(.main);
    __romPos = ALIGN(__romPos, 16);
    main_ROM_END = __romPos;
    main_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
