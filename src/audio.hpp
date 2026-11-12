#pragma once
// Audio output: song playback with crossfades between tracks.
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "analysis.hpp"

struct ma_device;

struct Track {
    std::string path, title;
    std::vector<float> pcm; // interleaved stereo at the engine sample rate
    uint32_t sampleRate = 48000;
    uint64_t frames() const { return pcm.size() / 2; }
    double duration() const { return (double)frames() / sampleRate; }
    Analysis analysis;
};

// Decodes a whole audio file (the menu's music) to interleaved stereo 16-bit at `sampleRate`; null on failure.
std::shared_ptr<std::vector<int16_t>> decodeAudio16(const std::string& path, uint32_t sampleRate);

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    bool init();
    void shutdown();
    uint32_t sampleRate() const { return sampleRate_; }

    void play(std::shared_ptr<Track> track); // crossfades from the current track
    std::shared_ptr<Track> current() const { return current_; }
    double position() const;                 // seconds into the current track
    bool finished() const;                   // current track reached its end
    void seek(double seconds);               // jump inside the current track
    void setPaused(bool p) { paused_ = p; }
    bool paused() const { return paused_; }
    void setMuted(bool m) { muted_ = m; }
    // Playback speed of the song (tape-style: slower is also lower). 1 = normal. With glideSeconds > 0
    // the speed slides there (a full 1 <-> 0.5 change takes that long) instead of jumping.
    void setSpeed(float s, float glideSeconds = 0.f) { glide_ = glideSeconds; speed_ = s; }
    float speed() const { return speed_; }          // target speed
    float currentSpeed() const { return curSpeed_; } // speed right now (during a glide)
    // A short tape-like bend of the song's speed: it slides by `depth` (e.g. -0.15 = 15% slower) and back to
    // where it was, along a smooth bump lasting `seconds`.
    void bend(float depth, float seconds);
    // Muffle the song (sound effects stay clear): a low-pass that keeps the bass and attenuates the high tones,
    // sliding in or out over `seconds`.
    void setMuffled(bool on, float seconds = 0.6f) { muffleGlide_ = seconds; muffleTarget_ = on ? 1.f : 0.f; }
    // The song's volume (sound effects keep theirs), sliding there over `seconds`: 1 = full. With setMuffled, the
    // game over's sound (the bonus' low-pass, quieter).
    void setSongGain(float g, float seconds = 0.6f) { gainGlide_ = seconds; gainTarget_ = g; }
    // The menu's music: a bed under everything (not muffled, not paused), looping with a 1 s equal-power crossfade at
    // its loop point; its gain slides to `gain` over `seconds`. Silent, it stays where it was (resumes from there).
    void setBed(std::shared_ptr<const std::vector<int16_t>> pcm);
    void setBedGain(float gain, float seconds);
    float bedGain() const { return bedGainNow_; }
    double bedPosition() const { return (double)bedPosAtomic_ / sampleRate_; }
    // One-shot sound mixed over the song (interleaved stereo at the engine rate); heard even when paused.
    void playSample(std::shared_ptr<const std::vector<float>> pcm, float gain = 1.f);

private:
    static void dataCallback(ma_device* dev, void* out, const void* in, unsigned int frameCount);
    void render(float* out, unsigned int frames);

    ma_device* device_ = nullptr;
    uint32_t sampleRate_ = 48000;
    std::mutex mutex_;
    std::shared_ptr<Track> current_, previous_;
    uint64_t pos_ = 0, prevPos_ = 0;
    std::atomic<uint64_t> atomicPos_{0};
    float fade_ = 1.f, prevFade_ = 0.f;
    struct Voice {
        std::shared_ptr<const std::vector<float>> pcm;
        size_t pos = 0;
        float gain = 1.f;
    };
    std::vector<Voice> voices_;
    std::atomic<bool> paused_{false}, muted_{false};
    std::atomic<float> speed_{1.f}, glide_{0.f}, curSpeed_{1.f};
    double frac_ = 0; // fractional position inside the current frame when slowed down
    float bendDepth_ = 0;          // current bend (guarded by mutex_)
    uint64_t bendLen_ = 0, bendPos_ = 0;
    std::atomic<float> muffleTarget_{0.f}, muffleGlide_{0.6f};
    float muffle_ = 0;                        // current muffle amount 0..1 (audio thread)
    float lpZ_[2][2] = {{0, 0}, {0, 0}};      // low-pass biquad state, per channel
    std::atomic<float> gainTarget_{1.f}, gainGlide_{0.6f};
    std::shared_ptr<const std::vector<int16_t>> bed_; // interleaved stereo at the engine rate (guarded by mutex_)
    uint64_t bedPos_ = 0;
    bool bedLooped_ = false;
    float bedGain_ = 0;
    std::atomic<float> bedTarget_{0.f}, bedGlide_{1.f}, bedGainNow_{0.f};
    std::atomic<uint64_t> bedPosAtomic_{0};
    void mixBed(float* out, unsigned int frames);
    float gain_ = 1.f;                        // current song gain (audio thread)
    void applyMuffle(float* out, unsigned int frames);
};
