#pragma once

#include <cstdint>
#include <cstdlib>
#include <new>

#include <clap/clap.h>

namespace clap {
   template <typename T = float>
   class AudioBuffer {
   public:
      AudioBuffer(uint32_t channelCount, uint32_t frameCount, double sampleRate = 0)
         : _channelCount(channelCount), _frameCount(frameCount), _sampleRate(sampleRate),
           _data(static_cast<T *>(std::aligned_alloc(32, channelCount * frameCount * sizeof(T)))) {
         if (!_data) [[unlikely]]
            throw std::bad_alloc();
      }

      AudioBuffer(const AudioBuffer<T> &other) = delete;
      AudioBuffer<T> &operator=(const AudioBuffer<T> &) = delete;
      AudioBuffer<T> &operator=(AudioBuffer<T> &&) = delete;
      AudioBuffer(AudioBuffer<T> &&o) = delete;

      ~AudioBuffer() { std::free(_data); }

      [[nodiscard]] T *data() noexcept { return _data; }
      [[nodiscard]] const T *data() const noexcept { return _data; }

      [[nodiscard]] uint32_t stride() const noexcept { return _stride; }
      [[nodiscard]] bool isConstant() const noexcept { return _stride == 0; }
      void setConstant(bool isConstant) noexcept { _stride = _channelCount * !isConstant; }
      [[nodiscard]] const T getSample(uint32_t frame, uint32_t channel) const noexcept {
         return _data[frame * _stride + channel];
      }

      [[nodiscard]] uint32_t frameCount() const noexcept { return _frameCount; }
      [[nodiscard]] uint32_t channelCount() const noexcept { return _channelCount; }

      [[nodiscard]] double sampleRate() const noexcept { return _sampleRate; }

      void fromClap(const clap_audio_buffer *buffer, uint32_t frameOffset, uint32_t frameCount) noexcept;
      void toClap(clap_audio_buffer *buffer, uint32_t frameOffset, uint32_t frameCount) const noexcept;

      // Store a + b into this buffer
      template <typename Operator>
      void compute(const Operator& op, const AudioBuffer<T>& a, const AudioBuffer<T>& b, uint32_t numFrames) noexcept;

      void sum(const AudioBuffer<T>& a, const AudioBuffer<T>& b, uint32_t numFrames) noexcept
      {
         struct Sum
         {
            T operator()(T a, T b) const noexcept { return a + b; }
         } op;

         compute(op, a, b, numFrames);
      }

      void product(const AudioBuffer<T>& a, const AudioBuffer<T>& b, uint32_t numFrames) noexcept
      {
         struct Sum
         {
            T operator()(T a, T b) const noexcept { return a * b; }
         } op;

         compute(op, a, b, numFrames);
      }

      template <typename Operator>
      void applyTo(const Operator& op, uint32_t numFrames) noexcept;

   private:
      const uint32_t _channelCount;
      const uint32_t _frameCount;
      const double _sampleRate; // 0 if unspecified
      T *const _data;
      uint32_t _stride = 0;
   };

   extern template class AudioBuffer<float>;
   extern template class AudioBuffer<double>;
} // namespace clap

#include "audio-buffer.hxx"