var ab = require('./archive_bindings');
var ar = new ab.ArchiveReader();
var sys = require('sys');

tf.addListener('ready', function() {
  sys.log('In ready function....');
  tf.next();
});

tf.addListener('entry', function(entry) {
  sys.log('path: '+ entry.getPath());
  sys.log('    size: '+ entry.getSize());
  sys.log('    mtime: '+ entry.getMtime());
/*
  estream = entry.createStream()
  estream.addListener('data', function(data) { });

  estream.addListener('end', function() { });
*/
  /* TODO: is the right api? */
  tf.next();
});

b.openFile("nofile.tar.gz", function(err){
  if (err) {
    sys.log(err);
  }
});

