/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-instr-2, Test Data Generator.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/rand.h>
#include <iprt/stream.h>
#include <iprt/x86.h>
#include "bs3-cpu-instr-2.h"


/*********************************************************************************************************************************
*   External Functions                                                                                                           *
*********************************************************************************************************************************/
#define PROTOTYPE_BINARY(a_Ins) \
    DECLASM(uint32_t) RT_CONCAT(GenU8_,a_Ins)( uint8_t,  uint8_t,  uint32_t, uint8_t *); \
    DECLASM(uint32_t) RT_CONCAT(GenU16_,a_Ins)(uint16_t, uint16_t, uint32_t, uint16_t *); \
    DECLASM(uint32_t) RT_CONCAT(GenU32_,a_Ins)(uint32_t, uint32_t, uint32_t, uint32_t *); \
    DECLASM(uint32_t) RT_CONCAT(GenU64_,a_Ins)(uint64_t, uint64_t, uint32_t, uint64_t *)

PROTOTYPE_BINARY(and);
PROTOTYPE_BINARY(or);
PROTOTYPE_BINARY(xor);
PROTOTYPE_BINARY(test);

PROTOTYPE_BINARY(add);
PROTOTYPE_BINARY(adc);
PROTOTYPE_BINARY(sub);
PROTOTYPE_BINARY(sbb);
PROTOTYPE_BINARY(cmp);

PROTOTYPE_BINARY(bt);
PROTOTYPE_BINARY(btc);
PROTOTYPE_BINARY(btr);
PROTOTYPE_BINARY(bts);


static uint8_t RandU8(unsigned i, unsigned iOp, unsigned iOuter = 0)
{
    RT_NOREF_PV(iOuter);
    if (i == 0)
        return 0;
    if (i == 1)
        return UINT8_MAX;
    if (i == 2)
        return iOp == 1 ? 0 : UINT8_MAX;
    return (uint8_t)RTRandU32Ex(0, UINT8_MAX);
}


static uint16_t RandU16(unsigned i, unsigned iOp, unsigned iOuter = 0)
{
    Assert(iOuter <= 1);
    if (i == 0)
        return 0;
    if (i == 1)
        return UINT16_MAX;
    if (i == 2)
        return iOp == 1 ? 0 : UINT16_MAX;
    if (iOuter == 1)
        return (uint16_t)(int16_t)(int8_t)RandU8(i, iOp);
    if ((i % 3) == 0)
        return (uint16_t)RTRandU32Ex(0, UINT16_MAX >> RTRandU32Ex(1, 11));
    return (uint16_t)RTRandU32Ex(0, UINT16_MAX);
}


static uint32_t RandU32(unsigned i, unsigned iOp, unsigned iOuter = 0)
{
    Assert(iOuter <= 1);
    if (i == 0)
        return 0;
    if (i == 1)
        return UINT32_MAX;
    if (i == 2)
        return iOp == 1 ? 0 : UINT32_MAX;
    if (iOuter == 1)
        return (uint32_t)(int32_t)(int8_t)RandU8(i, iOp);
    if ((i % 3) == 0)
        return RTRandU32Ex(0, UINT32_MAX >> RTRandU32Ex(1, 23));
    return RTRandU32();
}


static uint64_t RandU64(unsigned i, unsigned iOp, unsigned iOuter = 0)
{
    if (iOuter != 0)
    {
        Assert(iOuter <= 2);
        if (iOuter == 1)
            return (uint64_t)(int64_t)(int8_t)RTRandU32Ex(0, UINT8_MAX);
        if ((i % 2) != 0)
            return (uint64_t)(int32_t)RTRandU32();
        int32_t i32 = (int32_t)RTRandU32Ex(0, UINT32_MAX >> RTRandU32Ex(1, 23));
        if (RTRandU32Ex(0, 1) & 1)
            i32 = -i32;
        return (uint64_t)(int64_t)i32;
    }
    if (i == 0)
        return 0;
    if (i == 1)
        return UINT64_MAX;
    if (i == 2)
        return iOp == 1 ? 0 : UINT64_MAX;
    if ((i % 3) == 0)
        return RTRandU64Ex(0, UINT64_MAX >> RTRandU32Ex(1, 55));
    return RTRandU64();
}


DECL_FORCE_INLINE(uint32_t)
EnsureEflCoverage(unsigned iTest, unsigned cTests, unsigned cActiveEfls, uint32_t fActiveEfl,
                  uint32_t fSet, uint32_t fClear, uint32_t *pfMustBeClear)
{
    *pfMustBeClear = 0;
    unsigned cLeft = cTests - iTest;
    if (cLeft > cActiveEfls * 2)
        return 0;

    /* Find out which flag we're checking for now. */
    unsigned iBit = ASMBitFirstSetU32(fActiveEfl) - 1;
    while (cLeft >= 2)
    {
        cLeft -= 2;
        fActiveEfl &= ~RT_BIT_32(iBit);
        iBit = ASMBitFirstSetU32(fActiveEfl) - 1;
    }

    if (cLeft & 1)
    {
        if (!(fSet & RT_BIT_32(iBit)))
            return RT_BIT_32(iBit);
    }
    else if (!(fClear & RT_BIT_32(iBit)))
        *pfMustBeClear = RT_BIT_32(iBit);
    return 0;
}


static void FileHeader(PRTSTREAM pOut, const char *pszFilename, const char *pszIncludeBlocker)
{
    RTStrmPrintf(pOut,
                 "// ##### BEGINFILE \"%s\"\n"
                 "/* $" "Id$ */\n"
                 "/** @file\n"
                 " * BS3Kit - bs3-cpu-instr-2, %s - auto generated (do not edit).\n"
                 " */\n"
                 "\n"
                 "/*\n"
                 " * Copyright (C) 2024 Oracle and/or its affiliates.\n"
                 " *\n"
                 " * This file is part of VirtualBox base platform packages, as\n"
                 " * available from https://www.virtualbox.org.\n"
                 " *\n"
                 " * This program is free software; you can redistribute it and/or\n"
                 " * modify it under the terms of the GNU General Public License\n"
                 " * as published by the Free Software Foundation, in version 3 of the\n"
                 " * License.\n"
                 " *\n"
                 " * This program is distributed in the hope that it will be useful, but\n"
                 " * WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                 " * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
                 " * General Public License for more details.\n"
                 " *\n"
                 " * You should have received a copy of the GNU General Public License\n"
                 " * along with this program; if not, see <https://www.gnu.org/licenses>.\n"
                 " *\n"
                 " * The contents of this file may alternatively be used under the terms\n"
                 " * of the Common Development and Distribution License Version 1.0\n"
                 " * (CDDL), a copy of it is provided in the \"COPYING.CDDL\" file included\n"
                 " * in the VirtualBox distribution, in which case the provisions of the\n"
                 " * CDDL are applicable instead of those of the GPL.\n"
                 " *\n"
                 " * You may elect to license modified versions of this file under the\n"
                 " * terms and conditions of either the GPL or the CDDL or both.\n"
                 " *\n"
                 " * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0\n"
                 " */\n"
                 "\n"
                 , pszFilename, pszFilename);
    if (!pszIncludeBlocker)
        RTStrmPrintf(pOut,
                     "#include <bs3kit.h>\n"
                     "#include \"bs3-cpu-instr-2.h\"\n");
    else
        RTStrmPrintf(pOut,
                     "#ifndef %s\n"
                     "#define %s\n"
                     "#ifndef RT_WITHOUT_PRAGMA_ONCE\n"
                     "# pragma once\n"
                     "#endif\n",
                     pszIncludeBlocker, pszIncludeBlocker);
}

int main(int argc, char **argv)
{
    RTR3InitExe(argc,  &argv, 0);

    /*
     * Parse arguments.
     */
    PRTSTREAM   pOut        = g_pStdOut;
    unsigned    cTestsU8    = 48;
    unsigned    cTestsU16   = 48;
    unsigned    cTestsU32   = 48;
    unsigned    cTestsU64   = 64;

    /** @todo  */
    if (argc != 1)
    {
        RTMsgSyntax("No arguments expected");
        return RTEXITCODE_SYNTAX;
    }


    /*
     * Generate the test data.
     */
    static struct
    {
        const char *pszName;
        DECLCALLBACKMEMBER(uint32_t, pfnU8, ( uint8_t uSrc1,  uint8_t uSrc2, uint32_t fCarry,  uint8_t *puResult));
        DECLCALLBACKMEMBER(uint32_t, pfnU16,(uint16_t uSrc1, uint16_t uSrc2, uint32_t fCarry, uint16_t *puResult));
        DECLCALLBACKMEMBER(uint32_t, pfnU32,(uint32_t uSrc1, uint32_t uSrc2, uint32_t fCarry, uint32_t *puResult));
        DECLCALLBACKMEMBER(uint32_t, pfnU64,(uint64_t uSrc1, uint64_t uSrc2, uint32_t fCarry, uint64_t *puResult));
        uint8_t     cActiveEfls;
        uint16_t    fActiveEfls;
        bool        fCarryIn;
        bool        fImmVars;
    } const s_aInstr[] =
    {
        { "and",    GenU8_and,  GenU16_and,  GenU32_and,  GenU64_and,   3, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF, false, true  },
        { "or",     GenU8_or,   GenU16_or,   GenU32_or,   GenU64_or,    3, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF, false, true  },
        { "xor",    GenU8_xor,  GenU16_xor,  GenU32_xor,  GenU64_xor,   3, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF, false, true  },
        { "test",   GenU8_test, GenU16_test, GenU32_test, GenU64_test,  3, X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF, false, true  },

        { "add",    GenU8_add,  GenU16_add,  GenU32_add,  GenU64_add,   6, X86_EFL_STATUS_BITS,                  false, true  },
        { "adc",    GenU8_adc,  GenU16_adc,  GenU32_adc,  GenU64_adc,   6, X86_EFL_STATUS_BITS,                  true,  true  },
        { "sub",    GenU8_sub,  GenU16_sub,  GenU32_sub,  GenU64_sub,   6, X86_EFL_STATUS_BITS,                  false, true  },
        { "sbb",    GenU8_sbb,  GenU16_sbb,  GenU32_sbb,  GenU64_sbb,   6, X86_EFL_STATUS_BITS,                  true,  true  },
        { "cmp",    GenU8_cmp,  GenU16_cmp,  GenU32_cmp,  GenU64_cmp,   6, X86_EFL_STATUS_BITS,                  false, true  },

        { "bt",     NULL,       GenU16_bt,   GenU32_bt,   GenU64_bt,    1, X86_EFL_CF,                           false, false },
        { "btc",    NULL,       GenU16_btc,  GenU32_btc,  GenU64_btc,   1, X86_EFL_CF,                           false, false },
        { "btr",    NULL,       GenU16_btr,  GenU32_btr,  GenU64_btr,   1, X86_EFL_CF,                           false, false },
        { "bts",    NULL,       GenU16_bts,  GenU32_bts,  GenU64_bts,   1, X86_EFL_CF,                           false, false },
    };

    RTStrmPrintf(pOut, "\n"); /* filesplitter requires this. */

    /* Header: */
    FileHeader(pOut, "bs3-cpu-instr-2-data.h", "VBOX_INCLUDED_SRC_bootsectors_bs3_cpu_instr_2_data_h");
    for (unsigned iInstr = 0; iInstr < RT_ELEMENTS(s_aInstr); iInstr++)
    {
#define DO_ONE_TYPE(a_ValueType, a_cBits, a_szFmt, a_pfnMember, a_cTests) do { \
                RTStrmPrintf(pOut, \
                             "\n" \
                             "extern const unsigned g_cBs3CpuInstr2_%s_TestDataU" #a_cBits ";\n" \
                             "extern const BS3CPUINSTR2BIN" #a_cBits " g_aBs3CpuInstr2_%s_TestDataU" #a_cBits "[];\n", \
                             s_aInstr[iInstr].pszName, s_aInstr[iInstr].pszName); \
            } while (0)
        if (s_aInstr[iInstr].pfnU8)
            DO_ONE_TYPE(uint8_t,  8,   "%#04RX8",  pfnU8,  cTestsU8);
        if (s_aInstr[iInstr].pfnU16)
            DO_ONE_TYPE(uint16_t, 16, "%#06RX16",  pfnU16, cTestsU16);
        if (s_aInstr[iInstr].pfnU32)
            DO_ONE_TYPE(uint32_t, 32, "%#010RX32", pfnU32, cTestsU32);
        if (s_aInstr[iInstr].pfnU64)
            DO_ONE_TYPE(uint64_t, 64, "%#018RX64", pfnU64, cTestsU64);
#undef DO_ONE_TYPE
    }
    RTStrmPrintf(pOut,
                 "\n"
                 "#endif /* !VBOX_INCLUDED_SRC_bootsectors_bs3_cpu_instr_2_data_h */\n"
                 "\n// ##### ENDFILE\n");

#define DO_ONE_TYPE(a_ValueType, a_cBits, a_szFmt, a_pfnMember, a_cTests) do { \
                unsigned const cOuterLoops =  1 + s_aInstr[iInstr].fImmVars * (a_cBits == 64 ? 2 : a_cBits != 8 ? 1 : 0); \
                unsigned const cTestFactor = !s_aInstr[iInstr].fCarryIn ? 1 : 2; \
                RTStrmPrintf(pOut, \
                             "\n" \
                             "const unsigned g_cBs3CpuInstr2_%s_TestDataU" #a_cBits " = %u;\n" \
                             "const BS3CPUINSTR2BIN" #a_cBits " g_aBs3CpuInstr2_%s_TestDataU" #a_cBits "[%u] =\n" \
                             "{\n", \
                             s_aInstr[iInstr].pszName, a_cTests * cTestFactor * cOuterLoops, \
                             s_aInstr[iInstr].pszName, a_cTests * cTestFactor * cOuterLoops); \
                for (unsigned iOuter = 0; iOuter < cOuterLoops; iOuter++) \
                { \
                    if (iOuter != 0) \
                        RTStrmPrintf(pOut, "    /* r/m" #a_cBits", imm%u: */\n", iOuter == 1 ? 8 : 32); \
                    uint32_t fSet   = 0; \
                    uint32_t fClear = 0; \
                    for (unsigned iTest = 0; iTest < a_cTests; iTest++) \
                    { \
                        uint32_t fMustBeClear = 0; \
                        uint32_t fMustBeSet   = EnsureEflCoverage(iTest, a_cTests, s_aInstr[iInstr].cActiveEfls, \
                                                                  s_aInstr[iInstr].fActiveEfls, fSet, fClear, &fMustBeClear); \
                        for (unsigned iTry = 0;; iTry++) \
                        { \
                            a_ValueType const uSrc1   = RandU##a_cBits(iTest + iTry, 1); \
                            a_ValueType const uSrc2   = RandU##a_cBits(iTest + iTry, 2, iOuter); \
                            a_ValueType       uResult = 0; \
                            uint32_t          fEflOut = s_aInstr[iInstr].a_pfnMember(uSrc1, uSrc2, 0 /*fCarry*/, &uResult) \
                                                      & X86_EFL_STATUS_BITS; \
                            if (iTry < _1M && ((fEflOut & fMustBeClear) || (~fEflOut & fMustBeSet))) \
                                continue; \
                            fSet   |= fEflOut; \
                            fClear |= ~fEflOut; \
                            RTStrmPrintf(pOut,  "    { " a_szFmt ", " a_szFmt ", " a_szFmt ", %#05RX16 },\n", \
                                         uSrc1, uSrc2, uResult, fEflOut); \
                            if (s_aInstr[iInstr].fCarryIn) \
                            { \
                                uResult = 0; \
                                fEflOut = s_aInstr[iInstr].a_pfnMember(uSrc1, uSrc2, X86_EFL_CF, &uResult) & X86_EFL_STATUS_BITS; \
                                fSet   |= fEflOut; \
                                fClear |= ~fEflOut; \
                                RTStrmPrintf(pOut,  "    { " a_szFmt ", " a_szFmt ", " a_szFmt ", %#05RX16 },\n", \
                                             uSrc1, uSrc2, uResult, (fEflOut | RT_BIT_32(BS3CPUINSTR2BIN_EFL_CARRY_IN_BIT))); \
                            } \
                            break; \
                        } \
                    } \
                } \
                RTStrmPrintf(pOut, \
                             "};\n"); \
            } while (0)

    /* Source: 8, 16 & 32 bit data. */
    FileHeader(pOut, "bs3-cpu-instr-2-data16.c16", NULL);
    for (unsigned iInstr = 0; iInstr < RT_ELEMENTS(s_aInstr); iInstr++)
    {
        if (s_aInstr[iInstr].pfnU8)
            DO_ONE_TYPE(uint8_t,  8,   "%#04RX8",  pfnU8,  cTestsU8);
        if (s_aInstr[iInstr].pfnU16)
            DO_ONE_TYPE(uint16_t, 16, "%#06RX16",  pfnU16, cTestsU16);
        if (s_aInstr[iInstr].pfnU32)
            DO_ONE_TYPE(uint32_t, 32, "%#010RX32", pfnU32, cTestsU32);
    }
    RTStrmPrintf(pOut, "\n// ##### ENDFILE\n");

    /* Source: 64 bit data (goes in different data segment). */
    FileHeader(pOut, "bs3-cpu-instr-2-data64.c64", NULL);
    for (unsigned iInstr = 0; iInstr < RT_ELEMENTS(s_aInstr); iInstr++)
        if (s_aInstr[iInstr].pfnU64)
            DO_ONE_TYPE(uint64_t, 64, "%#018RX64", pfnU64, cTestsU64);
    RTStrmPrintf(pOut, "\n// ##### ENDFILE\n");
#undef DO_ONE_TYPE

    return 0;
}

