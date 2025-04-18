// Copyright (c) 2025 Kulagin Aleksandr
#include "jacobi_acc_oneapi.h"

std::vector<float> JacobiAccONEAPI(const std::vector<float> a, const std::vector<float> b, float accuracy, sycl::device device) {
  static_assert(ITERATIONS >= 1, "Must be more than 1 iteration");
  const size_t n = b.size();
  std::vector<float> res(n, 0.0f);
  std::vector<float> res_prev(res);
  int attempt = 0;
  float error = 0.0f;
  {
    sycl::queue dev_queue(device);
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::buffer<float, 1> b_buf(b.data(), b.size());
    sycl::buffer<float, 1> res_buf(res.data(), res.size());
    sycl::buffer<float, 1> res_prev_buf(res_prev.data(), res_prev.size());
    sycl::buffer<float, 1> error_buf(&error, 1);
    auto get_currect_res_buf = [&res_prev_buf, &res_buf, &attempt](bool is_res)->sycl::buffer<float, 1>&{
      const int attempt_even = attempt % 2;
      if (is_res) {
        if (attempt_even == 0) {
          return res_buf;
        } else {
          return res_prev_buf;
        }
      } else {
        if (attempt_even == 0) {
          return res_prev_buf;
        } else {
          return res_buf;
        }
      }
    };
    while (attempt < ITERATIONS) {
      dev_queue.submit([&](sycl::handler& handler) {
        auto in_a = a_buf.get_access<sycl::access::mode::read>(handler);
        auto in_b = b_buf.get_access<sycl::access::mode::read>(handler);
        auto in_res_prev = get_currect_res_buf(false).get_access<sycl::access::mode::read>(handler);
        auto out_res = get_currect_res_buf(true).get_access<sycl::access::mode::write>(handler);
        auto reduction = sycl::reduction(error_buf, handler, sycl::maximum<float>());
        handler.parallel_for(sycl::range<1>(n), reduction, [=](sycl::id<1> id, auto& error) {
          const size_t i = id.get(0);
          float g = in_b[i];
          for (size_t j = 0; j < n; j++) {
            if (i != j) {
              g -= in_a[i * n + j] * in_res_prev[j];
            }
          }
          g /= in_a[i * n + i];
          out_res[i] = g;
          error.combine(sycl::fabs(g - in_res_prev[i]));
        });
      });
      dev_queue.wait();
      {
        auto error_host = error_buf.get_host_access();
        if (error_host[0] < accuracy) {
          break;
        }
        error_host[0] = 0.0f;
      }
      attempt++;
    }
  }
  return res;
}
