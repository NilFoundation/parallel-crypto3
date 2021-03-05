//---------------------------------------------------------------------------//
// Copyright (c) 2018-2021 Mikhail Komarov <nemo@nil.foundation>
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//---------------------------------------------------------------------------//

#pragma once

#include <boost/predef.h>

#if BOOST_OS_LINUX
#include <endian.h>
#elif BOOST_OS_MACOS || BOOST_OS_IOS
#include <machine/endian.h>
#endif

#include <sys/time.h>
#include <sys/uio.h>

#include <csignal>
#include <cstdint>

namespace nil {
    namespace actor {

        namespace detail {

            namespace linux_abi {

                using aio_context_t = unsigned long;

                enum class iocb_cmd : uint16_t {
                    PREAD = 0,
                    PWRITE = 1,
                    FSYNC = 2,
                    FDSYNC = 3,
                    POLL = 5,
                    NOOP = 6,
                    PREADV = 7,
                    PWRITEV = 8,
                };

                struct io_event {
                    uint64_t data;
                    uint64_t obj;
                    int64_t res;
                    int64_t res2;
                };

                constexpr int IOCB_FLAG_RESFD = 1;

                struct iocb {
                    uint64_t aio_data;

#if __BYTE_ORDER == __LITTLE_ENDIAN
                    uint32_t aio_key;
                    int32_t aio_rw_flags;
#elif __BYTE_ORDER == __BIG_ENDIAN
                    int32_t aio_rw_flags;
                    uint32_t aio_key;
#else
#error bad byteorder
#endif

                    iocb_cmd aio_lio_opcode;
                    int16_t aio_reqprio;
                    uint32_t aio_fildes;

                    uint64_t aio_buf;
                    uint64_t aio_nbytes;
                    int64_t aio_offset;

                    uint64_t aio_reserved2;

                    uint32_t aio_flags;

                    uint32_t aio_resfd;
                };

                struct aio_sigset {
                    const sigset_t *sigmask;
                    size_t sigsetsize;
                };

            }    // namespace linux_abi

            linux_abi::iocb make_read_iocb(int fd, uint64_t offset, void *buffer, size_t len);
            linux_abi::iocb make_write_iocb(int fd, uint64_t offset, const void *buffer, size_t len);
            linux_abi::iocb make_readv_iocb(int fd, uint64_t offset, const ::iovec *iov, size_t niov);
            linux_abi::iocb make_writev_iocb(int fd, uint64_t offset, const ::iovec *iov, size_t niov);
            linux_abi::iocb make_poll_iocb(int fd, uint32_t events);

            void set_user_data(linux_abi::iocb &iocb, void *data);
            void *get_user_data(const linux_abi::iocb &iocb);
            void set_nowait(linux_abi::iocb &iocb, bool nowait);

            void set_eventfd_notification(linux_abi::iocb &iocb, int eventfd);

            linux_abi::iocb *get_iocb(const linux_abi::io_event &ioev);

            int io_setup(int nr_events, linux_abi::aio_context_t *io_context);
            int io_destroy(linux_abi::aio_context_t io_context);
            int io_submit(linux_abi::aio_context_t io_context, long nr, linux_abi::iocb **iocbs);
            int io_cancel(linux_abi::aio_context_t io_context, linux_abi::iocb *iocb, linux_abi::io_event *result);
            int io_getevents(linux_abi::aio_context_t io_context, long min_nr, long nr, linux_abi::io_event *events,
                             const ::timespec *timeout, bool force_syscall = false);
            int io_pgetevents(linux_abi::aio_context_t io_context, long min_nr, long nr, linux_abi::io_event *events,
                              const ::timespec *timeout, const sigset_t *sigmask, bool force_syscall = false);

            void setup_aio_context(size_t nr, linux_abi::aio_context_t *io_context);

        }    // namespace detail

        extern bool aio_nowait_supported;

        namespace detail {

            inline linux_abi::iocb make_read_iocb(int fd, uint64_t offset, void *buffer, size_t len) {
                linux_abi::iocb iocb {};
                iocb.aio_lio_opcode = linux_abi::iocb_cmd::PREAD;
                iocb.aio_fildes = fd;
                iocb.aio_offset = offset;
                iocb.aio_buf = reinterpret_cast<uintptr_t>(buffer);
                iocb.aio_nbytes = len;
                return iocb;
            }

            inline linux_abi::iocb make_write_iocb(int fd, uint64_t offset, const void *buffer, size_t len) {
                linux_abi::iocb iocb {};
                iocb.aio_lio_opcode = linux_abi::iocb_cmd::PWRITE;
                iocb.aio_fildes = fd;
                iocb.aio_offset = offset;
                iocb.aio_buf = reinterpret_cast<uintptr_t>(buffer);
                iocb.aio_nbytes = len;
                return iocb;
            }

            inline linux_abi::iocb make_readv_iocb(int fd, uint64_t offset, const ::iovec *iov, size_t niov) {
                linux_abi::iocb iocb {};
                iocb.aio_lio_opcode = linux_abi::iocb_cmd::PREADV;
                iocb.aio_fildes = fd;
                iocb.aio_offset = offset;
                iocb.aio_buf = reinterpret_cast<uintptr_t>(iov);
                iocb.aio_nbytes = niov;
                return iocb;
            }

            inline linux_abi::iocb make_writev_iocb(int fd, uint64_t offset, const ::iovec *iov, size_t niov) {
                linux_abi::iocb iocb {};
                iocb.aio_lio_opcode = linux_abi::iocb_cmd::PWRITEV;
                iocb.aio_fildes = fd;
                iocb.aio_offset = offset;
                iocb.aio_buf = reinterpret_cast<uintptr_t>(iov);
                iocb.aio_nbytes = niov;
                return iocb;
            }

            inline linux_abi::iocb make_poll_iocb(int fd, uint32_t events) {
                linux_abi::iocb iocb {};
                iocb.aio_lio_opcode = linux_abi::iocb_cmd::POLL;
                iocb.aio_fildes = fd;
                iocb.aio_buf = events;
                return iocb;
            }

            inline linux_abi::iocb make_fdsync_iocb(int fd) {
                linux_abi::iocb iocb {};
                iocb.aio_lio_opcode = linux_abi::iocb_cmd::FDSYNC;
                iocb.aio_fildes = fd;
                return iocb;
            }

            inline void set_user_data(linux_abi::iocb &iocb, void *data) {
                iocb.aio_data = reinterpret_cast<uintptr_t>(data);
            }

            inline void *get_user_data(const linux_abi::iocb &iocb) {
                return reinterpret_cast<void *>(uintptr_t(iocb.aio_data));
            }

            inline void set_eventfd_notification(linux_abi::iocb &iocb, int eventfd) {
                iocb.aio_flags |= linux_abi::IOCB_FLAG_RESFD;
                iocb.aio_resfd = eventfd;
            }

            inline linux_abi::iocb *get_iocb(const linux_abi::io_event &ev) {
                return reinterpret_cast<linux_abi::iocb *>(uintptr_t(ev.obj));
            }

            inline void set_nowait(linux_abi::iocb &iocb, bool nowait) {
#ifdef RWF_NOWAIT
                if (aio_nowait_supported) {
                    if (nowait) {
                        iocb.aio_rw_flags |= RWF_NOWAIT;
                    } else {
                        iocb.aio_rw_flags &= ~RWF_NOWAIT;
                    }
                }
#endif
            }

        }    // namespace detail

    }    // namespace actor
}    // namespace nil

