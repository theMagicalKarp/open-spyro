#ifndef SDATA_H
#define SDATA_H

/* SDATA(sym, size): declare an extern as small data (.sdata/.sbss) in this
   translation unit, so its accesses assemble $gp-relative
   (`lw $r,%gp_rel(sym)($gp)`) instead of lui/%lo. The original assembler did
   this from gcc's `.extern sym,size` lines; maspsx only honours symbols it sees
   sized inside an .sdata section, so this hands it a `.size` there. The symbol
   stays undefined (the link pins it at its original address) and no bytes are
   emitted. Per-TU on purpose: several of these globals are also accessed via
   lui/%lo from other functions. */
#if !defined(M2CTX) && !defined(PERMUTER)
#define SDATA(sym, size) \
    __asm__(".sdata\n.size " #sym "," #size "\n.text")
#else
#define SDATA(sym, size)
#endif

#endif /* SDATA_H */
