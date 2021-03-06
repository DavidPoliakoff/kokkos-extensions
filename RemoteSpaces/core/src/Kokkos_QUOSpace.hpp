/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact  H. Carter Edwards (hcedwar@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#ifndef KOKKOS_QUOSPACE_HPP
#define KOKKOS_QUOSPACE_HPP

#include <cstring>
#include <string>
#include <iosfwd>
#include <typeinfo>

#include <Kokkos_Core_fwd.hpp>
#include <Kokkos_Concepts.hpp>
#include <Kokkos_MemoryTraits.hpp>

#include <impl/Kokkos_Traits.hpp>
#include <impl/Kokkos_Error.hpp>
#include <impl/Kokkos_SharedAlloc.hpp>

#include <Kokkos_RemoteSpaces.hpp>

#include <quo.h>
#include <quo-xpm.h>

/*--------------------------------------------------------------------------*/

namespace Kokkos {

class QUOSpace {
 public:
  typedef QUOSpace memory_space;
  typedef size_t size_type;

#if defined(KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_OPENMP)
  typedef Kokkos::OpenMP execution_space;
#elif defined(KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_THREADS)
  typedef Kokkos::Threads execution_space;
#elif defined(KOKKOS_ENABLE_OPENMP)
  typedef Kokkos::OpenMP execution_space;
#elif defined(KOKKOS_ENABLE_THREADS)
  typedef Kokkos::Threads execution_space;
//#elif defined( KOKKOS_ENABLE_QTHREADS )
//  typedef Kokkos::Qthreads  execution_space;
#elif defined(KOKKOS_ENABLE_SERIAL)
  typedef Kokkos::Serial execution_space;
#else
#error \
    "At least one of the following host execution spaces must be defined: Kokkos::OpenMP, Kokkos::Threads, Kokkos::Qthreads, or Kokkos::Serial.  You might be seeing this message if you disabled the Kokkos::Serial device explicitly using the Kokkos_ENABLE_Serial:BOOL=OFF CMake option, but did not enable any of the other host execution space devices."
#endif

  typedef Kokkos::Device<execution_space, memory_space> device_type;

  QUOSpace();
  QUOSpace(QUOSpace&& rhs)      = default;
  QUOSpace(const QUOSpace& rhs) = default;
  QUOSpace& operator=(QUOSpace&&) = default;
  QUOSpace& operator=(const QUOSpace&) = default;
  ~QUOSpace()                          = default;

  explicit QUOSpace(const MPI_Comm&);

  void* allocate(const size_t arg_alloc_size) const;

  void deallocate(void* const arg_alloc_ptr, const size_t arg_alloc_size) const;

  void* allocate(const int* gids, const int& arg_local_alloc_size) const;

  void deallocate(const int* gids, void* const arg_alloc_ptr,
                  const size_t arg_alloc_size) const;

  /**\brief Return Name of the MemorySpace */
  static constexpr const char* name() { return m_name; }

  void fence();

  int* rank_list;
  int allocation_mode;
  int64_t extent;

  void impl_set_rank_list(int* const);
  void impl_set_allocation_mode(const int);
  void impl_set_extent(int64_t N);

 private:
  QUO_context quo_context;
  static constexpr const char* m_name = "QUO";
  friend class Kokkos::Impl::SharedAllocationRecord<Kokkos::QUOSpace, void>;
};

}  // namespace Kokkos

//----------------------------------------------------------------------------

namespace Kokkos {

namespace Impl {

static_assert(Kokkos::Impl::MemorySpaceAccess<Kokkos::QUOSpace,
                                              Kokkos::QUOSpace>::assignable,
              "");

}  // namespace Impl

}  // namespace Kokkos

//----------------------------------------------------------------------------

namespace Kokkos {

namespace Impl {

template <>
class SharedAllocationRecord<Kokkos::QUOSpace, void>
    : public SharedAllocationRecord<void, void> {
 private:
  friend Kokkos::QUOSpace;

  typedef SharedAllocationRecord<void, void> RecordBase;

  SharedAllocationRecord(const SharedAllocationRecord&) = delete;
  SharedAllocationRecord& operator=(const SharedAllocationRecord&) = delete;

  static void deallocate(RecordBase*);

  /**\brief  Root record for tracked allocations from this QUOSpace instance */
  static RecordBase s_root_record;

  const Kokkos::QUOSpace m_space;

 protected:
  ~SharedAllocationRecord();
  SharedAllocationRecord() = default;

  SharedAllocationRecord(
      const Kokkos::QUOSpace& arg_space, const std::string& arg_label,
      const size_t arg_alloc_size,
      const RecordBase::function_type arg_dealloc = &deallocate);

 public:
  inline std::string get_label() const {
    return std::string(RecordBase::head()->m_label);
  }

  KOKKOS_INLINE_FUNCTION static SharedAllocationRecord* allocate(
      const Kokkos::QUOSpace& arg_space, const std::string& arg_label,
      const size_t arg_alloc_size) {
#if defined(KOKKOS_ACTIVE_EXECUTION_MEMORY_SPACE_HOST)
    return new SharedAllocationRecord(arg_space, arg_label, arg_alloc_size);
#else
    return (SharedAllocationRecord*)0;
#endif
  }

  /**\brief  Allocate tracked memory in the space */
  static void* allocate_tracked(const Kokkos::QUOSpace& arg_space,
                                const std::string& arg_label,
                                const size_t arg_alloc_size);

  /**\brief  Reallocate tracked memory in the space */
  static void* reallocate_tracked(void* const arg_alloc_ptr,
                                  const size_t arg_alloc_size);

  /**\brief  Deallocate tracked memory in the space */
  static void deallocate_tracked(void* const arg_alloc_ptr);

  static SharedAllocationRecord* get_record(void* arg_alloc_ptr);

  static void print_records(std::ostream&, const Kokkos::QUOSpace&,
                            bool detail = false);
};

}  // namespace Impl

}  // namespace Kokkos

//----------------------------------------------------------------------------

namespace Kokkos {

namespace Impl {

template <>
struct MemorySpaceAccess<Kokkos::HostSpace, Kokkos::QUOSpace> {
  enum { assignable = false };
  enum { accessible = true };
  enum { deepcopy = true };
};

template <>
struct VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,
                                           Kokkos::QUOSpace> {
  enum { value = true };
  inline static void verify(void) {}
  inline static void verify(const void*) {}
};

template <class ExecutionSpace>
struct DeepCopy<QUOSpace, QUOSpace, ExecutionSpace> {
  DeepCopy(void* dst, const void* src, size_t n) { memcpy(dst, src, n); }

  DeepCopy(const ExecutionSpace& exec, void* dst, const void* src, size_t n) {
    exec.fence();
    memcpy(dst, src, n);
  }
};

}  // namespace Impl

}  // namespace Kokkos

#endif  // #define KOKKOS_QUOSPACE_HPP
