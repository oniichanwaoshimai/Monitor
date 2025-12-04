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
    bool provider_finished = false;     // поставщик завершил работу
    Value* shared_data = nullptr;

public:
    // Метод поставщика
    void provide(Value* temp_data) {
        for (int i = 0; i < 5; ++i) {
            this_thread::sleep_for(chrono::seconds(1));

            unique_lock<mutex> lock(mtx);

            // Ожидаем, пока потребитель не обработает предыдущее событие
            while (event_ready) {
                cv.wait(lock);
            }

            // Подготавливаем данные
            shared_data = temp_data;
            shared_data->increment();
            event_ready = true;

            cout << "Provider: event sent! Value = " << shared_data->val << endl;

            // Уведомляем потребителя
            cv.notify_one();
        }

        // Завершаем работу
        unique_lock<mutex> lock(mtx);
        provider_finished = true;
        cv.notify_one();
    }

    // Метод потребителя
    void consume() {
        while (true) {
            unique_lock<mutex> lock(mtx);

            // Ожидаем событие или завершение поставщика
            while (!event_ready && !provider_finished) {
                cv.wait(lock);
            }

            // Проверяем условие завершения
            if (provider_finished && !event_ready) {
                cout << "Consumer: finished work" << endl;
                break;
            }

            // Обрабатываем событие
            cout << "Consumer: event processed! Value = " << shared_data->val << endl;
            event_ready = false;

            // Уведомляем поставщика
            cv.notify_one();
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