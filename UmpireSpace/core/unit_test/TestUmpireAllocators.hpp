

namespace Test {

template <class T>
struct TestUmpireAllocators {
  const int N          = 100;
  using exec_device    = Kokkos::DefaultExecutionSpace;
  using exec_host      = Kokkos::DefaultHostExecutionSpace;
  using default_device = Kokkos::CudaSpace;
  using default_host   = Kokkos::HostSpace;
  //  using default_device   = typename exec_device::memory_space;
  //  using default_host     =
  //  Kokkos::Impl::if_c<std::is_same<Kokkos::Impl::DefaultHostMemorySpace,
  //                           Kokkos::UmpireSpace<Kokkos::HostSpace>>::value,
  //                           Kokkos::HostSpace, typename
  //                           exec_host::memory_space>;
  using mem_space_host   = Kokkos::UmpireSpace<default_host>;
  using mem_space_device = Kokkos::UmpireSpace<default_device>;
  using host_view_type   = Kokkos::View<T*, mem_space_host>;
  using device_view_type = Kokkos::View<T*, mem_space_device>;

  void run_tests() {
    // no pooling
    //
    {
       mem_space_host no_alloc_host;
       mem_space_device no_alloc_device;

       // direct memory allocation (not really a case we should share...)
       double* ptr = (double*)no_alloc_device.allocate(N * sizeof(double));
       Kokkos::parallel_for(
          Kokkos::RangePolicy<Kokkos::DefaultExecutionSpace>(0, N),
          KOKKOS_LAMBDA(const int i) { ptr[i] = (double)i; });

       // allocate views and mirrors
       device_view_type v1(Kokkos::view_alloc("v1", no_alloc_device), N);
       host_view_type v2(Kokkos::view_alloc("v2", no_alloc_host), N);

       auto h_v1 = Kokkos::create_mirror(Kokkos::HostSpace(), v1);
       auto h_v2 = Kokkos::create_mirror(Kokkos::HostSpace(), v2);

       // initialize host mirrors and copy to umpire space
       for (int i = 0; i < N; i++) {
          h_v1(i) = i;
          h_v2(i) = i * 2;
       }
       Kokkos::deep_copy(v1, h_v1);
       Kokkos::deep_copy(v2, h_v2);

       // Execute kernels one on device and one on host
       Kokkos::parallel_for(
           Kokkos::RangePolicy<Kokkos::DefaultExecutionSpace>(0, N),
           KOKKOS_LAMBDA(const int i) { v1(i) *= 2; });
       Kokkos::fence();
       Kokkos::parallel_for(
           Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, N),
           KOKKOS_LAMBDA(const int i) { v2(i) *= 2; });

       // copy back to host and validate
       Kokkos::deep_copy(h_v1, v1);
       Kokkos::deep_copy(h_v2, v2);

       for (int i = 0; i < N; i++) {
         ASSERT_EQ(h_v1(i), 2 * i);
         ASSERT_EQ(h_v2(i), i * 4);
       }
    }
    {
       // pooled allocator - host
       //
       mem_space_host::make_new_allocator<umpire::strategy::DynamicPool>("_pool");
       mem_space_host pooled_host("HOST_pool");
       double* ptrII = (double*)pooled_host.allocate(N * sizeof(double));
       pooled_host.deallocate(ptrII, N * sizeof(double));

       host_view_type v2(Kokkos::view_alloc("v2", pooled_host), N);

       auto h_v2 = Kokkos::create_mirror(Kokkos::HostSpace(), v2);

       // initialize host mirrors and copy to umpire space
       for (int i = 0; i < N; i++) {
          h_v2(i) = i * 2;
       }
       Kokkos::deep_copy(v2, h_v2);
       Kokkos::parallel_for(
           Kokkos::RangePolicy<Kokkos::DefaultHostExecutionSpace>(0, N),
           KOKKOS_LAMBDA(const int i) { v2(i) *= 2; });

       // copy back to host and validate
       Kokkos::deep_copy(h_v2, v2);

       for (int i = 0; i < N; i++) {
         ASSERT_EQ(h_v2(i), i * 4);
       }
    }
    {
       // pooled allocator - device
       mem_space_device::make_new_allocator<umpire::strategy::DynamicPool>(
           "_pool");
       mem_space_device pooled_dev("DEVICE_pool");
       double* ptrIII = (double*)pooled_dev.allocate(N * sizeof(double));
       pooled_dev.deallocate(ptrIII, N * sizeof(double));

       // allocate views and mirrors
       device_view_type v1(Kokkos::view_alloc("v1", pooled_dev), N);

       auto h_v1 = Kokkos::create_mirror(Kokkos::HostSpace(), v1);

       // initialize host mirrors and copy to umpire space
       for (int i = 0; i < N; i++) {
          h_v1(i) = i;
       }
       Kokkos::deep_copy(v1, h_v1);

       // Execute kernels one on device and one on host
       Kokkos::parallel_for(
           Kokkos::RangePolicy<Kokkos::DefaultExecutionSpace>(0, N),
           KOKKOS_LAMBDA(const int i) { v1(i) *= 2; });
       
       // copy back to host and validate
       Kokkos::deep_copy(h_v1, v1);

       for (int i = 0; i < N; i++) {
         ASSERT_EQ(h_v1(i), 2 * i);
       }
    }
    // typed allocator
    //
       printf("tests complete, let the descoping begin...\n");
  }
};

TEST(TEST_CATEGORY, umpire_space_view_allocators) {
  TestUmpireAllocators<double> f{};
  f.run_tests();
}

}  // namespace Test
