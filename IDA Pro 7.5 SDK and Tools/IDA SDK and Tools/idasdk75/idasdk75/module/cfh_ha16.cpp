
// This file contains a custom fixup handler for the HA16 fixup type.
// It is used by PPC, MIPS, and similar processors.
// Also thie file contains a custom refinfo handler for the HA16 type.

#include <fixup.hpp>

//--------------------------------------------------------------------------
// 'apply' a fixup: take it into account while analyzing the file
// usually it consists of converting the operand into an offset expression
static bool idaapi ha16_apply(
        const fixup_handler_t *fh,
        ea_t item_ea,
        ea_t fixup_ea,
        int opnum,
        bool is_macro,
        const fixup_data_t &fd)
{
  if ( is_unknown(get_flags(item_ea)) )
    create_16bit_data(fixup_ea, 2);

  refinfo_t ri;
  ri.base   = fd.get_base();
  ri.target = ri.base + fd.off;
  ri.tdelta = fd.displacement;
  ri.flags = REFINFO_CUSTOM;

  if ( is_macro )
  { // it is a macro instruction, check for the second fixup within it
    fixup_data_t fdl;
    ea_t end = get_item_end(item_ea);
    while ( true )
    {
      fixup_ea = get_next_fixup_ea(fixup_ea);
      if ( fixup_ea >= end )
        return false; // could not find the fixup for the low part
      if ( fdl.get(fixup_ea) && fdl.get_type() == FIXUP_LOW16 )
        break; // found the pair!
    }
//    if ( int16(fdl.off + fdl.displacement) < 0 )
//      ri->tdelta -= 0x10000;
    ri.flags = REF_OFF32;
    ri.target = BADADDR;
  }
  else
  {
    ri.set_type(fh->reftype);
  }
  if ( processor_t::adjust_refinfo(&ri, fixup_ea, opnum, fd) < 0 )
    return false;
  op_offset_ex(item_ea, opnum, &ri);
  return true;
}

//--------------------------------------------------------------------------
static uval_t idaapi ha16_get_value(const fixup_handler_t * /*fh*/, ea_t ea)
{
  // we can't get the exact value of this fixup as it depends on the low
  // part
  return get_word(ea) << 16;
}

//----------------------------------------------------------------------------
static bool idaapi ha16_patch_value(
        const fixup_handler_t * /*fh*/,
        ea_t ea,
        const fixup_data_t &fd)
{
  // compensate signed low part
  ea_t expr = fd.off + fd.displacement + 0x8000;
#ifdef __EA64__
  // in the case of 32-bit reloc we have to check overflow,
  // for simplicity we don't do.
  // overflow = is32bit_reloc && expr > UINT_MAX;
#endif
  put_word(ea, expr >> 16);
  return true;
}

//--------------------------------------------------------------------------
#define DEF_CFH_HA16(name) fixup_handler_t name =        \
{                                                        \
  sizeof(fixup_handler_t),                               \
  "HIGHA16",                    /* name */               \
  0,                            /* props */              \
  2, 0, 0, 0,                   /* size, width, shift */ \
  REFINFO_CUSTOM,               /* reftype */            \
  ha16_apply,                   /* apply */              \
  ha16_get_value,               /* get_value */          \
  ha16_patch_value,             /* patch_value */        \
}

// ALPHA, MIPS, PPC, TRICORE include this file but do not use fixup handler.
static QUNUSED DEF_CFH_HA16(cfh_ha16);
//lint -esym(2466,cfh_ha16) was used despite being marked as 'unused'

//--------------------------------------------------------------------------
inline bool was_displacement_generated(const char *buf)
{
  const char *ptr = strchr(buf, COLOR_SYMBOL);    // is there a displacement?
  return ptr != NULL && (ptr[1] == '+' || ptr[1] == '-');
}

//--------------------------------------------------------------------------
// get reference data from ri,
// check compliance of opval and the full value
static bool idaapi ha16_calc_reference_data(
        ea_t *target,
        ea_t *base,
        ea_t /*from*/,
        const refinfo_t &ri,
        adiff_t opval)
{
  if ( ri.target == BADADDR
    || ri.base == BADADDR
    || ri.is_subtract() )
  {
    return false;
  }
  ea_t fullvalue = ri.target + ri.tdelta - ri.base;
  int16 calc_opval = (fullvalue >> 16) & 0xFFFF;
  if ( (fullvalue & 0x8000) != 0 )
    calc_opval += 1;
  if ( calc_opval != int16(opval) )
    return false;

  *target = ri.target;
  *base = ri.base;
  return true;
}

//--------------------------------------------------------------------------
// simple format
static void idaapi ha16_get_format(qstring *format)
{
  *format = COLSTR("%s@ha", SCOLOR_KEYWORD);
}

//--------------------------------------------------------------------------
static const custom_refinfo_handler_t ref_ha16 =
{
  sizeof(custom_refinfo_handler_t),
  "HIGHA16",
  "high adjusted 16 bits of 32-bit offset",
  0,                        // properties (currently 0)
  NULL,                     // gen_expr
  ha16_calc_reference_data, // calc_reference_data
  ha16_get_format,          // get_format
};
