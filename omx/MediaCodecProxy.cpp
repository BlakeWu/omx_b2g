/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaCodecProxy"

#include <binder/IPCThreadState.h>
#include <cutils/properties.h>
#include <stagefright/foundation/ADebug.h>
#include <stagefright/MetaData.h>
#include <utils/Log.h>

#include "nsDebug.h"

#include "IMediaResourceManagerService.h"

#include "B2GMediaCodec.h"
#include "MediaCodecProxy.h"
#include <android/log.h>
#define ALOG(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace android {


// static
sp<MediaCodecProxy> MediaCodecProxy::Create(const char *mime, bool encoder)
{
  sp<MediaCodecProxy> proxy;

  if (!strncasecmp(mime, "video/", 6)) {
    proxy = new MediaCodecProxy(mime, encoder);
  }
  return proxy;
}


MediaCodecProxy::MediaCodecProxy(
        const char *mime,
        bool encoder)
    : mState(MediaResourceManagerClient::CLIENT_STATE_WAIT_FOR_RESOURCE),
    mMime(strdup(mime))
{
    if (!mCodecLooper) {
        mCodecLooper = new ALooper;
        mCodecLooper->start();
    }
}

MediaCodecProxy::~MediaCodecProxy()
{
    mState = MediaResourceManagerClient::CLIENT_STATE_SHUTDOWN;

    if (mMediaCodec.get()) {
        wp<MediaSource> tmp = mMediaCodec;
        mMediaCodec.clear();
        while (tmp.promote() != nullptr) {
        // this value come from stagefrigh's AwesomePlayer.
            usleep(1000);
        }
    }

    if (mManagerService.get() && mClient.get()) {
       mManagerService->cancelClient(mClient);
    }

    if(!mMime){
       free(mMime);
       mMime = nullptr;	    
    }	        
    
    if(mCodecLooper)
      mCodecLooper->stop();  	    
}

MediaResourceManagerClient::State MediaCodecProxy::getState()
{
  Mutex::Autolock autoLock(mLock);
  return mState;
}

void MediaCodecProxy::setEventListener(const wp<MediaCodecProxy::EventListener>& listener)
{
  Mutex::Autolock autoLock(mLock);
  mEventListener = listener;
}

void MediaCodecProxy::notifyStatusChangedLocked()
{
  if (mEventListener != nullptr) {
    sp<EventListener> listener = mEventListener.promote();
    if (listener != nullptr) {
      listener->statusChanged();
    }
  }
}

void MediaCodecProxy::requestResource()
{
  Mutex::Autolock autoLock(mLock);

  if (mClient.get()) {
    return;
  }
  sp<MediaResourceManagerClient::EventListener> listener = this;
  mClient = new MediaResourceManagerClient(listener);

  mManagerService = mClient->getMediaResourceManagerService();
  if (!mManagerService.get()) {
    mClient = nullptr;
    return;
  }

  mManagerService->requestMediaResource(mClient, MediaResourceManagerClient::HW_VIDEO_DECODER);
}

bool MediaCodecProxy::IsWaitingResources()
{
  Mutex::Autolock autoLock(mLock);
  return mState == MediaResourceManagerClient::CLIENT_STATE_WAIT_FOR_RESOURCE;
}

// called on Binder ipc thread
void MediaCodecProxy::statusChanged(int event)
{
  Mutex::Autolock autoLock(mLock);

  if (mState != MediaResourceManagerClient::CLIENT_STATE_WAIT_FOR_RESOURCE) {
    return;
  }

  mState = (MediaResourceManagerClient::State) event;
  if (mState != MediaResourceManagerClient::CLIENT_STATE_RESOURCE_ASSIGNED) {
    return;
  }

  if (!strncasecmp(mMime, "video/", 6)) {
    mMediaCodec = B2GMediaCodec::CreateByType(
                mCodecLooper, mMime, false /* encoder */);

    if (mMediaCodec == nullptr) {
      mState = MediaResourceManagerClient::CLIENT_STATE_SHUTDOWN;
      notifyStatusChangedLocked();
      return;
    }

    if (mMediaCodec->start() != OK) {
      NS_WARNING("Couldn't start OMX video source");
      mMediaCodec.clear();
      mState = MediaResourceManagerClient::CLIENT_STATE_SHUTDOWN;
      notifyStatusChangedLocked();
      return;
    }
  }
  notifyStatusChangedLocked();
}

status_t MediaCodecProxy::start()
{
  Mutex::Autolock autoLock(mLock);

  if (mState != MediaResourceManagerClient::CLIENT_STATE_RESOURCE_ASSIGNED) {
    return NO_INIT;
  }
  CHECK(mMediaCodec.get() != nullptr);
  status_t result = mMediaCodec->start();
  if (result == OK) {
    mCodec->getInputBuffers(&mInputBuffers);
    mCodec->getOutputBuffers(&mOutputBuffers);
  }
  else 
      return result; 

  return OK;
}

status_t MediaCodecProxy::stop()
{
  Mutex::Autolock autoLock(mLock);

  if (mState != MediaResourceManagerClient::CLIENT_STATE_RESOURCE_ASSIGNED) {
    return NO_INIT;
  }
  CHECK(mMediaCodec.get() != nullptr);
  return mMediaCodec->stop();
}


status_t MediaCodecProxy::Input(const uint8_t* aData, uint32_t aDataSize, int64_t aTimestampUsecs,uint64_t flags) {
     
    size_t index;
    status_t err = state->mCodec->dequeueInputBuffer(&index, -1ll);
    CHECK_EQ(err, (status_t)OK);
     
    const sp<ABuffer> &dstBuffer = mInputBuffers.itemAt(index);

    CHECK_LE(aDataSize, dstBuffer->capacity());
    dstBuffer->setRange(0, aDataSize);
    memcpy(dstBuffer->data(), aData, aDataSize);

    err = state->mCodec->queueInputBuffer(
                    index,
                    0,
                    dstBuffer->size(),
                    0ll,
                    flags);

     CHECK_EQ(err, (status_t)OK);

}

status_t MediaCodecProxy::Output(ABuffer** outBuffer,uint64_t flags,int64_t timeoutUs) {
     
    size_t index;
    status_t err = state->mCodec->dequeueInputBuffer(&index, -1ll);
    CHECK_EQ(err, (status_t)OK);
     
    const sp<ABuffer> &dstBuffer = mInputBuffers.itemAt(index);

    CHECK_LE(aDataSize, dstBuffer->capacity());
    dstBuffer->setRange(0, aDataSize);
    memcpy(dstBuffer->data(), aData, aDataSize);

    err = state->mCodec->queueInputBuffer(
                    index,
                    0,
                    dstBuffer->size(),
                    0ll,
                    flags, timeoutUs);

    CHECK_EQ(err, (status_t)OK);

    BufferInfo info;
    err = state->mCodec->dequeueOutputBuffer(
                    &info.mIndex,
                    &info.mOffset,
                    &info.mSize,
                    &info.mPresentationTimeUs,
                    &info.mFlags);

    if (err == OK) {

      outBuffer = mOutputBuffers[info.mIndex];

    } else {
      ALOG("Output returned %d", err);
    }

}

	
}  // namespace android
