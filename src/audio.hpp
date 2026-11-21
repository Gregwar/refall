#pragma once
// Audio output: song playback with crossfades between tracks.
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "analysis.hpp"

struct ma_device;
struct ma_context;

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
    // silent: no sound card, the mix still runs in real time (miniaudio's null device), for the tap
    bool init(bool silent = false);
    // Called on the audio thread with each block of the mix (interleaved stereo at the engine rate): --stream
    void setTap(std::function<void(const float*, unsigned int)> tap) { tap_ = std::move(tap); }
    void shutdown();
    uint32_t sampleRate() const { return sampleRate_; }

    // crossfades from the current track; fadeIn false: the song starts at full level on its first frame
    void play(std::shared_ptr<Track> track, bool fadeIn = true);
    std::shared_ptr<Track> current() const { return current_; }
    double position() const;                 // seconds into the current track (where the mix has got to: it moves by whole audio blocks)
    // The song's clock for the game (main thread only): the position carried on in real time between the audio blocks
    // (10-40 ms apart, depending on the backend) at the rate the song plays, kept in step with them: it moves on
    // smoothly, never goes back but for a seek or a new song, and on average reads what position() did.
    double smoothPosition();
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
    // Back to the bed's start (taken by the audio thread at its next block; meant while it is silent).
    void rewindBed() { bedRewind_ = true; }
    float bedGain() const { return bedGainNow_; }
    double bedPosition() const { return (double)bedPosAtomic_ / sampleRate_; }
    // One-shot sound mixed over the song (interleaved stereo at the engine rate); heard even when paused.
    void playSample(std::shared_ptr<const std::vector<float>> pcm, float gain = 1.f);

private:
    static void dataCallback(ma_device* dev, void* out, const void* in, unsigned int frameCount);
    void render(float* out, unsigned int frames);

    ma_device* device_ = nullptr;
    ma_context* context_ = nullptr; // the null backend's (silent)
    std::function<void(const float*, unsigned int)> tap_;
    uint32_t sampleRate_ = 48000;
    std::mutex mutex_;
    std::shared_ptr<Track> current_, previous_;
    uint64_t pos_ = 0, prevPos_ = 0;
    std::atomic<uint64_t> atomicPos_{0};
    // The smooth clock. The audio thread publishes, at each block, where the song got to, when, how fast it went over
    // the block and how long the block was (a seqlock: snapSeq_ odd while it writes); gen_ counts the jumps (play,
    // seek) so the clock knows a block from before one.
    std::atomic<uint32_t> snapSeq_{0}, snapGen_{0}, gen_{0};
    std::atomic<double> snapPos_{0};       // seconds into the track at the block's end
    std::atomic<int64_t> snapNs_{0};       // the block's time (steady clock, ns)
    std::atomic<float> snapRate_{0};       // song seconds per second over the block (0 paused, or at the end)
    std::atomic<float> snapPeriod_{0};     // the block's length in seconds
    void publishClock(double posSec, int64_t ns, float rate, float period, uint32_t gen);
    double clock_ = 0;                     // the smooth clock's last reading (main thread)
    int64_t clockNs_ = 0;
    uint32_t clockGen_ = ~0u;
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
    std::atomic<bool> bedRewind_{false};
    void mixBed(float* out, unsigned int frames);
    float gain_ = 1.f;                        // current song gain (audio thread)
    void applyMuffle(float* out, unsigned int frames);
};
