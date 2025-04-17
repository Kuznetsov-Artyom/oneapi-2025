#include "jacobi_dev_oneapi.h"
#include <utility>

std::vector<float> JacobiDevONEAPI(
    const std::vector<float> a, const std::vector<float> b,
    float accuracy, sycl::device device) {
    const auto n = b.size();

    std::vector<float> x_old(n);
    std::vector<float> x_new(n, 0.0f);
    std::vector<float> a_diag(n);
    for (int i = 0; i < n; i++) a_diag[i] = a[i * n + i];

    sycl::queue q(device);
    sycl::buffer<float, 1> a_buf(a.data(), sycl::range<1>(n * n));
    sycl::buffer<float, 1> a_diag_buf(a_diag.data(), sycl::range<1>(n));
    sycl::buffer<float, 1> b_buf(b.data(), sycl::range<1>(n));
    sycl::buffer<float, 1> x_old_buf(x_old.data(), sycl::range<1>(n));
    sycl::buffer<float, 1> x_new_buf(x_new.data(), sycl::range<1>(n));

    float err = 0.0f;
    int k = ITERATIONS;
    do {
        err = 0.0f;
        std::swap(x_old_buf, x_new_buf);

        sycl::buffer<float, 1> err_buf(&err, sycl::range<1>(1));

        q.submit([&](sycl::handler &cgh) {
            auto reduction = sycl::reduction(err_buf, cgh, sycl::maximum<>());

            auto in_a = a_buf.get_access<sycl::access::mode::read>(cgh);
            auto in_a_diag = a_diag_buf.get_access<sycl::access::mode::read>(cgh);
            auto in_b = b_buf.get_access<sycl::access::mode::read>(cgh);
            auto in_x_old = x_old_buf.get_access<sycl::access::mode::read>(cgh);
            auto out_x_new = x_new_buf.get_access<sycl::access::mode::write>(cgh);

            cgh.parallel_for(sycl::range<1>(n), reduction,
                             [=](sycl::id<1> i, auto &err_max) {
                float d = 0.0f;

                int j = 0;
                for (; j < i; j++) {
                    d += in_a[i * n + j] * in_x_old[j];
                }
                j++;
                for (; j < n; j++) {
                    d += in_a[i * n + j] * in_x_old[j];
                }

                float x = (in_b[i] - d) / in_a_diag[i];
                out_x_new[i] = x;

                err_max.combine(sycl::fabs(x - in_x_old[i]));
            });
        }).wait();

        k--;
    } while (err >= accuracy && k > 0);

    sycl::host_accessor<float, 1> x_new_acc(x_new_buf);
    std::vector<float> result(n);
    for (size_t i = 0; i < n; ++i) {
        result[i] = x_new_acc[i];
    }

    return result;
}
