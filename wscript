

def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")
  if not conf.check_cfg(package='libarchive', args='--cflags --libs', uselib_store='ARCHIVE'):
    if not conf.check(lib="archive", uselib_store="ARCHIVE"):
      conf.fatal('Missing libarchive, download it from http://code.google.com/p/libarchive/');

def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.cxxflags = ["-g", "-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-Wall"]
  obj.target = "archive_bindings"
  obj.source = "src/archive_bindings.cc"
  obj.uselib = "ARCHIVE"