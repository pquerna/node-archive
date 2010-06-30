/*
 * Licensed to Cloudkick, Inc ('Cloudkick') under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * Cloudkick licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var bindings = require('archive_bindings');
var sys = require('sys');

bindings.ArchiveEntry.prototype.read = function(buf)
{
  var entry = this;
  function entry_reader(length, err) {
    if (!err) {
      if (length === 0) {
        entry.emit('end');
      }
      else {
        var b = buf.slice(0, length);
        entry.emit('data', b);
        entry.readAll(buf, entry_reader);
      }
    }
    else {
      entry.emit('end', err);
    }
  }
  entry.readAll(buf, entry_reader);
};

exports.ArchiveReader = bindings.ArchiveReader;
