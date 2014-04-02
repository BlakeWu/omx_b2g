/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MEDIA_CODEC_PROXY_DECODER_H_
#define MEDIA_CODEC_PROXY_DECODER_H_


#include <android/native_window.h>
#include <IOMX.h>
#include <stagefright/MediaBuffer.h>
#include <stagefright/MediaSource.h>
#include <utils/threads.h>

#include "MediaResourceManagerClient.h"
#include "B2GMediaCodec.h"

namespace android {

struct MediaBufferGroup;
struct MetaData;

class MediaCodecProxy : public MediaResourceManagerClient::EventListener
{
public:
    struct EventListener : public virtual RefBase {
      virtual void statusChanged() = 0;
    };

    static sp<MediaCodecProxy> Create(const char *mime, bool encoder);
	

    MediaResourceManagerClient::State getState();

    void setEventListener(const wp<EventListener>& listener);

    void requestResource();
    bool IsWaitingResources();

    // MediaResourceManagerClient::EventListener
    virtual void statusChanged(int event);

    virtual status_t pause();
    
    status_t Input(const uint8_t* aData,
                uint32_t aDataSize,
                int64_t aTimestampUsecs);

    status_t start();

protected:
    MediaCodecProxy(const char *mime, bool encoder);

    virtual ~MediaCodecProxy();

    void notifyStatusChangedLocked();

private:
    MediaCodecProxy(const MediaCodecProxy &);
    MediaCodecProxy &operator=(const MediaCodecProxy &);

    Mutex mLock;

    sp<IOMX> mOMX;
    char *mMime;
    bool mIsEncoder;
    // Flags specified in the creation of the codec.
    uint32_t mFlags;
    sp<ANativeWindow> mNativeWindow;

    sp<ALooper> mCodecLooper;

    sp<B2GMediaCodec> mMediaCodec;
    sp<MediaResourceManagerClient> mClient;
    MediaResourceManagerClient::State mState;

    sp<IMediaResourceManagerService> mManagerService;
    wp<MediaCodecProxy::EventListener> mEventListener;

    
    Vector<sp<B2GMediaCodec::BufferInfo> > mInputBuffers;  // B2GMediaCodec buffers to hold input data.
    Vector<sp<B2GMediaCodec::BufferInfo> > mOutputBuffers; // B2GMediaCodec buffers to hold output data.
};

}  // namespace android

#endif  // MEDIA_CODEC_PROXY_DECODER_H_
