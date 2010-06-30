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

#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>

#include <string>
#include <archive.h>
#include <archive_entry.h>

#include "archive_helpers.h"

using namespace v8;
using namespace node;

class ArchiveEntry : public EventEmitter
{
public:

  static Persistent<FunctionTemplate> s_ct;
  static void Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(EventEmitter::constructor_template);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("ArchiveEntry"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "read", Read);

    /* Attributes */
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getPath", GetPath);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getSize", GetSize);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "getMtime", GetMtime);

    target->Set(String::NewSymbol("ArchiveEntry"),
                s_ct->GetFunction());
  }

  struct archive *m_arc;
  struct archive_entry *m_entry;

  ArchiveEntry(struct archive *arc, struct archive_entry *entry) :
    EventEmitter(),
    m_arc(arc),
    m_entry(entry)
  {
  }
  
  ~ArchiveEntry()
  {
  }

  static Handle<Value> New(const Arguments& args)
  {
    HandleScope scope;
    REQ_EXT_ARG(0, archive);
    REQ_EXT_ARG(1, entry);

    ArchiveEntry* ae = new ArchiveEntry(static_cast<struct archive *>(archive->Value()),
                                        static_cast<struct archive_entry *>(entry->Value()));

    ae->Wrap(args.This());
    return args.This();
  }

  static Handle<Value> GetPath(const Arguments& args)
  {
    HandleScope scope;
    ArchiveEntry* ar = ObjectWrap::Unwrap<ArchiveEntry>(args.This());

    Local<String> result = String::New(archive_entry_pathname(ar->m_entry));
    return scope.Close(result);
  }


  static Handle<Value> GetSize(const Arguments& args)
  {
    HandleScope scope;
    ArchiveEntry* ar = ObjectWrap::Unwrap<ArchiveEntry>(args.This());

    Local<Number> result = Integer::New(archive_entry_size(ar->m_entry));
    return scope.Close(result);
  }

  static Handle<Value> GetMtime(const Arguments& args)
  {
    HandleScope scope;
    ArchiveEntry* ar = ObjectWrap::Unwrap<ArchiveEntry>(args.This());

    Local<Number> result = Integer::New(archive_entry_mtime(ar->m_entry));
    return scope.Close(result);
  }

  typedef struct read_baton_t {
    int rv;
    std::string errstr;
    ArchiveEntry *entry;
    Buffer* buffer;
    Persistent<Function> cb;
  } read_baton_t;

  static int EIO_Read(eio_req *req)
  {
    read_baton_t *baton = static_cast<read_baton_t *>(req->data);

    baton->rv = archive_read_data(baton->entry->m_arc, baton->buffer->data(), baton->buffer->length());
    if (baton->rv < 0 && baton->rv != ARCHIVE_OK) {
      baton->errstr = archive_error_string(baton->entry->m_arc);
    }
    return 0;
  }

  static int EIO_AfterRead(eio_req *req)
  {
    HandleScope scope;
    ev_unref(EV_DEFAULT_UC);
    read_baton_t *baton = static_cast<read_baton_t *>(req->data);

    Local<Value> argv[2];
    bool err = false;

    argv[0] = Integer::New(baton->rv);
    if (baton->rv < 0 && baton->rv != ARCHIVE_OK) {
      err = true;
      argv[1] = Exception::Error(String::New(baton->errstr.c_str()));
    }

    TryCatch try_catch;

    baton->entry->Unref();
    baton->cb->Call(Context::GetCurrent()->Global(), err ? 2 : 1, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }

    baton->cb.Dispose();

    delete baton;
    return 0;
  }

  static Handle<Value> Read(const Arguments& args)
  {
    HandleScope scope;

    if (!Buffer::HasInstance(args[0])) {
      return ThrowException(Exception::Error(
                  String::New("First argument needs to be a buffer")));
    }

    REQ_FUN_ARG(1, cb);

    ArchiveEntry* entry = ObjectWrap::Unwrap<ArchiveEntry>(args.This());
    Buffer* buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

    read_baton_t *baton = new read_baton_t();
    baton->rv = 0;
    baton->cb = Persistent<Function>::New(cb);
    baton->buffer = buffer;
    baton->entry = entry;

    eio_custom(EIO_Read, EIO_PRI_DEFAULT, EIO_AfterRead, baton);

    ev_ref(EV_DEFAULT_UC);
    entry->Ref();

    return Undefined();
  }

};

/* TODO: share baseclass in Archive{Reader,Writer} */
class ArchiveReader : public EventEmitter
{
public:

  static Persistent<FunctionTemplate> s_ct;
  static void Init(Handle<Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    s_ct = Persistent<FunctionTemplate>::New(t);
    s_ct->Inherit(EventEmitter::constructor_template);
    s_ct->InstanceTemplate()->SetInternalFieldCount(1);
    s_ct->SetClassName(String::NewSymbol("ArchiveReader"));

    NODE_SET_PROTOTYPE_METHOD(s_ct, "openFile", OpenFile);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "nextEntry", NextEntry);
    NODE_SET_PROTOTYPE_METHOD(s_ct, "close", Close);

    target->Set(String::NewSymbol("ArchiveReader"),
                s_ct->GetFunction());
  }

protected:
  struct archive *m_arc;

  ArchiveReader() : EventEmitter(), m_arc(NULL)
  {
  }
  
  ~ArchiveReader()
  {
    if (m_arc != NULL) {
      archive_read_finish(m_arc);
    }
  }

  static Handle<Value> New(const Arguments& args)
  {
    HandleScope scope;
    ArchiveReader* ar = new ArchiveReader();
    ar->Wrap(args.This());
    return args.This();
  }

  typedef struct openfile_baton_t {
    int rv;
    std::string errstr;
    std::string path;
    /* TODO: refcount this? */
    ArchiveReader *ar;
    Persistent<Function> cb;
  } openfile_baton_t;

  static int EIO_OpenFile(eio_req *req)
  {
    openfile_baton_t *baton = static_cast<openfile_baton_t *>(req->data);

    archive_read_support_compression_all(baton->ar->m_arc);
    archive_read_support_format_all(baton->ar->m_arc);

    baton->rv = archive_read_open_filename(baton->ar->m_arc,
                                           baton->path.c_str(),
                                           10240);
    if (baton->rv != ARCHIVE_OK) {
      baton->errstr = archive_error_string(baton->ar->m_arc);
    }

    return 0;
  }

  static int EIO_AfterOpenFile(eio_req *req)
  {
    HandleScope scope;
    ev_unref(EV_DEFAULT_UC);
    openfile_baton_t *baton = static_cast<openfile_baton_t *>(req->data);

    Local<Value> argv[1];
    bool err = false;

    if (baton->rv != ARCHIVE_OK) {
      err = true;
      argv[0] = Exception::Error(String::New(baton->errstr.c_str()));
    }

    TryCatch try_catch;

    baton->ar->Unref();
    baton->cb->Call(Context::GetCurrent()->Global(), err ? 1 : 0, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }

    if (!err) {
      baton->ar->Emit(String::New("ready"), 0, NULL);
      if (try_catch.HasCaught()) {
        FatalException(try_catch);
      }
    }
    baton->cb.Dispose();

    delete baton;
    return 0;
  }

  static Handle<Value> OpenFile(const Arguments& args)
  {
    HandleScope scope;

    REQ_STR_ARG(0, filename);
    REQ_FUN_ARG(1, cb);
    
    ArchiveReader* ar = ObjectWrap::Unwrap<ArchiveReader>(args.This());
    if (ar->m_arc == NULL) {
      ar->m_arc = archive_read_new();
    }

    openfile_baton_t *baton = new openfile_baton_t();
    baton->rv = 0;
    baton->path = *filename;
    baton->cb = Persistent<Function>::New(cb);
    baton->ar = ar;

    eio_custom(EIO_OpenFile, EIO_PRI_DEFAULT, EIO_AfterOpenFile, baton);

    ev_ref(EV_DEFAULT_UC);
    ar->Ref();

    return Undefined();
  }

  static Handle<Value> Close(const Arguments& args)
  {
    HandleScope scope;
    
    ArchiveReader* ar = ObjectWrap::Unwrap<ArchiveReader>(args.This());

    if (ar->m_arc != NULL) {
      archive_read_finish(ar->m_arc);
      ar->m_arc = NULL;
    }

    return Undefined();
  }

  typedef struct entry_baton_t {
    int rv;
    std::string errstr;
    ArchiveReader *ar;
    struct archive_entry *entry;
  } entry_baton_t;

  static int EIO_NextEntry(eio_req *req)
  {
    entry_baton_t *baton = static_cast<entry_baton_t *>(req->data);

    baton->rv = archive_read_next_header(baton->ar->m_arc, &baton->entry);

    if (baton->rv != ARCHIVE_OK) {
      baton->errstr = archive_error_string(baton->ar->m_arc);
    }

    return 0;
  }

  static int EIO_AfterNextEntry(eio_req *req)
  {
    HandleScope scope;
    ev_unref(EV_DEFAULT_UC);
    entry_baton_t *baton = static_cast<entry_baton_t *>(req->data);

    Local<Value> argv[1];
    bool err = false;

    if (baton->rv != ARCHIVE_OK) {
      err = true;
      argv[0] = Exception::Error(String::New(baton->errstr.c_str()));
    }
    else {
      Local<Value> aeargv[2];
      aeargv[0] = External::New(baton->ar->m_arc);
      aeargv[1] = External::New(baton->entry);
      Persistent<Object> ae(ArchiveEntry::s_ct->GetFunction()->NewInstance(2, aeargv));
      argv[0] = Local<Value>::New(ae);
    }

    baton->ar->Unref();

    TryCatch try_catch;

    if (!err) {
      baton->ar->Emit(String::New("entry"), 1, argv);
    }
    else {
      baton->ar->Emit(String::New("error"), 1, argv);
    }

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }

    delete baton;
    return 0;
  }

  static Handle<Value> NextEntry(const Arguments& args)
  {
    HandleScope scope;

    ArchiveReader* ar = ObjectWrap::Unwrap<ArchiveReader>(args.This());

    entry_baton_t *baton = new entry_baton_t();
    baton->rv = 0;
    baton->ar = ar;
    baton->entry = NULL;

    eio_custom(EIO_NextEntry, EIO_PRI_DEFAULT, EIO_AfterNextEntry, baton);

    ev_ref(EV_DEFAULT_UC);
    ar->Ref();

    return Undefined();
  }
};

Persistent<FunctionTemplate> ArchiveEntry::s_ct;
Persistent<FunctionTemplate> ArchiveReader::s_ct;

extern "C" {
  void init (Handle<Object> target)
  {
    ArchiveReader::Init(target);
    ArchiveEntry::Init(target);
  }
}
