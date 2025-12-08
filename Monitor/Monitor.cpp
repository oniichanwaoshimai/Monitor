#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace std;

class Value {
public:
    int val;
    Value(int value = 1) {
        val = value;
    }
    ~Value() = default;

    void increment() {
        val++;
    }
};

class Monitor {
private:
    mutex mtx;
    condition_variable cv;
    bool event_ready = false;           // событие готово для обработки
    bool isStopped = false;             // монитор остановлен
    Value* shared_data = nullptr;

public:
    // Метод поставщика
    void provide(Value* temp_data) {
        for (int i = 0; i < 5; ++i) {
            this_thread::sleep_for(chrono::seconds(1)); // типа имитация полезной нагрузки

            unique_lock<mutex> lock(mtx, defer_lock);  // СОЗДАЛИ, НЕ БЛОКИРУЕМ
            lock.lock();  // ЯВНАЯ блокировка когда нужно

            // Ожидаем, пока потребитель не обработает предыдущее событие
            // И проверяем, не остановлен ли монитор
            while (event_ready && !isStopped) {
                cv.wait(lock);
            }

            // Если монитор остановлен - выходим
            if (isStopped) {
                cout << "Monitor is stopped!" << endl;
                lock.unlock();  // Явная разблокировка перед break
                break;
            }

            // Подготавливаем данные
            shared_data = temp_data;
            shared_data->increment();
            event_ready = true;

            cout << "Provider: event sent! Value = " << shared_data->val << endl;

            // Уведомляем потребителя
            cv.notify_one();    

            lock.unlock(); // тож явно разблокируем
        }

        // Завершаем работу - останавливаем монитор
        unique_lock<mutex> lock(mtx, defer_lock);
        lock.lock();
        isStopped = true;
        cv.notify_one();  // Будим потребителя, если он спит
        lock.unlock();
    }

    // Метод потребителя
    void consume() {
        while (true) {
            unique_lock<mutex> lock(mtx, defer_lock);
            lock.lock();    

            // Ожидаем событие или остановку монитора
            while (!event_ready && !isStopped) {
                cv.wait(lock);
            }

            // Проверяем условие завершения
            if (isStopped) {
                cout << "Consumer: finished work" << endl;
                lock.unlock(); // тут тоже надо разблокать, тк до нижнего можем не дойти
                break;
            }

            // Обрабатываем событие
            cout << "Consumer: event processed! Value = " << shared_data->val << endl;
            event_ready = false;

            // Уведомляем поставщика
            cv.notify_one();

            lock.unlock();
        }
    }
};

int main() {
    Monitor monitor;
    Value value_obj(10);

    thread provider_thread(&Monitor::provide, &monitor, &value_obj);
    thread consumer_thread(&Monitor::consume, &monitor);

    provider_thread.join();
    consumer_thread.join();

    cout << "Final value from main: " << value_obj.val << endl;
    cout << "end" << endl;
    return 0;
}
