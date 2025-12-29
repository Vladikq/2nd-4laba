#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

using namespace std;

class ThreadVector {
public:
    ThreadVector() : size_(0), capacity_(1) {
        threads_ = new thread*[capacity_];
    }

    ~ThreadVector() {
        delete[] threads_;
    }

    void add(thread* t) {
        if (size_ == capacity_) {
            expand();
        }
        threads_[size_++] = t;
    }

    thread* get(size_t index) const {
        return threads_[index];
    }

    size_t count() const {
        return size_;
    }

private:
    thread** threads_;
    size_t size_;
    size_t capacity_;

    void expand() {
        capacity_ *= 2;
        thread** new_threads = new thread*[capacity_];
        for (size_t i = 0; i < size_; ++i) {
            new_threads[i] = threads_[i];
        }
        delete[] threads_;
        threads_ = new_threads;
    }
};

const int kNumPhilosophers = 5;
mutex forks[kNumPhilosophers];
mutex output_lock;

void philosopherRoutine(int id) {
    {
        lock_guard<mutex> lock(output_lock);
        cout << "Философ " << id + 1 << " размышляет\n";
    }
    this_thread::sleep_for(chrono::seconds(5));

    // Определяем порядок взятия вилок, чтобы избежать взаимной блокировки
    mutex& first_fork = forks[min(id, (id + 1) % kNumPhilosophers)];
    mutex& second_fork = forks[max(id, (id + 1) % kNumPhilosophers)];

    lock(first_fork, second_fork);
    lock_guard<mutex> lock1(first_fork, adopt_lock);
    lock_guard<mutex> lock2(second_fork, adopt_lock);

    {
        lock_guard<mutex> lock(output_lock);
        cout << "Философ " << id + 1 << " ест\n";
    }
    this_thread::sleep_for(chrono::seconds(5));

    {
        lock_guard<mutex> lock(output_lock);
        cout << "Философ " << id + 1 << " закончил\n";
    }
}

int main() {
    ThreadVector threads;

    for (int i = 0; i < kNumPhilosophers; ++i) {
        threads.add(new thread(philosopherRoutine, i));
    }

    for (size_t i = 0; i < threads.count(); ++i) {
        threads.get(i)->join();
        delete threads.get(i);
    }

    return 0;
}
