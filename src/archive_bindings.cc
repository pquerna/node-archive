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

#include <string>
#include <archive.h>
#include <archive_entry.h>

#include "archive_helpers.h"

using namespace v8;
using namespace node;

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

    NODE_SET_PROTOTYPE_METHOD(s_ct, "open_file", OpenFile);

    target->Set(String::NewSymbol("ArchiveReader"),
                s_ct->GetFunction());
  }

protected:
  struct archive *m_arc;

  ArchiveReader() : EventEmitter()
  {
    m_arc = archive_read_new();

    /* TODO: these can dlopen shit. should be doing this in an async thread? */
    archive_read_support_compression_all(m_arc);
    archive_read_support_format_all(m_arc);
  }
  
  ~ArchiveReader()
  {
    archive_read_finish(m_arc);
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

    baton->ar->Emit(String::New("ready"), 0, NULL);
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
};

Persistent<FunctionTemplate> ArchiveReader::s_ct;

extern "C" {
  void init (Handle<Object> target)
  {
    ArchiveReader::Init(target);
  }
}
