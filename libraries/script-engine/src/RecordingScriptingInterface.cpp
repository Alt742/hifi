//
//  Created by Bradley Austin Davis on 2015/11/13
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RecordingScriptingInterface.h"

#include <QThread>

#include <recording/Deck.h>
#include <recording/Recorder.h>
#include <recording/Clip.h>
#include <recording/Frame.h>
#include <NumericalConstants.h>
// FiXME
//#include <AudioClient.h>
#include <AudioConstants.h>
#include <Transform.h>

#include "ScriptEngineLogging.h"

typedef int16_t AudioSample;


using namespace recording;

// FIXME move to somewhere audio related?
static const QString AUDIO_FRAME_NAME = "com.highfidelity.recording.Audio";

RecordingScriptingInterface::RecordingScriptingInterface() {
    static const recording::FrameType AVATAR_FRAME_TYPE = recording::Frame::registerFrameType(AvatarData::FRAME_NAME);
    // FIXME how to deal with driving multiple avatars locally?  
    Frame::registerFrameHandler(AVATAR_FRAME_TYPE, [this](Frame::ConstPointer frame) {
        processAvatarFrame(frame);
    });

    static const recording::FrameType AUDIO_FRAME_TYPE = recording::Frame::registerFrameType(AUDIO_FRAME_NAME);
    Frame::registerFrameHandler(AUDIO_FRAME_TYPE, [this](Frame::ConstPointer frame) {
        processAudioFrame(frame);
    });

    _player = DependencyManager::get<Deck>();
    _recorder = DependencyManager::get<Recorder>();

    // FIXME : Disabling Sound
//    auto audioClient = DependencyManager::get<AudioClient>();
 //   connect(audioClient.data(), &AudioClient::inputReceived, this, &RecordingScriptingInterface::processAudioInput);
}

void RecordingScriptingInterface::setControlledAvatar(AvatarData* avatar) {
    _controlledAvatar = avatar;
}

bool RecordingScriptingInterface::isPlaying() const {
    return _player->isPlaying();
}

bool RecordingScriptingInterface::isPaused() const {
    return _player->isPaused();
}

float RecordingScriptingInterface::playerElapsed() const {
    return _player->position();
}

float RecordingScriptingInterface::playerLength() const {
    return _player->length();
}

void RecordingScriptingInterface::loadRecording(const QString& filename) {
    using namespace recording;

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "loadRecording", Qt::BlockingQueuedConnection,
            Q_ARG(QString, filename));
        return;
    }

    ClipPointer clip = Clip::fromFile(filename);
    if (!clip) {
        qWarning() << "Unable to load clip data from " << filename;
    }
    _player->queueClip(clip);
}

void RecordingScriptingInterface::startPlaying() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "startPlaying", Qt::BlockingQueuedConnection);
        return;
    }

    // Playback from the current position
    if (_playFromCurrentLocation && _controlledAvatar) {
        _dummyAvatar.setRecordingBasis(std::make_shared<Transform>(_controlledAvatar->getTransform()));
    } else {
        _dummyAvatar.clearRecordingBasis();
    }
    _player->play();
}

void RecordingScriptingInterface::setPlayerVolume(float volume) {
    // FIXME 
}

void RecordingScriptingInterface::setPlayerAudioOffset(float audioOffset) {
    // FIXME 
}

void RecordingScriptingInterface::setPlayerTime(float time) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "setPlayerTime", Qt::BlockingQueuedConnection, Q_ARG(float, time));
        return;
    }
    _player->seek(time);
}

void RecordingScriptingInterface::setPlayFromCurrentLocation(bool playFromCurrentLocation) {
    _playFromCurrentLocation = playFromCurrentLocation;
}

void RecordingScriptingInterface::setPlayerLoop(bool loop) {
    _player->loop(loop);
}

void RecordingScriptingInterface::setPlayerUseDisplayName(bool useDisplayName) {
    _useDisplayName = useDisplayName;
}

void RecordingScriptingInterface::setPlayerUseAttachments(bool useAttachments) {
    _useAttachments = useAttachments;
}

void RecordingScriptingInterface::setPlayerUseHeadModel(bool useHeadModel) {
    _useHeadModel = useHeadModel;
}

void RecordingScriptingInterface::setPlayerUseSkeletonModel(bool useSkeletonModel) {
    _useSkeletonModel = useSkeletonModel;
}

void RecordingScriptingInterface::pausePlayer() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "pausePlayer", Qt::BlockingQueuedConnection);
        return;
    }
    _player->pause();
}

void RecordingScriptingInterface::stopPlaying() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "stopPlaying", Qt::BlockingQueuedConnection);
        return;
    }
    _player->stop();
}

bool RecordingScriptingInterface::isRecording() const {
    return _recorder->isRecording();
}

float RecordingScriptingInterface::recorderElapsed() const {
    return _recorder->position();
}

void RecordingScriptingInterface::startRecording() {
    if (_recorder->isRecording()) {
        qCWarning(scriptengine) << "Recorder is already running";
        return;
    }

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "startRecording", Qt::BlockingQueuedConnection);
        return;
    }

    _recordingEpoch = Frame::epochForFrameTime(0);

    if (_controlledAvatar) {
        _controlledAvatar->setRecordingBasis();
    }

    _recorder->start();
}

void RecordingScriptingInterface::stopRecording() {
    _recorder->stop();
    _lastClip = _recorder->getClip();
    _lastClip->seek(0);

    if (_controlledAvatar) {
        _controlledAvatar->clearRecordingBasis();
    }
}

void RecordingScriptingInterface::saveRecording(const QString& filename) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "saveRecording", Qt::BlockingQueuedConnection,
            Q_ARG(QString, filename));
        return;
    }

    if (!_lastClip) {
        qWarning() << "There is no recording to save";
        return;
    }

    recording::Clip::toFile(filename, _lastClip);
}

void RecordingScriptingInterface::loadLastRecording() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "loadLastRecording", Qt::BlockingQueuedConnection);
        return;
    }

    if (!_lastClip) {
        qCDebug(scriptengine) << "There is no recording to load";
        return;
    }

    _player->queueClip(_lastClip);
    _player->play();
}

void RecordingScriptingInterface::processAvatarFrame(const Frame::ConstPointer& frame) {
    Q_ASSERT(QThread::currentThread() == thread());

    if (!_controlledAvatar) {
        return;
    }

    AvatarData::fromFrame(frame->data, _dummyAvatar);



    if (_useHeadModel && _dummyAvatar.getFaceModelURL().isValid() && 
        (_dummyAvatar.getFaceModelURL() != _controlledAvatar->getFaceModelURL())) {
        // FIXME
        //myAvatar->setFaceModelURL(_dummyAvatar.getFaceModelURL());
    }

    if (_useSkeletonModel && _dummyAvatar.getSkeletonModelURL().isValid() &&
        (_dummyAvatar.getSkeletonModelURL() != _controlledAvatar->getSkeletonModelURL())) {
        // FIXME
        //myAvatar->useFullAvatarURL()
    }

    if (_useDisplayName && _dummyAvatar.getDisplayName() != _controlledAvatar->getDisplayName()) {
        _controlledAvatar->setDisplayName(_dummyAvatar.getDisplayName());
    }

    _controlledAvatar->setPosition(_dummyAvatar.getPosition());
    _controlledAvatar->setOrientation(_dummyAvatar.getOrientation());

    // FIXME attachments
    // FIXME joints
    // FIXME head lean
    // FIXME head orientation
}

void RecordingScriptingInterface::processAudioInput(const QByteArray& audio) {
    if (_recorder->isRecording()) {
        static const recording::FrameType AUDIO_FRAME_TYPE = recording::Frame::registerFrameType(AUDIO_FRAME_NAME);
        _recorder->recordFrame(AUDIO_FRAME_TYPE, audio);
    }
}

void RecordingScriptingInterface::processAudioFrame(const recording::FrameConstPointer& frame) {
   // auto audioClient = DependencyManager::get<AudioClient>();
   // audioClient->handleRecordedAudioInput(frame->data);
}