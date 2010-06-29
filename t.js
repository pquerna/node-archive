var a = require('./archive_bindings');
var b = new a.ArchiveReader();
var sys = require('sys')
b.open_file("nofile.tar.gz", function(err){sys.log('hit it in error handler'); sys.log(err);})

