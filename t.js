var ab = require('./archive_bindings');
var ar = new ab.ArchiveReader();
var sys = require('sys');
var Buffer = require('buffer').Buffer;

ar.addListener('ready', function() {
  sys.log('In ready function....');
  ar.nextEntry();
});

ar.addListener('entry', function(entry) {
  sys.log('path: '+ entry.getPath());
  sys.log('    size: '+ entry.getSize());
  sys.log('    mtime: '+ entry.getMtime());

  buf = new Buffer(256);
  entry.read(buf, function(length, err) {
    if (!err) {
      if (length === 0) {
        entry.emit('end');
      }
      else {
        var b = buf.slice(0, length);
        entry.emit('data', b);
      }
    }
    else {
      sys.log(err);
      entry.emit('end');
    }
  });

  entry.addListener('data', function(data) { 
    sys.log("got data");
  });

  entry.addListener('end', function() { 
    process.nextTick(function() {
      ar.nextEntry();
    });
  });
  
});

ar.openFile("nofile.tar.gz", function(err){
  if (err) {
    sys.log(err);
  }
});

