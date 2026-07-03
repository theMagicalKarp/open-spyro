SECTIONS
{
    __romPos = 0;
    level_23_beast_makers_wild_flight_ROM_START = __romPos;
    level_23_beast_makers_wild_flight_VRAM = ADDR(.level_23_beast_makers_wild_flight);
    .level_23_beast_makers_wild_flight 0x8007AA38 : AT(level_23_beast_makers_wild_flight_ROM_START) SUBALIGN(2)
    {
        FILL(0x00000000);
        level_23_beast_makers_wild_flight_RODATA_START = .;
        /* Per-piece fixed-VMA rodata slots (`open-spyro gen-slots-ld --overlay level_23_beast_makers_wild_flight`). */
        INCLUDE config/overlays/level_23_beast_makers_wild_flight.rodata_slots.ld
        build/overlays/level_23_beast_makers_wild_flight/asm/overlays/level_23_beast_makers_wild_flight/data/rodata.rodata.o(.rodata);
        build/overlays/level_23_beast_makers_wild_flight/asm/overlays/level_23_beast_makers_wild_flight/text.o(.rodata);
        level_23_beast_makers_wild_flight_RODATA_END = .;
        level_23_beast_makers_wild_flight_RODATA_SIZE = ABSOLUTE(level_23_beast_makers_wild_flight_RODATA_END - level_23_beast_makers_wild_flight_RODATA_START);
        level_23_beast_makers_wild_flight_TEXT_START = .;
        /* Per-function fixed-VMA text slots (`open-spyro gen-slots-ld --overlay level_23_beast_makers_wild_flight`). */
        INCLUDE config/overlays/level_23_beast_makers_wild_flight.slots.ld
        build/overlays/level_23_beast_makers_wild_flight/asm/overlays/level_23_beast_makers_wild_flight/text.o(.text);
        level_23_beast_makers_wild_flight_TEXT_END = .;
        level_23_beast_makers_wild_flight_TEXT_SIZE = ABSOLUTE(level_23_beast_makers_wild_flight_TEXT_END - level_23_beast_makers_wild_flight_TEXT_START);
        level_23_beast_makers_wild_flight_DATA_START = .;
        build/overlays/level_23_beast_makers_wild_flight/asm/overlays/level_23_beast_makers_wild_flight/data/rodata.rodata.o(.data);
        build/overlays/level_23_beast_makers_wild_flight/asm/overlays/level_23_beast_makers_wild_flight/text.o(.data);
        build/overlays/level_23_beast_makers_wild_flight/asm/overlays/level_23_beast_makers_wild_flight/data/data.data.o(.data);
        level_23_beast_makers_wild_flight_DATA_END = .;
        level_23_beast_makers_wild_flight_DATA_SIZE = ABSOLUTE(level_23_beast_makers_wild_flight_DATA_END - level_23_beast_makers_wild_flight_DATA_START);
        level_23_beast_makers_wild_flight_BSS_START = .;
        build/overlays/level_23_beast_makers_wild_flight/asm/overlays/level_23_beast_makers_wild_flight/data/rodata.rodata.o(.bss);
        build/overlays/level_23_beast_makers_wild_flight/asm/overlays/level_23_beast_makers_wild_flight/text.o(.bss);
        level_23_beast_makers_wild_flight_BSS_END = .;
        level_23_beast_makers_wild_flight_BSS_SIZE = ABSOLUTE(level_23_beast_makers_wild_flight_BSS_END - level_23_beast_makers_wild_flight_BSS_START);
    }
    __romPos += SIZEOF(.level_23_beast_makers_wild_flight);
    __romPos = ALIGN(__romPos, 16);
    level_23_beast_makers_wild_flight_ROM_END = __romPos;
    level_23_beast_makers_wild_flight_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
