// g++ -o child child.cpp -pthread -lrt
// g++ -o parent parent.cpp -pthread -lrt
// ./parent

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <string>

#include <stdint.h>

#define SHM_SIZE 4096
#define MSG_EXIT UINT32_MAX   // значение len = "родитель закончил работу"

// Раскладка общей памяти (должна быть одинакова в parent.cpp и child.cpp)
struct SharedData {
    uint32_t len;                                   // длина строки в data; MSG_EXIT = выход
    char     status;                                // ответ ребёнка: 'k' / 'e' / 'z'
    char     data[SHM_SIZE - sizeof(uint32_t) - 1]; // строка с числами
};

static_assert(sizeof(SharedData) <= SHM_SIZE, "SharedData не помещается в SHM_SIZE");

static void print(int fd, const std::string& s) {
    const char* p = s.c_str();
    size_t left = s.size();
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return;
        }
        p += n;
        left -= n;
    }
}

static bool read_line(std::string& line) {
    line.clear();
    char c;
    while (true) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return !line.empty();
        if (c == '\n') return true;
        line += c;
    }
}

static std::string self_dir() {
    char path[4096];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1) return ".";
    path[len] = '\0';
    while (len > 0 && path[len] != '/') --len;
    path[len] = '\0';
    return std::string(path);
}

// Ждём ответ ребёнка (sem_post от него), но раз в 100 мс проверяем, жив ли он.
// 0 - дождались; -1 - ребёнок завершился; -2 - ошибка семафора
static int wait_reply(sem_t* sem, pid_t pid, bool& exited) {
    while (true) {
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100 * 1000 * 1000;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }
        if (sem_timedwait(sem, &ts) == 0) return 0;
        if (errno == EINTR) continue;
        if (errno == ETIMEDOUT) {
            int st;
            if (waitpid(pid, &st, WNOHANG) == pid) {
                exited = true;
                return -1;
            }
            continue;
        }
        return -2;
    }
}

int main() {
    print(STDOUT_FILENO, "Введите имя файла: ");
    std::string fileName;
    if (!read_line(fileName) || fileName.empty()) {
        print(STDERR_FILENO, "Ошибка: имя файла не задано\n");
        return 1;
    }

    // Уникальные имена для каждой пары parent/child: добавляем pid родителя
    std::string suffix  = std::to_string(getpid());
    std::string shmName = "/lab3_shm_" + suffix;
    std::string semCmd  = "/lab3_cmd_" + suffix;   // parent -> child: "строка готова"
    std::string semResp = "/lab3_rsp_" + suffix;   // child -> parent: "ответ готов"
    std::string childPath = self_dir() + "/child";

    int shm = shm_open(shmName.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (shm == -1) {
        print(STDERR_FILENO, "Ошибка: не удалось создать shared memory\n");
        return 1;
    }
    if (ftruncate(shm, SHM_SIZE) == -1) {
        print(STDERR_FILENO, "Ошибка: ftruncate не удался\n");
        close(shm);
        shm_unlink(shmName.c_str());
        return 1;
    }
    SharedData* sh = (SharedData*)mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE,
                                       MAP_SHARED, shm, 0);
    if (sh == MAP_FAILED) {
        print(STDERR_FILENO, "Ошибка: mmap не удался\n");
        close(shm);
        shm_unlink(shmName.c_str());
        return 1;
    }
    sh->len = 0;
    sh->status = 0;

    // Оба семафора стартуют с 0: "события ещё не было"
    sem_t* cmd  = sem_open(semCmd.c_str(),  O_CREAT | O_EXCL, 0600, 0);
    sem_t* resp = sem_open(semResp.c_str(), O_CREAT | O_EXCL, 0600, 0);

    auto cleanup = [&]() {
        if (cmd != SEM_FAILED)  { sem_close(cmd);  sem_unlink(semCmd.c_str()); }
        if (resp != SEM_FAILED) { sem_close(resp); sem_unlink(semResp.c_str()); }
        munmap(sh, SHM_SIZE);
        close(shm);
        shm_unlink(shmName.c_str());
    };

    if (cmd == SEM_FAILED || resp == SEM_FAILED) {
        print(STDERR_FILENO, "Ошибка: не удалось создать семафор\n");
        cleanup();
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        print(STDERR_FILENO, "Ошибка: fork не удался\n");
        cleanup();
        return 1;
    }
    if (pid == 0) {
        execl(childPath.c_str(), "child",
              fileName.c_str(), shmName.c_str(), semCmd.c_str(), semResp.c_str(),
              (char*)nullptr);
        print(STDERR_FILENO, "Ошибка: не удалось запустить child\n");
        _exit(1);
    }

    bool exited = false;
    print(STDOUT_FILENO, "Вводите числа через пробел (пустая строка или Ctrl+D для выхода):\n");

    const size_t maxLen = sizeof(sh->data);
    std::string line;
    while (read_line(line)) {
        if (line.empty()) break;
        line += '\n';
        if (line.size() > maxLen) {
            print(STDOUT_FILENO, "Слишком длинная строка, максимум " +
                                 std::to_string(maxLen - 1) + " символов\n");
            continue;
        }

        // кладём строку в shared memory и будим ребёнка
        for (size_t i = 0; i < line.size(); i++) sh->data[i] = line[i];
        sh->len = (uint32_t)line.size();
        sem_post(cmd);

        // ждём ответ
        int r = wait_reply(resp, pid, exited);
        if (r != 0) {
            print(STDERR_FILENO, "Дочерний процесс неожиданно завершился\n");
            break;
        }
        char status = sh->status;
        if (status == 'z') {
            print(STDOUT_FILENO, "Деление на 0. Завершение работы.\n");
            break;
        }
        if (status == 'e') {
            print(STDOUT_FILENO, "Некорректный ввод, ожидается: число число ...\n");
        }
    }

    // сообщаем ребёнку о выходе (если он ещё жив) и ждём его
    if (!exited) {
        sh->len = MSG_EXIT;
        sem_post(cmd);
        int st;
        waitpid(pid, &st, 0);
    }

    cleanup();
    return 0;
}
