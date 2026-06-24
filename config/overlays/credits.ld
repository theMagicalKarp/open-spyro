SECTIONS
{
    __romPos = 0;
    credits_ROM_START = __romPos;
    credits_VRAM = ADDR(.credits);
    .credits 0x8007AA38 : AT(credits_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        credits_RODATA_START = .;
        build/overlays/credits/asm/overlays/credits/data/rodata.rodata.o(.rodata);
        build/overlays/credits/asm/overlays/credits/text.o(.rodata);
        credits_RODATA_END = .;
        credits_RODATA_SIZE = ABSOLUTE(credits_RODATA_END - credits_RODATA_START);
        credits_TEXT_START = .;
        build/overlays/credits/asm/overlays/credits/text.o(.text);
        credits_TEXT_END = .;
        credits_TEXT_SIZE = ABSOLUTE(credits_TEXT_END - credits_TEXT_START);
        credits_DATA_START = .;
        build/overlays/credits/asm/overlays/credits/data/rodata.rodata.o(.data);
        build/overlays/credits/asm/overlays/credits/text.o(.data);
        build/overlays/credits/asm/overlays/credits/data/data.data.o(.data);
        credits_DATA_END = .;
        credits_DATA_SIZE = ABSOLUTE(credits_DATA_END - credits_DATA_START);
        credits_BSS_START = .;
        build/overlays/credits/asm/overlays/credits/data/rodata.rodata.o(.bss);
        build/overlays/credits/asm/overlays/credits/text.o(.bss);
        credits_BSS_END = .;
        credits_BSS_SIZE = ABSOLUTE(credits_BSS_END - credits_BSS_START);
    }
    __romPos += SIZEOF(.credits);
    __romPos = ALIGN(__romPos, 16);
    credits_ROM_END = __romPos;
    credits_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
