var path = require('path');
var root = path.dirname(__filename);
require.paths.unshift(path.join(root, '../build/default'));
require.paths.unshift(path.join(root, '../lib'));

var sys = require('sys');
var Buffer = require('buffer').Buffer;
var archive = require('archive');

var ar = new archive.ArchiveReader();

var buf = new Buffer(1024 * 16);

ar.addListener('ready', function() {
  sys.log('In ready function....');
  ar.nextEntry();
});

ar.addListener('entry', function(entry) {
  sys.log('path: '+ entry.getPath());
  sys.log('    size: '+ entry.getSize());
  sys.log('    mtime: '+ entry.getMtime());

  entry.addListener('data', function(data) { 
    sys.log("got data of length: "+ data.length);
  });

  entry.addListener('end', function() { 
    process.nextTick(function() {
      ar.nextEntry();
    });
  });

  entry.read(buf);
});

ar.openFile(path.join(root, "test.tar.gz"), function(err){
  if (err) {
    sys.log(err);
  }
});


