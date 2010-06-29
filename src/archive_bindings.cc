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

    NODE_SET_PROTOTYPE_METHOD(constructor_template, "open_file", OpenFile);

    target->Set(String::NewSymbol("ArchiveReader"),
                constructor_template->GetFunction());
  }

protected:
  ArchiveReader() : EventEmitter()
  {
  }

  static Handle<Value> New(const Arguments& args)
  {
    HandleScope scope;
    ArchiveReader* ar = new ArchiveReader();
    ar->Wrap(args.This());
    return args.This();
  }

  /* TODO: EIO stuff */
  static Handle<Value> OpenFile(const Arguments& args) {
    HandleScope scope;
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
