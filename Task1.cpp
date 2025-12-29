#include <iostream>
#include <random>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <semaphore.h>
#include <condition_variable>
#include <atomic>

using namespace std;

// Класс Semaphore реализует системные семафоры POSIX.
// Он используется для управления доступом к ресурсу с использованием счетчика.
class Semaphore {
public:
    Semaphore(int initialCount, int maxCount) : iCount(initialCount), mCount(maxCount) {
        sem_init(&sem, 0, initialCount); // Инициализация семафора с начальным значением.
    }

    void acquire() {
        sem_wait(&sem); // Захват семафора, уменьшает счетчик на 1.
    }

    void release() {
        sem_post(&sem); // Освобождение семафора, увеличивает счетчик на 1.
    }

private:
    sem_t sem; // POSIX-семафор.
    int iCount, mCount; // Текущий и максимальный счетчик.
};


// Класс Barrier используется для синхронизации потоков, чтобы они ждали друг друга
// до достижения определенного барьера.
class Barrier {
public:
    Barrier(int count) : initialCount(count), maxCount(count), generationCount(0) {}

    void wait() {
        unique_lock<mutex> lock(mtx);
        int initialGen = generationCount; // Текущее поколение.

        if (--initialCount == 0) {
            // Если все потоки достигли барьера, переходим к следующему поколению.
            ++generationCount;
            initialCount = maxCount;
            cv.notify_all(); // Уведомление всех потоков.
        } else {
            // Ожидание, пока текущее поколение не изменится.
            cv.wait(lock, [this, initialGen] { return initialGen != generationCount; });
        }
    }

private:
    mutex mtx; // Мьютекс для защиты состояния.
    condition_variable cv; // Условная переменная для ожидания.
    int initialCount, maxCount, generationCount; // Счетчики и текущее поколение.
};

// Класс Monitor обеспечивает взаимное исключение с использованием condition_variable.
// Подходит для более сложной логики синхронизации.
class Monitor {
public:
    Monitor() : isReady(false) {}

    void locker() {
        unique_lock<mutex> lock(mtx);
        while (isReady) {
            cv.wait(lock); // Ожидание, пока ресурс не станет доступен.
        }
        isReady = true; // Установка ресурса в состояние "занято".
    }

    void unlocker() {
        lock_guard<mutex> lock(mtx);
        isReady = false; // Освобождение ресурса.
        cv.notify_one(); // Уведомление одного ожидающего потока.
    }

private:
    mutex mtx; // Мьютекс для защиты состояния.
    condition_variable cv; // Условная переменная для ожидания.
    bool isReady; // Флаг состояния ресурса.
};

// Функция для генерации случайного символа.
void randomSymbols(char& symbol) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(32, 126); // Диапазон символов ASCII.
    symbol = static_cast<char>(dis(gen));
}

// Потоковая функция с использованием мьютекса для синхронизации.
void threadMutex(char& symbol, mutex& mtx, vector<char>& allSymbols) {
    for (int i = 0; i < 10000; i++) {
        randomSymbols(symbol);
        lock_guard<mutex> lock(mtx); // Блокировка мьютекса.
        allSymbols.push_back(symbol); // Добавление символа в общий вектор.
    }
}

// Потоковая функция с использованием Semaphore для синхронизации.
void threadSemaphore(char& symbol, Semaphore& sem, vector<char>& allSymbols) {
    for (int i = 0; i < 10000; i++) {
        randomSymbols(symbol);
        sem.acquire(); // Захват семафора.
        allSymbols.push_back(symbol);
        sem.release(); // Освобождение семафора.
    }
}

// Потоковая функция с использованием Barrier для синхронизации.
void threadBarrier(char& symbol, Barrier& barrier, vector<char>& allSymbols) {
    for (int i = 0; i < 10000; i++) {
        randomSymbols(symbol);
        barrier.wait(); // Ожидание на барьере.
        allSymbols.push_back(symbol);
    }
}

// Потоковая функция с использованием spinlock (атомарного флага) для синхронизации.
void threadSpinLock(char& symbol, atomic_flag& spinLock, vector<char>& allSymbols) {
    for (int i = 0; i < 10000; i++) {
        randomSymbols(symbol);
        while (spinLock.test_and_set(memory_order_acquire)) {} // Активное ожидание блокировки.
        allSymbols.push_back(symbol);
        spinLock.clear(memory_order_release); // Сброс блокировки.
    }
}

// Потоковая функция с использованием spinlock и yield для уменьшения нагрузки на процессор.
void threadSpinWait(char& symbol, atomic_flag& spinLock, vector<char>& allSymbols) {
    for (int i = 0; i < 10000; i++) {
        randomSymbols(symbol);
        while (spinLock.test_and_set(memory_order_acquire)) {
            this_thread::yield(); // Уступить управление другому потоку.
        }
        allSymbols.push_back(symbol);
        spinLock.clear(memory_order_release);
    }
}

// Потоковая функция с использованием Monitor для синхронизации.
void threadMonitor(char& symbol, Monitor& monitor, vector<char>& allSymbols) {
    for (int i = 0; i < 10000; i++) {
        randomSymbols(symbol);
        monitor.locker(); // Захват ресурса.
        allSymbols.push_back(symbol);
        monitor.unlocker(); // Освобождение ресурса.
    }
}

int main() {
    vector<char> allSymbols; // Вектор для хранения символов.
    mutex mtx; // Мьютекс для синхронизации потоков.
    char symbol; // Переменная для случайного символа.

    // Тестирование с использованием мьютекса.
    auto start = chrono::high_resolution_clock::now();
    vector<thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(threadMutex, ref(symbol), ref(mtx), ref(allSymbols));
    }
    for (auto& t : threads) {
        t.join();
    }
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end - start;
    cout << "Mutex time: " << elapsed.count() << " seconds\n";
    threads.clear();
    allSymbols.clear();

    // Тестирование с использованием Semaphore.
    Semaphore sem(4, 4);
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(threadSemaphore, ref(symbol), ref(sem), ref(allSymbols));
    }
    for (auto& t : threads) {
        t.join();
    }
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "Semaphore time: " << elapsed.count() << " seconds\n";
    threads.clear();
    allSymbols.clear();

    // Тестирование с использованием Barrier.
    Barrier barrier(4);
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(threadBarrier, ref(symbol), ref(barrier), ref(allSymbols));
    }
    for (auto& t : threads) {
        t.join();
    }
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "Barrier time: " << elapsed.count() << " seconds\n";
    threads.clear();
    allSymbols.clear();

    // SpinLock
    atomic_flag spinLock = ATOMIC_FLAG_INIT;
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(threadSpinLock, ref(symbol), ref(spinLock), ref(allSymbols));
    }
    for (auto& t : threads) {
        t.join();
    }
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "SpinLock time: " << elapsed.count() << " seconds\n";
    threads.clear();
    allSymbols.clear();

    // SpinWait
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(threadSpinWait, ref(symbol), ref(spinLock), ref(allSymbols));
    }
    for (auto& t : threads) {
        t.join();
    }
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "SpinWait time: " << elapsed.count() << " seconds\n";
    threads.clear();
    allSymbols.clear();

    // Monitor
    Monitor monitor;
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(threadMonitor, ref(symbol), ref(monitor), ref(allSymbols));
    }
    for (auto& t : threads) {
        t.join();
    }
    end = chrono::high_resolution_clock::now();
    elapsed = end - start;
    cout << "Monitor time: " << elapsed.count() << " seconds\n";

    return 0;
}:
