/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2025 xsysinfo contributors.
 * xSysInfo adaptation Copyright (c) 2026 John Knipper.
 * Derived from xSysInfo 0ee81383380251df0fc67f78c0282530a7dcff84.
 * Original attribution retained below; see LICENSE.xSysInfo.
 * Per-worker explicit state replaces globals; loop work is retained.
 */
/*
 ****************************************************************************
 *
 *                   "DHRYSTONE" Benchmark Program
 *                   -----------------------------
 *
 *  Version:    C, Version 2.1
 *
 *  File:       dhry_1.c (part 2 of 3)
 *
 *  Date:       May 25, 1988
 *
 *  Author:     Reinhold P. Weicker
 *
 ****************************************************************************
 */

#include "dhry.h"
#include <limits.h>

#ifndef REG
#define REG
        /* REG becomes defined as empty */
        /* i.e. no register variables   */
#endif

int Dhry_Initialize(DhryState *ctx)
{
  Ptr_Glob = &ctx->records[0];
  Next_Ptr_Glob = &ctx->records[1];

  memset (Ptr_Glob, 0, sizeof (Rec_Type));
  memset (Next_Ptr_Glob, 0, sizeof (Rec_Type));

  /* memset only clears the first line, use nested loop instead */
  for (int i = 0; i < 50; ++i) {
    Arr_1_Glob[i] = 0;
    for (int j = 0; j < 50; ++j) {
      Arr_2_Glob[i][j] = 0;
    }
  }

  Ptr_Glob->Ptr_Comp                    = Next_Ptr_Glob;
  Ptr_Glob->Discr                       = Ident_1;
  Ptr_Glob->variant.var_1.Enum_Comp     = Ident_3;
  Ptr_Glob->variant.var_1.Int_Comp      = 40;
  strcpy (Ptr_Glob->variant.var_1.Str_Comp,
          "DHRYSTONE PROGRAM, SOME STRING");

  Next_Ptr_Glob->Ptr_Comp               = Ptr_Glob;
  Next_Ptr_Glob->Discr                  = Ident_1;
  Next_Ptr_Glob->variant.var_1.Enum_Comp= Ident_3;
  Next_Ptr_Glob->variant.var_1.Int_Comp = 40;
  strcpy (Next_Ptr_Glob->variant.var_1.Str_Comp,
          "DHRYSTONE PROGRAM, SOME STRING");

  Arr_2_Glob [8][7] = 10;
        /* Was missing in published program. Without this statement,    */
        /* Arr_2_Glob [8][7] would have an undefined value.             */
        /* Warning: With 16-Bit processors and Number_Of_Runs > 32000,  */
        /* overflow may occur for this array element.                   */

  Int_Glob = 0;
  Bool_Glob = false;
  Ch_1_Glob = ' ';
  Ch_2_Glob = ' ';

  return 1;
}

void Dhry_Run(DhryState *ctx, unsigned long Number_Of_Runs)
{
  One_Fifty       Int_1_Loc;
  One_Fifty       Int_2_Loc;
  One_Fifty       Int_3_Loc;
  REG   char      Ch_Index;
  Enumeration     Enum_Loc;
  Str_30          Str_1_Loc;
  Str_30          Str_2_Loc;
  unsigned long   Run_Index;
  unsigned long   Runs_To_Do;

  if (Number_Of_Runs == 0 || Ptr_Glob == Null || Next_Ptr_Glob == Null)
    return;

  strcpy (Str_1_Loc, "DHRYSTONE PROGRAM, 1'ST STRING");

  Arr_2_Glob[8][7] = 10;
  Runs_To_Do = Number_Of_Runs;
  if (Runs_To_Do > (unsigned long)(INT_MAX - 10))
    Runs_To_Do = (unsigned long)(INT_MAX - 10);

  for (Run_Index = 1; Run_Index <= Runs_To_Do; ++Run_Index)
  {

    Proc_5(ctx);
    Proc_4(ctx);
      /* Ch_1_Glob == 'A', Ch_2_Glob == 'B', Bool_Glob == true */
    Int_1_Loc = 2;
    Int_2_Loc = 3;
    strcpy (Str_2_Loc, "DHRYSTONE PROGRAM, 2'ND STRING");
    Enum_Loc = Ident_2;
    Bool_Glob = ! Func_2(ctx, Str_1_Loc, Str_2_Loc);
      /* Bool_Glob == 1 */
    while (Int_1_Loc < Int_2_Loc)  /* loop body executed once */
    {
      Int_3_Loc = 5 * Int_1_Loc - Int_2_Loc;
        /* Int_3_Loc == 7 */
      Proc_7(ctx, Int_1_Loc, Int_2_Loc, &Int_3_Loc);
        /* Int_3_Loc == 7 */
      Int_1_Loc += 1;
    } /* while */
      /* Int_1_Loc == 3, Int_2_Loc == 3, Int_3_Loc == 7 */
    Proc_8(ctx, Arr_1_Glob, Arr_2_Glob, Int_1_Loc, Int_3_Loc);
      /* Int_Glob == 5 */
    Proc_1(ctx, Ptr_Glob);
    for (Ch_Index = 'A'; Ch_Index <= Ch_2_Glob; ++Ch_Index)
                             /* loop body executed twice */
    {
      if (Enum_Loc == Func_1(ctx, Ch_Index, 'C'))
          /* then, not executed */
        {
        Proc_6(ctx, Ident_1, &Enum_Loc);
        strcpy (Str_2_Loc, "DHRYSTONE PROGRAM, 3'RD STRING");
        Int_2_Loc = (int)Run_Index;
        Int_Glob = (int)Run_Index;
        }
    }
      /* Int_1_Loc == 3, Int_2_Loc == 3, Int_3_Loc == 7 */
    Int_2_Loc = Int_2_Loc * Int_1_Loc;
    Int_1_Loc = Int_2_Loc / Int_3_Loc;
    Int_2_Loc = 7 * (Int_2_Loc - Int_3_Loc) - Int_1_Loc;
      /* Int_1_Loc == 1, Int_2_Loc == 13, Int_3_Loc == 7 */
    Proc_2(ctx, &Int_1_Loc);
      /* Int_1_Loc == 5 */

  } /* loop "for Run_Index" */
  ctx->local1 = Int_1_Loc; ctx->local2 = Int_2_Loc;
  ctx->local3 = Int_3_Loc; ctx->enumeration = Enum_Loc;
}


void Proc_1(DhryState *ctx, REG Rec_Pointer Ptr_Val_Par)
/******************/
    /* executed once */
{
  REG Rec_Pointer Next_Record = Ptr_Val_Par->Ptr_Comp;
                                        /* == Ptr_Glob_Next */
  /* Local variable, initialized with Ptr_Val_Par->Ptr_Comp,    */
  /* corresponds to "rename" in Ada, "with" in Pascal           */

  structassign (*Ptr_Val_Par->Ptr_Comp, *Ptr_Glob);
  Ptr_Val_Par->variant.var_1.Int_Comp = 5;
  Next_Record->variant.var_1.Int_Comp
        = Ptr_Val_Par->variant.var_1.Int_Comp;
  Next_Record->Ptr_Comp = Ptr_Val_Par->Ptr_Comp;
  Proc_3(ctx, &Next_Record->Ptr_Comp);
    /* Ptr_Val_Par->Ptr_Comp->Ptr_Comp
                        == Ptr_Glob->Ptr_Comp */
  if (Next_Record->Discr == Ident_1)
    /* then, executed */
  {
    Next_Record->variant.var_1.Int_Comp = 6;
    Proc_6(ctx, Ptr_Val_Par->variant.var_1.Enum_Comp,
           &Next_Record->variant.var_1.Enum_Comp);
    Next_Record->Ptr_Comp = Ptr_Glob->Ptr_Comp;
    Proc_7(ctx, Next_Record->variant.var_1.Int_Comp, 10,
           &Next_Record->variant.var_1.Int_Comp);
  }
  else /* not executed */
    structassign (*Ptr_Val_Par, *Ptr_Val_Par->Ptr_Comp);
} /* Proc_1 */


void Proc_2(DhryState *ctx, One_Fifty *Int_Par_Ref)
/******************/
    /* executed once */
    /* *Int_Par_Ref == 1, becomes 4 */
{
  One_Fifty  Int_Loc;
  Enumeration   Enum_Loc;

  Int_Loc = *Int_Par_Ref + 10;
  do /* executed once */
    if (Ch_1_Glob == 'A')
      /* then, executed */
    {
      Int_Loc -= 1;
      *Int_Par_Ref = Int_Loc - Int_Glob;
      Enum_Loc = Ident_1;
    } /* if */
  while (Enum_Loc != Ident_1); /* true */
} /* Proc_2 */


void Proc_3(DhryState *ctx, Rec_Pointer *Ptr_Ref_Par)
/******************/
    /* executed once */
    /* Ptr_Ref_Par becomes Ptr_Glob */
{
  if (Ptr_Glob != Null)
    /* then, executed */
    *Ptr_Ref_Par = Ptr_Glob->Ptr_Comp;
  Proc_7(ctx, 10, Int_Glob, &Ptr_Glob->variant.var_1.Int_Comp);
} /* Proc_3 */


void Proc_4(DhryState *ctx) /* without parameters */
/*******/
    /* executed once */
{
  Boolean Bool_Loc;

  Bool_Loc = Ch_1_Glob == 'A';
  Bool_Glob = Bool_Loc | Bool_Glob;
  Ch_2_Glob = 'B';
} /* Proc_4 */


void Proc_5(DhryState *ctx) /* without parameters */
/*******/
    /* executed once */
{
  Ch_1_Glob = 'A';
  Bool_Glob = false;
} /* Proc_5 */


        /* Procedure for the assignment of structures,          */
        /* if the C compiler doesn't support this feature       */
#ifdef  NOSTRUCTASSIGN
memcpy (d, s, l)
register char   *d;
register char   *s;
register int    l;
{
        while (l--) *d++ = *s++;
}
#endif

