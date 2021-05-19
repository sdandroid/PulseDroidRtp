/*
 * Copyright 2020 Wenxin Wang
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PULSERTP_OBOEENGINE_H
#define PULSERTP_OBOEENGINE_H

#include <cstdint>
#include <vector>
#include <atomic>
#include <thread>
#include <asio.hpp>
#include <oboe/Oboe.h>

#define MODULE_NAME "PULSE_RTP_OBOE_ENGINE"

// MTU: 1280, channel 2, sample se16e -> 320 sample per pkt
// 48k sample per s -> 150 pkt/s
// 100ms buffer: 15pkt
// RTP payload: 1280 + 12 = 1292

class PacketBuffer {
public:
    PacketBuffer(unsigned mtu, unsigned sample_rate, unsigned max_latency, unsigned num_channel);
    const std::vector<int16_t>* RefNextHeadForRead();
    std::vector<int16_t>* RefTailForWrite();
    bool NextTail();

    unsigned capacity() const { return pkts_.size(); }
    unsigned size() const { return size_; }
    unsigned head_move_req() const { return head_move_req_; }
    unsigned head_move() const { return head_move_; }
    unsigned tail_move_req() const { return tail_move_req_; }
    unsigned tail_move() const { return tail_move_; }
private:
    std::vector<std::vector<int16_t>> pkts_;
    std::atomic<unsigned> head_;
    std::atomic<unsigned> tail_;
    std::atomic<unsigned> size_;

    std::atomic<unsigned> head_move_req_;
    std::atomic<unsigned> head_move_;
    std::atomic<unsigned> tail_move_req_;
    std::atomic<unsigned> tail_move_;
};

class RtpReceiveThread {
public:
    RtpReceiveThread(PacketBuffer& pkt_buffer, std::string  ip, uint16_t port, unsigned mtu);
    ~RtpReceiveThread();
    bool Start();

    unsigned pkt_recved() const { return pkt_recved_; }
private:
    void Restart();
    void Stop();
    void StartReceive();
    void HandleReceive(size_t bytes_recvd);
    PacketBuffer& pkt_buffer_;
    asio::io_context io_;
    std::string ip_;
    uint16_t port_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint sender_endpoint_;
    std::vector<char> data_;
    asio::steady_timer idle_check_timer_;
    std::thread thread_;
    bool is_idle_ = false;
    unsigned pkt_recved_ = 0;
};

class PulseRtpOboeEngine
: public oboe::AudioStreamCallback {
public:
    static std::unique_ptr<PulseRtpOboeEngine> Create(
            int latency_option, const std::string &ip, uint16_t port, unsigned mtu,
            unsigned max_latency, unsigned num_channel, unsigned mask_channel
    );
    ~PulseRtpOboeEngine();

    int num_underrun() const { return num_underrun_; }
    int audio_buffer_size() const { return audio_buffer_size_; }
    unsigned pkt_buffer_capacity() const { return pkt_buffer_.capacity(); }
    unsigned pkt_buffer_size() const { return pkt_buffer_.size(); }
    unsigned pkt_buffer_head_move_req() const { return pkt_buffer_.head_move_req(); }
    unsigned pkt_buffer_head_move() const { return pkt_buffer_.head_move(); }
    unsigned pkt_buffer_tail_move_req() const { return pkt_buffer_.tail_move_req(); }
    unsigned pkt_buffer_tail_move() const { return pkt_buffer_.tail_move(); }
    unsigned pkt_recved() const { return receive_thread_.pkt_recved(); }
    int32_t getBufferCapacityInFrames() const {
        return managedStream_->getBufferSizeInFrames();
    }

    int getSharingMode() const {
        return (int)managedStream_->getSharingMode();
    }

    int getPerformanceMode() const {
        return (int)managedStream_->getPerformanceMode();
    }

    int32_t getFramesPerBurst() const {
        return managedStream_->getFramesPerBurst();
    }

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;
private:
    PulseRtpOboeEngine(const std::string& ip, uint16_t port, unsigned mtu,
                       unsigned max_latency, unsigned num_channel, unsigned mask_channel);
    bool Start(int latency_option, const std::string& ip, uint16_t port, unsigned mtu);
    void Stop();
    bool EnsureBuffer();
    PacketBuffer pkt_buffer_;
    RtpReceiveThread receive_thread_;
    oboe::ManagedStream managedStream_;
    std::unique_ptr<oboe::LatencyTuner> latencyTuner_;

    unsigned num_channel_ = 0;
    unsigned num_output_channel_ = 0;
    unsigned mask_channel_ = 0;

    const std::vector<int16_t>* buffer_ = nullptr;
    unsigned offset_ = 0;
    std::vector<int16_t> last_samples_;
    enum State {
        None,
        Overrun,
        Underrun,
        Depleted,
    } state_ = State::None;
    bool is_thread_affinity_set_ = false;

    std::atomic<int> num_underrun_;
    std::atomic<int> audio_buffer_size_;
};

#endif //PULSERTP_OBOEENGINE_H
