// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#include "NtpServer.h"
#include <errno.h>
#include <fcntl.h>
#include <lwip/sockets.h>
#include <unistd.h>
#include <esp_timer.h>
#include <string.h>

namespace aac {
namespace {
void count(uint32_t& value) { if (value != UINT32_MAX) ++value; }

#ifdef AAC_NTP_TIMING_DIAGNOSTICS
class ScopeTiming {
public:
    explicit ScopeTiming(TimingMetric& metric) : metric_(metric), startedAt_(esp_timer_get_time()) {}
    ~ScopeTiming() {
        const MonotonicUs endedAt = static_cast<MonotonicUs>(esp_timer_get_time());
        metric_.add(endedAt >= startedAt_ ? endedAt - startedAt_ : 0);
    }
private:
    TimingMetric& metric_;
    MonotonicUs startedAt_;
};
#endif
}

#ifdef AAC_NTP_TIMING_DIAGNOSTICS
NtpTimingSnapshot NtpServer::takeTimingSnapshot() {
    const NtpTimingSnapshot snapshot = timing_;
    timing_.clear();
    return snapshot;
}
#endif

NtpServer::~NtpServer() { closeSocket(); }

void NtpServer::closeSocket() {
    if (socket_ >= 0) close(socket_);
    socket_ = -1;
    boundAddress_ = 0;
    diagnostics_.state = NtpServiceState::Offline;
}

bool NtpServer::openSocket(uint32_t lanAddress) {
    closeSocket();
    const int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        diagnostics_.state = NtpServiceState::Error;
        diagnostics_.lastError = errno;
        return false;
    }
    const int flags = fcntl(fd, F_GETFL, 0);
    const int receiveBytes = 512;
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &receiveBytes, sizeof(receiveBytes)) < 0) {
        diagnostics_.lastError = errno;
        close(fd);
        diagnostics_.state = NtpServiceState::Error;
        return false;
    }
    struct sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_port = htons(123);
    local.sin_addr.s_addr = lanAddress;
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&local), sizeof(local)) < 0) {
        diagnostics_.lastError = errno;
        close(fd);
        diagnostics_.state = NtpServiceState::Error;
        return false;
    }
    socket_ = fd;
    boundAddress_ = lanAddress;
    diagnostics_.state = NtpServiceState::Listening;
    diagnostics_.lastError = 0;
    return true;
}

void NtpServer::poll(bool lanConnected, uint32_t lanAddress, const ClockState& clock, MonotonicUs now) {
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    ScopeTiming serviceDuration(timing_.serviceDuration);
    if (haveOpportunity_ && now >= lastOpportunityUs_) timing_.opportunities.add(now - lastOpportunityUs_);
    lastOpportunityUs_ = now;
    haveOpportunity_ = true;
    const MonotonicUs pollStartedAt = static_cast<MonotonicUs>(esp_timer_get_time());
#endif
    int64_t ignoredUtc = 0;
    diagnostics_.authorityQualified = currentUtc(clock, now, ignoredUtc);
    diagnostics_.stratum = diagnostics_.authorityQualified ? kNtpStratumAac : kNtpStratumUnsynchronized;
    if (!lanConnected || !lanAddress) {
        if (socket_ >= 0 || diagnostics_.state != NtpServiceState::Offline) closeSocket();
        lastOpenAttemptUs_ = 0;
        return;
    }
    if (boundAddress_ != lanAddress && socket_ >= 0) {
        closeSocket();
        lastOpenAttemptUs_ = 0; // A new DHCP address is a rebind, not a retry after failure.
    }
    if (socket_ < 0 && now - lastOpenAttemptUs_ >= 5000000ULL) {
        lastOpenAttemptUs_ = now;
        openSocket(lanAddress);
    }
    if (socket_ < 0) {
        return;
    }

    // One fixed-size nonblocking receive per application pass keeps UDP work bounded.
    uint8_t request[512];
    struct sockaddr_in peer = {};
    socklen_t peerSize = sizeof(peer);
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    const MonotonicUs recvStartedAt = static_cast<MonotonicUs>(esp_timer_get_time());
#endif
    const int received = recvfrom(socket_, request, sizeof(request), MSG_DONTWAIT,
                                  reinterpret_cast<struct sockaddr*>(&peer), &peerSize);
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    const MonotonicUs recvReturnedAt = static_cast<MonotonicUs>(esp_timer_get_time());
#endif
    if (received < 0) {
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
        timing_.recvCall.add(recvReturnedAt >= recvStartedAt ? recvReturnedAt - recvStartedAt : 0);
        if (timing_.noDatagram != UINT32_MAX) ++timing_.noDatagram;
#endif
        if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
            diagnostics_.lastError = errno;
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
            if (timing_.errors != UINT32_MAX) ++timing_.errors;
#endif
        }
        return;
    }
    count(diagnostics_.requests);
    const MonotonicUs receivedAt = static_cast<MonotonicUs>(esp_timer_get_time());
    uint8_t response[kNtpPacketSize];
    bool synchronized = false;
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    const MonotonicUs transmitAt = static_cast<MonotonicUs>(esp_timer_get_time());
    timing_.recvCall.add(recvReturnedAt >= recvStartedAt ? recvReturnedAt - recvStartedAt : 0);
    timing_.receiveToT2.add(receivedAt >= recvReturnedAt ? receivedAt - recvReturnedAt : 0);
    timing_.pollToReceive.add(receivedAt >= pollStartedAt ? receivedAt - pollStartedAt : 0);
    timing_.t2ToT3.add(transmitAt >= receivedAt ? transmitAt - receivedAt : 0);
    if (timing_.dequeued != UINT32_MAX) ++timing_.dequeued;
    if (timing_.requests != UINT32_MAX) ++timing_.requests;
    const auto result = buildNtpResponse(request, static_cast<size_t>(received), clock,
        receivedAt, transmitAt, response, sizeof(response), synchronized);
#else
    const auto result = buildNtpResponse(request, static_cast<size_t>(received), clock,
        receivedAt, static_cast<MonotonicUs>(esp_timer_get_time()), response, sizeof(response), synchronized);
#endif
    if (result != NtpRequestError::None) {
        count(diagnostics_.rejectedRequests);
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
        if (timing_.errors != UINT32_MAX) ++timing_.errors;
#endif
        return;
    }
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    const MonotonicUs sendStartedAt = static_cast<MonotonicUs>(esp_timer_get_time());
    const int sent = sendto(socket_, response, sizeof(response), MSG_DONTWAIT,
                            reinterpret_cast<struct sockaddr*>(&peer), peerSize);
    const MonotonicUs sendReturnedAt = static_cast<MonotonicUs>(esp_timer_get_time());
    timing_.t3ToSend.add(sendStartedAt >= transmitAt ? sendStartedAt - transmitAt : 0);
    timing_.sendCall.add(sendReturnedAt >= sendStartedAt ? sendReturnedAt - sendStartedAt : 0);
#else
    const int sent = sendto(socket_, response, sizeof(response), MSG_DONTWAIT,
                            reinterpret_cast<struct sockaddr*>(&peer), peerSize);
#endif
    if (sent != static_cast<int>(sizeof(response))) {
        diagnostics_.lastError = sent < 0 ? errno : EIO;
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
        if (timing_.errors != UINT32_MAX) ++timing_.errors;
#endif
        return;
    }
    count(diagnostics_.replies);
#ifdef AAC_NTP_TIMING_DIAGNOSTICS
    if (timing_.replies != UINT32_MAX) ++timing_.replies;
#endif
    if (synchronized) count(diagnostics_.synchronizedReplies);
    else count(diagnostics_.unsynchronizedReplies);
    diagnostics_.authorityQualified = synchronized;
    diagnostics_.stratum = synchronized ? kNtpStratumAac : kNtpStratumUnsynchronized;
    diagnostics_.lastError = 0;
}
} // namespace aac
