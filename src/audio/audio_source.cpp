//
// Created by William on 2025-10-17.
//

#include "audio_source.h"

namespace Audio
{
AudioSource::AudioSource() = default;

AudioSource::~AudioSource() = default;

AudioSource::AudioSource(const AudioSource& other) noexcept
    : position(other.position.load())
      , velocity(other.velocity.load())
      , clip(other.clip)
      , baseVolume(other.baseVolume)
      , basePitch(other.basePitch)
      , baseSpeed(other.baseSpeed)
      , looping(other.looping)
      , spatial(other.spatial)
      , doppler(other.doppler)
      , playbackPosition(other.playbackPosition)
      , dopplerPitch(other.dopplerPitch)
      , bIsPlaying(other.bIsPlaying)
      , bIsFinished(other.bIsFinished)
{}

AudioSource& AudioSource::operator=(const AudioSource& other) noexcept
{
    if (this != &other) {
        position.store(other.position.load());
        velocity.store(other.velocity.load());
        clip = other.clip;
        baseVolume = other.baseVolume;
        basePitch = other.basePitch;
        baseSpeed = other.baseSpeed;
        looping = other.looping;
        spatial = other.spatial;
        doppler = other.doppler;
        playbackPosition = other.playbackPosition;
        dopplerPitch = other.dopplerPitch;
        bIsPlaying = other.bIsPlaying;
        bIsFinished = other.bIsFinished;
    }
    return *this;
}

AudioSource::AudioSource(AudioSource&& other) noexcept
    : position(other.position.load())
      , velocity(other.velocity.load())
      , clip(other.clip)
      , baseVolume(other.baseVolume)
      , basePitch(other.basePitch)
      , baseSpeed(other.baseSpeed)
      , looping(other.looping)
      , spatial(other.spatial)
      , doppler(other.doppler)
      , playbackPosition(other.playbackPosition)
      , dopplerPitch(other.dopplerPitch)
      , bIsPlaying(other.bIsPlaying)
      , bIsFinished(other.bIsFinished)
{}

AudioSource& AudioSource::operator=(AudioSource&& other) noexcept
{
    if (this != &other) {
        position.store(other.position.load());
        velocity.store(other.velocity.load());
        clip = other.clip;
        baseVolume = other.baseVolume;
        basePitch = other.basePitch;
        baseSpeed = other.baseSpeed;
        looping = other.looping;
        spatial = other.spatial;
        doppler = other.doppler;
        playbackPosition = other.playbackPosition;
        dopplerPitch = other.dopplerPitch;
        bIsPlaying = other.bIsPlaying;
        bIsFinished = other.bIsFinished;
    }
    return *this;
}
} // Audio
