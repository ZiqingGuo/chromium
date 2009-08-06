// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(WORKERS)

#include "base/compiler_specific.h"

#include "DedicatedWorkerThread.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "GenericWorkerTask.h"
#include "MessagePort.h"
#include "MessagePortChannel.h"
#include "ScriptExecutionContext.h"
#include "WorkerContextExecutionProxy.h"
#include "WorkerMessagingProxy.h"
#include "Worker.h"
#include "WorkerContext.h"
#include <wtf/Threading.h>

#undef LOG

#include "webkit/glue/webworkerclient_impl.h"

#include "base/command_line.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebKitClient.h"
#include "webkit/api/public/WebMessagePortChannel.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebWorker.h"
#include "webkit/api/src/PlatformMessagePortChannel.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframeloaderclient_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webview_impl.h"
#include "webkit/glue/webworker_impl.h"

using WebKit::WebMessagePortChannel;
using WebKit::WebString;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

// When WebKit creates a WorkerContextProxy object, we check if we're in the
// renderer or worker process.  If the latter, then we just use
// WebCore::WorkerMessagingProxy.
//
// If we're in the renderer process, then we need use the glue provided
// WebWorker object to talk to the worker process over IPC.  The worker process
// talks to WebCore::Worker* using WorkerObjectProxy, which we implement on
// WebWorkerClientImpl.
//
// Note that if we're running each worker in a separate process, then nested
// workers end up using the same codepath as the renderer process.
WebCore::WorkerContextProxy* WebCore::WorkerContextProxy::create(
    WebCore::Worker* worker) {
  if (!worker->scriptExecutionContext()->isDocument() &&
      CommandLine::ForCurrentProcess()->HasSwitch(
            L"web-worker-share-processes")) {
    return new WebCore::WorkerMessagingProxy(worker);
  }

  WebWorker* webworker = NULL;
  WebWorkerClientImpl* proxy = new WebWorkerClientImpl(worker);

  if (worker->scriptExecutionContext()->isDocument()) {
    // Get to the RenderView, so that we can tell the browser to create a
    // worker process if necessary.
    WebCore::Document* document = static_cast<WebCore::Document*>(
        worker->scriptExecutionContext());
    WebFrameLoaderClient* frame_loader_client =
        static_cast<WebFrameLoaderClient*>(
            document->frame()->loader()->client());
    WebViewDelegate* webview_delegate =
        frame_loader_client->webframe()->GetWebViewImpl()->delegate();
    webworker = webview_delegate->CreateWebWorker(proxy);
  } else {
    WebCore::WorkerContextExecutionProxy* current_context =
        WebCore::WorkerContextExecutionProxy::retrieve();
    if (!current_context) {
      NOTREACHED();
      return NULL;
    }

    WebCore::DedicatedWorkerThread* thread =
        static_cast<WebCore::DedicatedWorkerThread*>(
            current_context->workerContext()->thread());
    WebCore::WorkerObjectProxy* worker_object_proxy =
        &thread->workerObjectProxy();
    WebWorkerImpl* impl = reinterpret_cast<WebWorkerImpl*>(worker_object_proxy);
    webworker = impl->client()->createWorker(proxy);
  }

  proxy->set_webworker(webworker);
  return proxy;
}


WebWorkerClientImpl::WebWorkerClientImpl(WebCore::Worker* worker)
    : script_execution_context_(worker->scriptExecutionContext()),
      worker_(worker),
      asked_to_terminate_(false),
      unconfirmed_message_count_(0),
      worker_context_had_pending_activity_(false),
      worker_thread_id_(WTF::currentThread()) {
}

WebWorkerClientImpl::~WebWorkerClientImpl() {
}

void WebWorkerClientImpl::set_webworker(WebWorker* webworker) {
  webworker_ = webworker;
}

void WebWorkerClientImpl::startWorkerContext(
    const WebCore::KURL& script_url,
    const WebCore::String& user_agent,
    const WebCore::String& source_code) {
  // Worker.terminate() could be called from JS before the context is started.
  if (asked_to_terminate_)
    return;

  if (!WTF::isMainThread()) {
    WebWorkerImpl::DispatchTaskToMainThread(
        WebCore::createCallbackTask(&StartWorkerContextTask, this,
            script_url.string(), user_agent, source_code));
    return;
  }

  webworker_->startWorkerContext(
      webkit_glue::KURLToWebURL(script_url),
      webkit_glue::StringToWebString(user_agent),
      webkit_glue::StringToWebString(source_code));
}

void WebWorkerClientImpl::terminateWorkerContext() {
  if (asked_to_terminate_)
    return;

  asked_to_terminate_ = true;

  if (!WTF::isMainThread()) {
    WebWorkerImpl::DispatchTaskToMainThread(
        WebCore::createCallbackTask(&TerminateWorkerContextTask, this));
    return;
  }

  webworker_->terminateWorkerContext();
}

void WebWorkerClientImpl::postMessageToWorkerContext(
    const WebCore::String& message,
    WTF::PassOwnPtr<WebCore::MessagePortChannel> channel) {
  // Worker.terminate() could be called from JS before the context is started.
  if (asked_to_terminate_)
    return;

  ++unconfirmed_message_count_;

  if (!WTF::isMainThread()) {
    WebWorkerImpl::DispatchTaskToMainThread(
        WebCore::createCallbackTask(
            &PostMessageToWorkerContextTask, this, message, channel));
    return;
  }

  WebMessagePortChannel* webchannel = NULL;
  if (channel.get()) {
    webchannel = channel->channel()->webChannelRelease();
    webchannel->setClient(0);
  }

  webworker_->postMessageToWorkerContext(
      webkit_glue::StringToWebString(message), webchannel);
}

bool WebWorkerClientImpl::hasPendingActivity() const {
  return !asked_to_terminate_ &&
      (unconfirmed_message_count_ || worker_context_had_pending_activity_);
}

void WebWorkerClientImpl::workerObjectDestroyed() {
  if (WTF::isMainThread()) {
    webworker_->workerObjectDestroyed();
    worker_ = NULL;
  }

  // Even if this is called on the main thread, there could be a queued task for
  // this object, so don't delete it right away.
  WebWorkerImpl::DispatchTaskToMainThread(
      WebCore::createCallbackTask(&WorkerObjectDestroyedTask, this));
}

void WebWorkerClientImpl::postMessageToWorkerObject(
    const WebString& message,
    WebMessagePortChannel* channel) {
  WebCore::String message2 = webkit_glue::WebStringToString(message);
  OwnPtr<WebCore::MessagePortChannel> channel2;
  if (channel) {
    RefPtr<WebCore::PlatformMessagePortChannel> platform_channel =
        WebCore::PlatformMessagePortChannel::create(channel);
    channel->setClient(platform_channel.get());
    channel2 = WebCore::MessagePortChannel::create(platform_channel);
  }

  if (WTF::currentThread() != worker_thread_id_) {
    script_execution_context_->postTask(
        WebCore::createCallbackTask(&PostMessageToWorkerObjectTask, this,
            message2, channel2.release()));
    return;
  }

  PostMessageToWorkerObjectTask(
      script_execution_context_.get(), this, message2, channel2.release());
}

void WebWorkerClientImpl::postExceptionToWorkerObject(
    const WebString& error_message,
    int line_number,
    const WebString& source_url) {
  if (WTF::currentThread() != worker_thread_id_) {
    script_execution_context_->postTask(
        WebCore::createCallbackTask(&PostExceptionToWorkerObjectTask, this,
            webkit_glue::WebStringToString(error_message),
            line_number,
            webkit_glue::WebStringToString(source_url)));
    return;
  }

  bool handled = false;
  if (worker_->onerror())
    handled = worker_->dispatchScriptErrorEvent(
        webkit_glue::WebStringToString(error_message),
        webkit_glue::WebStringToString(source_url),
        line_number);
  if (!handled)
    script_execution_context_->reportException(
        webkit_glue::WebStringToString(error_message),
        line_number,
        webkit_glue::WebStringToString(source_url));
}

void WebWorkerClientImpl::postConsoleMessageToWorkerObject(
    int destination_id,
    int source_id,
    int message_type,
    int message_level,
    const WebString& message,
    int line_number,
    const WebString& source_url) {
  if (WTF::currentThread() != worker_thread_id_) {
    script_execution_context_->postTask(
        WebCore::createCallbackTask(&PostConsoleMessageToWorkerObjectTask, this,
            destination_id, source_id, message_type, message_level,
            webkit_glue::WebStringToString(message),
            line_number,
            webkit_glue::WebStringToString(source_url)));
    return;
  }

  script_execution_context_->addMessage(
      static_cast<WebCore::MessageDestination>(destination_id),
      static_cast<WebCore::MessageSource>(source_id),
      static_cast<WebCore::MessageType>(message_type),
      static_cast<WebCore::MessageLevel>(message_level),
      webkit_glue::WebStringToString(message),
      line_number,
      webkit_glue::WebStringToString(source_url));
}

void WebWorkerClientImpl::confirmMessageFromWorkerObject(
    bool has_pending_activity) {
  // unconfirmed_message_count_ can only be updated on the thread where it's
  // accessed.  Otherwise there are race conditions with v8's garbage
  // collection.
  script_execution_context_->postTask(
      WebCore::createCallbackTask(&ConfirmMessageFromWorkerObjectTask, this));
}

void WebWorkerClientImpl::reportPendingActivity(bool has_pending_activity) {
  // See above comment in confirmMessageFromWorkerObject.
  script_execution_context_->postTask(
      WebCore::createCallbackTask(&ReportPendingActivityTask, this,
          has_pending_activity));
}

void WebWorkerClientImpl::workerContextDestroyed() {
}

void WebWorkerClientImpl::StartWorkerContextTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr,
    const WebCore::String& script_url,
    const WebCore::String& user_agent,
    const WebCore::String& source_code) {
  this_ptr->webworker_->startWorkerContext(
      webkit_glue::KURLToWebURL(WebCore::KURL(script_url)),
      webkit_glue::StringToWebString(user_agent),
      webkit_glue::StringToWebString(source_code));
}

void WebWorkerClientImpl::TerminateWorkerContextTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr) {
  this_ptr->webworker_->terminateWorkerContext();
}

void WebWorkerClientImpl::PostMessageToWorkerContextTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr,
    const WebCore::String& message,
    WTF::PassOwnPtr<WebCore::MessagePortChannel> channel) {
  WebMessagePortChannel* webChannel = NULL;
  if (channel.get()) {
    webChannel = channel->channel()->webChannelRelease();
    webChannel->setClient(0);
  }

  this_ptr->webworker_->postMessageToWorkerContext(
      webkit_glue::StringToWebString(message), webChannel);
}

void WebWorkerClientImpl::WorkerObjectDestroyedTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr) {
  if (this_ptr->worker_)  // Check we haven't alread called this.
    this_ptr->webworker_->workerObjectDestroyed();
  delete this_ptr;
}

void WebWorkerClientImpl::PostMessageToWorkerObjectTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr,
    const WebCore::String& message,
    WTF::PassOwnPtr<WebCore::MessagePortChannel> channel) {

  if (this_ptr->worker_) {
    WTF::RefPtr<WebCore::MessagePort> port;
    if (channel) {
      port = WebCore::MessagePort::create(*context);
      port->entangle(channel.release());
    }

    this_ptr->worker_->dispatchMessage(message, port.release());
  }
}

void WebWorkerClientImpl::PostExceptionToWorkerObjectTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr,
    const WebCore::String& error_message,
    int line_number,
    const WebCore::String& source_url) {
  bool handled = false;
  if (this_ptr->worker_ && this_ptr->worker_->onerror())
    handled = this_ptr->worker_->dispatchScriptErrorEvent(
        error_message, source_url, line_number);
  if (!handled)
    this_ptr->script_execution_context_->reportException(
        error_message, line_number, source_url);
}

void WebWorkerClientImpl::PostConsoleMessageToWorkerObjectTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr,
    int destination_id,
    int source_id,
    int message_type,
    int message_level,
    const WebCore::String& message,
    int line_number,
    const WebCore::String& source_url) {
  this_ptr->script_execution_context_->addMessage(
      static_cast<WebCore::MessageDestination>(destination_id),
      static_cast<WebCore::MessageSource>(source_id),
      static_cast<WebCore::MessageType>(message_type),
      static_cast<WebCore::MessageLevel>(message_level),
      message,
      line_number,
      source_url);
}

void WebWorkerClientImpl::ConfirmMessageFromWorkerObjectTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr) {
  this_ptr->unconfirmed_message_count_--;
}

void WebWorkerClientImpl::ReportPendingActivityTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerClientImpl* this_ptr,
    bool has_pending_activity) {
  this_ptr->worker_context_had_pending_activity_ = has_pending_activity;
}

#endif
