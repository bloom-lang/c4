require 'rubygems'
require 'ffi'

# TODO:
# * Invoke c4_initialize() once at startup-time, and c4_terminate()
#   once at shutdown
# * Arrange to invoke c4_destroy() when C4 object is GC'd
class C4
  module C4Lib
    extend FFI::Library
    ffi_lib 'c4'
    attach_function 'c4_initialize', [], :void
    attach_function 'c4_make', [:pointer, :int], :pointer
    attach_function 'c4_install_file', [:pointer, :string], :int
    attach_function 'c4_install_str', [:pointer, :string], :int
    attach_function 'c4_dump_table', [:pointer, :string], :string
    attach_function 'c4_destroy', [:pointer], :void
    attach_function 'c4_terminate', [], :void
  end

  def initialize(port=0)
    C4Lib.c4_initialize
    @c4 = C4Lib.c4_make(nil, port)
  end

  # TODO: check status
  def install_prog(inprog)
    s = C4Lib.c4_install_file(@c4, inprog)
  end

  # TODO: check status
  def install_str(inprog)
    s = C4Lib.c4_install_str(@c4, inprog)
  end

  def dump_table(tbl_name)
    C4Lib.c4_dump_table(@c4, tbl_name)
  end

  def destroy
    C4Lib.c4_destroy(@c4)
    C4Lib.c4_terminate
  end
end
