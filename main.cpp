#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <future>
#include <mutex>
#include <thread>

template <typename T, typename F, std::size_t... Is>
std::array<T, sizeof...(Is)> make_array_impl(F &&func, std::index_sequence<Is...>) {
    auto factory = [&](auto i) -> T { return T(func, i); };
    return std::array<T, sizeof...(Is)> {(void(Is), factory(Is))...};
}

template <typename T, std::size_t N, typename F>
std::array<T, N> make_array(F &&func) {
    return make_array_impl<T>(func, std::make_index_sequence<N> {});
}

int main(void) {
    static constexpr std::size_t N_THREADS {100};

    std::mutex mux;
    std::atomic_bool exit {false};

    std::array<int, N_THREADS> hits {0};
    std::array<std::promise<void>, N_THREADS> thread_promises {};
    std::promise<void> ready_promise;
    std::shared_future<void> ready_future {ready_promise.get_future()};

    auto func = [&, ready_future](int id) {
        thread_promises[id].set_value();
        ready_future.wait();

        while (!exit.load()) {
            std::lock_guard<std::mutex> lock(mux);
            hits[id]++;
            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
        }
    };

    std::array<std::thread, N_THREADS> threads {make_array<std::thread, N_THREADS>(func)};
    for (auto &&f : thread_promises) {
        f.get_future().wait();
    }
    ready_promise.set_value();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    exit.store(true);

    for (auto &&t : threads) {
        t.join();
    }

    for (std::size_t i = 0; i < hits.size(); ++i) {
        std::cerr << "[" << i << "] hit " << hits[i] << " times" << std::endl;
    }

    return 0;
}

