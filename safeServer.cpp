#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

using namespace std;

// Класс для безопасного сервера

class SafeServer {
private:
    int serverFd;                    // Дескриптор слушающего сокета
    int port;                        // Порт сервера
    bool isRunning;                  // Флаг работы сервера
    
    // Обработка сигналов
    static volatile sig_atomic_t signalReceived;
    sigset_t blockedMask;            // Маска заблокированных сигналов
    sigset_t originalMask;           // Исходная маска сигналов
    
    // Клиентские подключения: дескриптор -> информация
    map<int, pair<string, int>> clients; // clientFd -> {ip, port}
    
public:
    SafeServer(int p = 8080) : serverFd(-1), port(p), isRunning(false) {
        signalReceived = 0;
    }
    
    ~SafeServer() {
        shutdown();
    }

    // Обработчик сигнала SIGHUP
    
    static void signalHandler(int sig) {
        // Простейший обработчик - только устанавливает флаг
        // Выполняется в контексте сигнала, поэтому должен быть минимальным
        if (sig == SIGHUP) {
            signalReceived = 1;
            // Можно добавить syslog() для логирования, но не cout!
        }
    }
    
    //Настройка обработки сигналов
    void setupSignalHandling() {
        struct sigaction sa;
        
        // Получаем текущие настройки
        sigaction(SIGHUP, NULL, &sa);
        
        // Устанавливаем наш обработчик
        sa.sa_handler = signalHandler;
        sa.sa_flags = SA_RESTART;    // перезапуск системных вызовов
        
        // Регистрируем обработчик
        if (sigaction(SIGHUP, &sa, NULL) == -1) {
            perror("sigaction failed");
            exit(EXIT_FAILURE);
        }
        
    }
    
    // Блокировка сигнала
    
    void blockSignals() {
        // Инициализируем пустую маску
        sigemptyset(&blockedMask);
        
        // Добавляем SIGHUP в маску блокируемых сигналов
        sigaddset(&blockedMask, SIGHUP);
        
        // Блокируем сигнал SIGHUP
        if (sigprocmask(SIG_BLOCK, &blockedMask, &originalMask) == -1) {
            perror("sigprocmask failed");
            exit(EXIT_FAILURE);
        }
        
    }
    
    // Создание серверного сокета
    void createServerSocket() {
        // Создаем TCP сокет
        serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd == -1) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }
        
        // Разрешаем повторное использование адреса
        int opt = 1;
        if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            perror("setsockopt failed");
            close(serverFd);
            exit(EXIT_FAILURE);
        }
        
        // Настраиваем адрес сервера
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        // Привязываем сокет к адресу
        if (bind(serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
            perror("bind failed");
            close(serverFd);
            exit(EXIT_FAILURE);
        }
        
        // Переводим сокет в режим прослушивания
        if (listen(serverFd, 10) == -1) {  // Очередь из 10 ожидающих подключений
            perror("listen failed");
            close(serverFd);
            exit(EXIT_FAILURE);
        }
        
        cout << "[SERVER] Port: " << port << endl;
        cout << "[SERVER] PID: " << getpid() << endl;
    }
    
    // Принятие нового подключения
    void acceptNewConnection() {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientFd = accept(serverFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd == -1) {
            if (errno == EINTR) {
                // accept был прерван сигналом
                cout << "[ACCEPT] Прервано сигналом" << endl;
                return;
            }
            perror("accept failed");
            return;
        }
        
        // Получаем информацию о клиенте
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
        int clientPort = ntohs(clientAddr.sin_port);
        
        cout << "\n[CLIENT] New connection:" << endl;
        cout << "        IP: " << clientIp << ":" << clientPort << endl;
        cout << "        FD: " << clientFd << endl;
        cout << "        Active clients: " << (clients.size() + 1) << endl;
        
        // Добавляем клиента в список
        clients[clientFd] = make_pair(string(clientIp), clientPort);
        
        // Отправляем приветственное сообщение
        const char* welcome = "Welcome ! Type something...\n";
        send(clientFd, welcome, strlen(welcome), 0);
    }
    
    // Обработка данных от клиента
    void handleClientData(int clientFd) {
        char buffer[1024];
        ssize_t bytesRead;
        
        // Читаем данные от клиента
        bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRead > 0) {
            // Успешно прочитали данные
            buffer[bytesRead] = '\0';
            
            auto& clientInfo = clients[clientFd];
            cout << "\n[DATA] From: " << clientInfo.first << ":" 
                 << clientInfo.second << endl;
            cout << "       FD: " << clientFd << endl;
            cout << "       Size: " << bytesRead << " bytes" << endl;
            cout << "       Text: " << string(buffer, bytesRead) << endl;
            
            // Эхо-ответ (пусть будет)
            send(clientFd, buffer, bytesRead, 0);
            
        } else if (bytesRead == 0) {
            // Клиент отключился
            cout << "\n[CLIENT] disconnected: " << clients[clientFd].first 
                 << ":" << clients[clientFd].second << endl;
            closeClient(clientFd);
            
        } else {
            // Ошибка при чтении
            if (errno != EINTR) {
                perror("recv failed");
                closeClient(clientFd);
            }
        }
    }
    
    // Закрытие клиентского соединения
    void closeClient(int clientFd) {
        if (clients.find(clientFd) != clients.end()) {
            auto& info = clients[clientFd];
            clients.erase(clientFd);
            cout << "[CLIENT] Active clients: " << clients.size() << endl;
        }
        close(clientFd);
    }
    
    // Основной цикл с pselect()
    void run() {
        isRunning = true;
        
        while (isRunning) {
            // Подготовка множества дескрипторов для pselect()
            fd_set readFds;
            int maxFd = serverFd;
            
            FD_ZERO(&readFds);
            FD_SET(serverFd, &readFds);
            
            // Добавляем клиентские дескрипторы
            for (const auto& client : clients) {
                int clientFd = client.first;
                FD_SET(clientFd, &readFds);
                if (clientFd > maxFd) maxFd = clientFd;
            }
            
            // aтомарный вызов pselect()
            // pselect временно разблокирует SIGHUP на время ожидания,
            // предотвращая race condition между проверкой флага и ожиданием
            int ready = pselect(maxFd + 1, &readFds, NULL, NULL, NULL, &originalMask);
            
            if (ready == -1) {
                // Ошибка или прерывание сигналом
                if (errno == EINTR) {
                    // Был получен сигнал
                    if (signalReceived) {
                        cout << "\n[SIGNAL] SIGHUP recieved" << endl;
                        signalReceived = 0;  // Сбрасываем флаг
                    }
                    continue;  // Возвращаемся к ожиданию
                }
                
                perror("pselect failed");
                break;
            }
            
            // Обработка сетевых событий
            
            // a) Проверяем новое подключение на серверном сокете
            if (FD_ISSET(serverFd, &readFds)) {
                acceptNewConnection();
            }
            
            // b) Проверяем данные от существующих клиентов
            // Создаем копию списка, чтобы безопасно удалять элементы во время итерации
            vector<int> currentClients;
            for (const auto& client : clients) {
                currentClients.push_back(client.first);
            }
            
            for (int clientFd : currentClients) {
                if (FD_ISSET(clientFd, &readFds)) {
                    handleClientData(clientFd);
                }
            }
        }
    }
    
    // Завершение работы сервера
    void shutdown() {
        cout << "\n[SERVER] shutting down..." << endl;
        isRunning = false;
        
        //cout << "[SERVER] Закрытие всех клиентских соединений (" 
             //<< clients.size() << " клиентов)" << endl;
        
        // Закрываем все клиентские соединения
        for (const auto& client : clients) {
            cout << "[SERVER] Quitting FD " << client.first << endl;
            close(client.first);
        }
        clients.clear();
        
        // Закрываем серверный сокет
        if (serverFd != -1) {
            close(serverFd);
            serverFd = -1;
        }
        
        cout << "[SERVER] Off" << endl;
    }
};

// Инициализация статической переменной
volatile sig_atomic_t SafeServer::signalReceived = 0;

int main(int argc, char* argv[]) {
    // Настройка порта (по умолчанию 8080)
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    cout << "Save TCP-server" << endl;
    
    try {
        SafeServer server(port);
        
        // Настраиваем обработку сигналов
        server.setupSignalHandling();
        
        // Блокируем сигнал SIGHUP
        server.blockSignals();
        
        // Создаем серверный сокет
        server.createServerSocket();
        
        // Запускаем основной цикл
        server.run();
        
    } catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
