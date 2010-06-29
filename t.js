var ab = require('./archive_bindings');
var ar = new ab.ArchiveReader();
var sys = require('sys');

ar.addListener('ready', function() {
  sys.log('In ready function....');
  ar.next();
});

ar.addListener('entry', function(entry) {
  sys.log('path: '+ entry.getPath());
  sys.log('    size: '+ entry.getSize());
  sys.log('    mtime: '+ entry.getMtime());
/*
  estream = entry.createStream()
  estream.addListener('data', function(data) { });

  estream.addListener('end', function() { });
*/
  /* TODO: is the right api? */
  ar.next();
});

ar.openFile("nofile.tar.gz", function(err){
  if (err) {
    sys.log(err);
  }
});

