SECTIONS
{
    __romPos = 0;
    level_5_artisans_sunny_flight_ROM_START = __romPos;
    level_5_artisans_sunny_flight_VRAM = ADDR(.level_5_artisans_sunny_flight);
    .level_5_artisans_sunny_flight 0x8007AA38 : AT(level_5_artisans_sunny_flight_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_5_artisans_sunny_flight_RODATA_START = .;
        build/overlays/level_5_artisans_sunny_flight/asm/overlays/level_5_artisans_sunny_flight/data/rodata.rodata.o(.rodata);
        build/overlays/level_5_artisans_sunny_flight/asm/overlays/level_5_artisans_sunny_flight/text.o(.rodata);
        level_5_artisans_sunny_flight_RODATA_END = .;
        level_5_artisans_sunny_flight_RODATA_SIZE = ABSOLUTE(level_5_artisans_sunny_flight_RODATA_END - level_5_artisans_sunny_flight_RODATA_START);
        level_5_artisans_sunny_flight_TEXT_START = .;
        build/overlays/level_5_artisans_sunny_flight/asm/overlays/level_5_artisans_sunny_flight/text.o(.text);
        level_5_artisans_sunny_flight_TEXT_END = .;
        level_5_artisans_sunny_flight_TEXT_SIZE = ABSOLUTE(level_5_artisans_sunny_flight_TEXT_END - level_5_artisans_sunny_flight_TEXT_START);
        level_5_artisans_sunny_flight_DATA_START = .;
        build/overlays/level_5_artisans_sunny_flight/asm/overlays/level_5_artisans_sunny_flight/data/rodata.rodata.o(.data);
        build/overlays/level_5_artisans_sunny_flight/asm/overlays/level_5_artisans_sunny_flight/text.o(.data);
        build/overlays/level_5_artisans_sunny_flight/asm/overlays/level_5_artisans_sunny_flight/data/data.data.o(.data);
        level_5_artisans_sunny_flight_DATA_END = .;
        level_5_artisans_sunny_flight_DATA_SIZE = ABSOLUTE(level_5_artisans_sunny_flight_DATA_END - level_5_artisans_sunny_flight_DATA_START);
        level_5_artisans_sunny_flight_BSS_START = .;
        build/overlays/level_5_artisans_sunny_flight/asm/overlays/level_5_artisans_sunny_flight/data/rodata.rodata.o(.bss);
        build/overlays/level_5_artisans_sunny_flight/asm/overlays/level_5_artisans_sunny_flight/text.o(.bss);
        level_5_artisans_sunny_flight_BSS_END = .;
        level_5_artisans_sunny_flight_BSS_SIZE = ABSOLUTE(level_5_artisans_sunny_flight_BSS_END - level_5_artisans_sunny_flight_BSS_START);
    }
    __romPos += SIZEOF(.level_5_artisans_sunny_flight);
    __romPos = ALIGN(__romPos, 16);
    level_5_artisans_sunny_flight_ROM_END = __romPos;
    level_5_artisans_sunny_flight_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
