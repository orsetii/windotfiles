/*
 *      Interactive disassembler (IDA).
 *      Copyright (c) 1990-2020 Hex-Rays
 *      ALL RIGHTS RESERVED.
 *
 */

#ifndef FIXUP_HPP
#define FIXUP_HPP

#include <nalt.hpp>
#include <segment.hpp>

/*! \file fixup.hpp

  \brief Functions that deal with fixup information

  A loader should setup fixup information using set_fixup().
*/

//--------------------------------------------------------------------------
/// Fixup information structure

typedef uint16 fixup_type_t;  ///< see \ref fixup_type_t
/// \defgroup fixup_type_t Types of fixups
/// Fixup may be of standard or custom type. Standard types fall into
/// range 1..FIXUP_CUSTOM-1 and custom types fall into range
/// FIXUP_CUSTOM..MAX_UINT16.
/// \note Fixup type 0 is unused.
/// \name Fixup standard types
//@{
#define FIXUP_OFF8       13     ///< 8-bit offset
#define FIXUP_OFF16       1     ///< 16-bit offset
#define FIXUP_SEG16       2     ///< 16-bit base--logical segment base (selector)
#define FIXUP_PTR16       3     ///< 32-bit long pointer (16-bit base:16-bit
                                ///< offset)
#define FIXUP_OFF32       4     ///< 32-bit offset
#define FIXUP_PTR32       5     ///< 48-bit pointer (16-bit base:32-bit offset)
#define FIXUP_HI8         6     ///< high  8 bits of 16bit offset
#define FIXUP_HI16        7     ///< high 16 bits of 32bit offset
#define FIXUP_LOW8        8     ///< low   8 bits of 16bit offset
#define FIXUP_LOW16       9     ///< low  16 bits of 32bit offset
#define V695_FIXUP_VHIGH 10     ///< obsolete
#define V695_FIXUP_VLOW  11     ///< obsolete
#define FIXUP_OFF64      12     ///< 64-bit offset
//#define FIXUP_          0xE
#define FIXUP_CUSTOM 0x8000     ///< start of the custom types range
//@}

/// Is fixup processed by processor module?
inline THREAD_SAFE bool is_fixup_custom(fixup_type_t type)
{
  return (type & FIXUP_CUSTOM) != 0;
}

/// \defgroup FIXUPF_ Fixup flags
/// Used by fixup_data_t
//@{
/// fixup is relative to the linear address `base'. Otherwise fixup is
/// relative to the start of the segment with `sel' selector.
#define FIXUPF_REL         0x0001
/// target is a location (otherwise - segment).
/// Use this bit if the target is a symbol rather than an offset from the
/// beginning of a segment.
#define FIXUPF_EXTDEF      0x0002
/// fixup is ignored by IDA
///   - disallows the kernel to convert operands
///   - this fixup is not used during output
#define FIXUPF_UNUSED      0x0004
/// fixup was not present in the input file
#define FIXUPF_CREATED     0x0008
/// additional flags. The bits from this mask are not stored in the database
/// and can be used by the loader at its discretion.
#define FIXUPF_LOADER_MASK 0xF0000000
//@}

struct fixup_handler_t;
struct fixup_data_t
{
protected:
  fixup_type_t type;    // fixup type
  uint32 flags;         // FIXUPF_... bits
  uval_t base;          // base for relative fixups

public:
  sel_t sel;            ///< selector of the target segment.
                        ///< BADSEL means an absolute (zero based) target.
                        ///< \sa FIXUPF_REL

  ea_t off;             ///< target offset
                        ///< \note The target is calculated as
                        ///< `get_base() + off`.

  adiff_t displacement; ///< displacement (offset from the target)

public:
  fixup_data_t()
    : type(0),
      flags(0),
      base(0),
      sel(BADSEL),
      off(0),
      displacement(0) {}
  fixup_data_t(fixup_type_t type_, uint32 flags_ = 0)
    : type(type_),
      flags(flags_),
      base(0),
      sel(BADSEL),
      off(0),
      displacement(0) {}

  /// Fixup type \ref fixup_type_t
  fixup_type_t get_type(void) const { return type; }
  void set_type(fixup_type_t type_) { type = type_; }
  void set_type_and_flags(fixup_type_t type_, uint32 flags_ = 0)
  {
    type = type_;
    flags = flags_;
  }

  bool is_custom(void) const;       ///< \ref is_fixup_custom()

  /// Fixup flags \ref FIXUPF_
  uint32 get_flags() const { return flags; }

  bool is_extdef(void) const { return (flags & FIXUPF_EXTDEF) != 0; }
  void set_extdef(void) { flags |= FIXUPF_EXTDEF; }
  void clr_extdef(void) { flags &= ~FIXUPF_EXTDEF; }

  bool is_unused(void) const { return (flags & FIXUPF_UNUSED) != 0; }
  void set_unused(void) { flags |= FIXUPF_UNUSED; }
  void clr_unused(void) { flags &= ~FIXUPF_UNUSED; }

  /// Is fixup relative?
  bool has_base(void) const { return (flags & FIXUPF_REL) != 0; }

  /// Is fixup artificial?
  bool was_created(void) const { return (flags & FIXUPF_CREATED) != 0; }

  /// Get base of fixup.
  /// \note The target is calculated as `get_base() + off`.
  /// \sa FIXUPF_REL
  ea_t get_base() const
  {
    return has_base() ? base : sel != BADSEL ? sel2ea(sel) : 0;
  }

  /// Set base of fixup.
  /// The target should be set before a call of this function.
  void set_base(ea_t new_base)
  {
    ea_t target = get_base() + off;
    flags |= FIXUPF_REL;
    base = new_base;
    off = target - base;
  }

  void set_sel(const segment_t *seg)
  {
    sel = seg == NULL ? BADSEL : seg->sel;
  }

  /// Set selector of fixup to the target.
  /// The target should be set before a call of this function.
  void set_target_sel()
  {
    ea_t target = get_base() + off;
    set_sel(getseg(target));
    flags &= ~FIXUPF_REL;
    base = 0; // just in case
    off = target - get_base();
  }

  void set(ea_t source) const;      ///< \ref set_fixup()
  bool get(ea_t source);            ///< \ref get_fixup()

  /// \ref get_fixup_handler()
  const fixup_handler_t *get_handler() const;

  /// \ref get_fixup_desc()
  const char *get_desc(qstring *buf, ea_t source) const;

  // TODO rewrite to the inline implementation which uses
  // fixup_handler_t::size
  int calc_size() const;            ///< \ref calc_fixup_size()
  uval_t get_value(ea_t ea) const;  ///< \ref get_fixup_value()
  bool patch_value(ea_t ea) const;  ///< \ref patch_fixup_value()

};

/// Get fixup information

idaman bool ida_export get_fixup(fixup_data_t *fd, ea_t source);


/// Check that a fixup exists at the given address

inline bool exists_fixup(ea_t source)
{
  return get_fixup(NULL, source);
}


/// Set fixup information. You should fill ::fixup_data_t and call this
/// function and the kernel will remember information in the database.
/// \param source  the fixup source address, i.e. the address modified by
///                the fixup
/// \param fd      fixup data

idaman void ida_export set_fixup(ea_t source, const fixup_data_t &fd);


/// Delete fixup information

idaman void ida_export del_fixup(ea_t source);


/// \name Enumerate addresses with fixup information:
//@{
/// Get the first address with fixup information
///
/// \return the first address with fixup information, or BADADDR
idaman ea_t ida_export get_first_fixup_ea(void);

/// Find next address with fixup information
///
/// \param ea current address
/// \return the next address with fixup information, or BADADDR
idaman ea_t ida_export get_next_fixup_ea(ea_t ea);

/// Find previous address with fixup information
///
/// \param ea current address
/// \return the previous address with fixup information, or BADADDR
idaman ea_t ida_export get_prev_fixup_ea(ea_t ea);
//@}


/// Get handler of standard or custom fixup

idaman const fixup_handler_t * ida_export get_fixup_handler(fixup_type_t type);


/// Use fixup information for an address.
/// This function converts item_ea flags to offsets/segments.
/// For undefined bytes, you may set item_ea == fixup_ea. In this case this
/// function will create an item (byte, word, dword) there.
/// \param item_ea   start address of item to modify
/// \param fixup_ea  address of fixup record
/// \param n         number of operand. may be 0, 1, 2, or OPND_ALL
/// \param is_macro  is the instruction at 'item_ea' a macro?
///                  if yes, then partial fixups (HIGH, LOW) won't be applied
/// \retval false  no fixup at fixup_ea or it has #FIXUPF_UNUSED flag
/// \retval true   ok, the fixup information was applied

idaman bool ida_export apply_fixup(ea_t item_ea, ea_t fixup_ea, int n, bool is_macro);


/// Get the operand value.
/// This function get fixup bytes from data or an instruction at `ea' and
/// convert them to the operand value (maybe partially).
/// It is opposite in meaning to the `patch_fixup_value()`.
/// For example, FIXUP_HI8 read a byte at `ea' and shifts it left by 8 bits,
/// or AArch64's custom fixup BRANCH26 get low 26 bits of the insn at `ea'
/// and shifts it left by 2 bits.
/// This function is mainly used to get a relocation addend.
/// \param ea    address to get fixup bytes from, the size of the fixup
///              bytes depends on the fixup type.
///              \sa fixup_handler_t::size
/// \param type  fixup type
/// \retval    operand value

idaman uval_t ida_export get_fixup_value(ea_t ea, fixup_type_t type);


/// Patch the fixup bytes.
/// This function updates data or an instruction at `ea' to the fixup bytes.
/// For example, FIXUP_HI8 updates a byte at `ea' to the high byte of
/// `fd->off', or AArch64's custom fixup BRANCH26 updates low 26 bits of the
/// insn at `ea' to the value of `fd->off' shifted right by 2.
/// \param ea  address where data are changed, the size of the changed data
///            depends on the fixup type.
///            \sa fixup_handler_t::size
/// \param fd  fixup data
/// \retval false  the fixup bytes do not fit (e.g. `fd->off' is greater
///                than 0xFFFFFFC for BRANCH26). The database is changed
///                even in this case.

idaman bool ida_export patch_fixup_value(ea_t ea, const fixup_data_t &fd);


/// Get FIXUP description comment.

idaman const char *ida_export get_fixup_desc(
        qstring *buf,
        ea_t source,
        const fixup_data_t &fd);


/// Calculate size of fixup in bytes (the number of bytes the fixup patches)
/// \retval -1 means error

idaman int ida_export calc_fixup_size(fixup_type_t type);


//--------------------------------------------------------------------------
// inline implementation
inline bool fixup_data_t::is_custom(void) const
{
  return is_fixup_custom(type);
}
inline void fixup_data_t::set(ea_t source) const
{
  set_fixup(source, *this);
}

inline bool fixup_data_t::get(ea_t source)
{
  return get_fixup(this, source);
}

inline const char *fixup_data_t::get_desc(qstring *buf, ea_t source) const
{
  return get_fixup_desc(buf, source, *this);
}

inline int fixup_data_t::calc_size() const
{
  return calc_fixup_size(type);
}

inline uval_t fixup_data_t::get_value(ea_t ea) const
{
  return get_fixup_value(ea, type);
}

inline bool fixup_data_t::patch_value(ea_t ea) const
{
  return patch_fixup_value(ea, *this);
}

inline const fixup_handler_t *fixup_data_t::get_handler() const
{
  return get_fixup_handler(type);
}


/// \name Custom fixups
/// Processor modules and plugins may register custom fixup handlers. File
/// loaders should use find_custom_fixup() function to find the handler
/// created by the processor module. The custom fixup handlers will be
/// unregistered automatically before the database gets closed.
//@{

//--------------------------------------------------------------------------
/// Implements the core behavior of a custom fixup
struct fixup_handler_t
{
  int32 cbsize;       ///< size of this structure
  const char *name;   ///< Format name, must be unique
  uint32 props;       ///< \ref FHF_
/// \defgroup FHF_ Fixup handler properties
/// Used by fixup_handler_t::props
//@{
#define FHF_VERIFY     0x0001 ///< verify that the value fits into WIDTH
                              ///< bits. If this property is not set we
                              ///< just truncate the value.
#define FHF_CODE       0x0002 ///< verify that ITEM_EA in std_apply() points
                              ///< to an instruction.
#define FHF_FORCE_CODE 0x0004 ///< if ITEM_EA in std_apply() points to an
                              ///< unknown item, then convert it to code.
                              ///< this property is valid only with FHF_CODE.
#define FHF_ABS_OPVAL  0x0008 ///< create absolute refinfo in std_apply()
                              ///< because the operand also has the absolute
                              ///< value (usually for o_near operands)
//@}

/// \defgroup fh_options Tuning options
//@{
  uint8 size;         ///< size in bytes
  uint8 width;        ///< number of significant bits before shifting
  uint8 shift;        ///< number of bits to shift right before patching.
                      ///< The following should be true:
                      ///< `width - shift <= size * 8'
  uint8 rsrv4;        // reserved

  uint32 reftype;     ///< reference info type and flags,
                      ///< std_apply() produces an offset of this type
//@}

  /// Apply a fixup: take it into account while analyzing the file.
  /// Usually it consists of converting the operand into an offset expression.
  /// \sa apply_fixup()
  /// If this callback is not specified then std_apply() is used.
  bool (idaapi *apply)(
          const fixup_handler_t *fh,
          ea_t item_ea,
          ea_t fixup_ea,
          int opnum,
          bool is_macro,
          const fixup_data_t &fd);

  /// Get the operand value.
  /// This callback is called from get_fixup_value().
  /// \sa get_fixup_value()
  /// If this callback is not specified then std_get_value() is used.
  uval_t (idaapi *get_value)(const fixup_handler_t *fh, ea_t ea);

  /// Patch the fixup bytes.
  /// This callback is called from patch_fixup_value() or after changing the
  /// fixup (e.g. after it was moved from one location to another).
  /// If this callback is not specified then std_patch_value() is used.
  /// \sa patch_fixup_value()
  /// \retval false  the fixup bytes do not fit. The database is changed
  ///                even in this case.
  bool (idaapi *patch_value)(
          const fixup_handler_t *fh,
          ea_t ea,
          const fixup_data_t &fd);

};


/// \name std_apply()
/// This internal function takes \ref fh_options and \ref FHF_ to convert
/// the fixup to an offset.

/// \name std_patch_value()
/// This internal function takes \ref fh_options and \ref FHF_ to determine
/// how to patch the bytes.
/// 1) it verifies that the fixup value fits to the fixup_handler_t::width
/// bits if the FHF_VERIFY property is specified,
/// 2) it discards bits that do not fit,
/// 3) it shifts the result right by fixup_handler_t::shift bits,
/// 4) it puts the result to the rightmost bits of fixup_handler_t::size
/// bytes at the given address.
/// For example, FIXUP_HI8 uses size = 1, and width = 16, and shift = 8, and
/// property FHF_VERIFY, or AArch64's custom fixup BRANCH26 uses size = 4,
/// and width = 28, and shift = 2, and property FHF_VERIFY.

/// \name std_get_value()
/// This internal function takes \ref fh_options to determine how to get the
/// operand value.
/// It is opposite in meaning to the `std_patch_value()`.
/// 1) it gets the fixup_handler_t::size bytes at the given address,
/// 2) it shifts the result left by fixup_handler_t::shift bits,
/// 3) it returns the rightmost fixup_handler_t::width bits as a signed
/// value.


/// Register a new custom fixup.
/// This function must be called by a processor module or plugin, but not
/// by a file loader. File loaders should use find_custom_fixup() function
/// to find the handler created by the processor module.
/// \return id of the new custom fixup handler with FIXUP_CUSTOM bit set or
///         0 (e.g. when the custom fixup handler with the same name was
///         already registered).

idaman fixup_type_t ida_export register_custom_fixup(
        const fixup_handler_t *cfh);


/// Unregister a new custom fixup format. Should be called by the processor
/// module before the database gets closed.

idaman bool ida_export unregister_custom_fixup(fixup_type_t type);


/// Get id of a custom fixup handler.
/// \param  name  name of the custom fixup handler
/// \return id with FIXUP_CUSTOM bit set or 0

idaman fixup_type_t ida_export find_custom_fixup(const char *name);

//@}


//--------------------------------------------------------------------------
/// Collect fixup records for the specified range.
/// Partially overlapping records will be reported too.
/// \return success (false means no fixup info have been found)

struct fixup_info_t
{
  ea_t ea;
  fixup_data_t fd;
};
DECLARE_TYPE_AS_MOVABLE(fixup_info_t);
typedef qvector<fixup_info_t> fixups_t;

idaman bool ida_export get_fixups(fixups_t *out, ea_t ea, asize_t size);


/// Does the specified address range contain any fixup information?

inline bool contains_fixups(ea_t ea, asize_t size)
{
  return get_fixups(NULL, ea, size);
}


/// Relocate the bytes with fixup information once more (generic function).
/// This function may be called from loader_t::move_segm() if it suits the goal.
/// If loader_t::move_segm is not defined then this function will be called
/// automatically when moving segments or rebasing the entire program.
/// Special parameter values (from = BADADDR, size = 0, to = delta) are used
/// when the function is called from rebase_program(delta).

idaman void ida_export gen_fix_fixups(ea_t from, ea_t to, asize_t size);


#endif // FIXUP_HPP
