#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <string>


static void print(int fd, const std:: string& s){
    const char* p = s.c_str();
    size_t left = s.size();
    while (left > 0){
        ssize_t n = write(fd, p, left);
        if (n < 0){
            if (errno == EINTR) continue;
            return;
        }
        p += n;
        left -=n;
    }
}

static bool read_line(std::string& line){
    line.clear();
    char c;
    while (true){
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if ( n < 0){
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0){
            return !line.empty();
        }
        if (c == '\n'){
            return true;
        }
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


int main(){
    signal (SIGPIPE, SIG_IGN);

    print(STDOUT_FILENO, "Введите имя файла: ");
    std::string fileName;
    if(!read_line(fileName) || fileName.empty()){
        print(STDERR_FILENO, "Ошибка: имя файла не задано\n");
        return 1;
    }

    std:: string childPath = self_dir() + "/child";

    int pipe1[2], pipe2[2];
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1){
        print(STDERR_FILENO, "Ошибка: не удалось создать pipe\n");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1){
        print(STDERR_FILENO, "Ошибка: fork не удался\n");
        return 1;
    }
        if (pid == 0) {
        
        close(pipe1[1]);
        close(pipe2[0]);
        if (dup2(pipe1[0], STDIN_FILENO) == -1 ||
            dup2(pipe2[1], STDOUT_FILENO) == -1) {
            print(STDERR_FILENO, "Ошибка: dup2 не удался\n");
            _exit(1);
        }
        close(pipe1[0]);
        close(pipe2[1]);
        execl(childPath.c_str(), "child", fileName.c_str(), (char*)nullptr);
        print(STDERR_FILENO, "Ошибка: не удалось запустить child\n");
        _exit(1);
    }

    // родительский процесс
    close(pipe1[0]);
    close(pipe2[1]);

    print(STDOUT_FILENO, "Вводите числа через пробел (пустая строка или Ctrl+D для выхода):\n");

    std::string line;
    while (read_line(line)) {
        if (line.empty()) break; // пустая строка = выход

        line += '\n';
        print(pipe1[1], line);

        char status;
        ssize_t n;
        do {
            n = read(pipe2[0], &status, 1);
        } while (n < 0 && errno == EINTR);

        if (n <= 0) { // ребёнок умер/закрыл pipe
            print(STDERR_FILENO, "Дочерний процесс неожиданно завершился\n");
            break;
        }
        if (status == 'z') {
            print(STDOUT_FILENO, "Деление на 0. Завершение работы.\n");
            break;
        }
        if (status == 'e') {
            print(STDOUT_FILENO, "Некорректный ввод, ожидается: число число ...\n");
        }
    }

    close(pipe1[1]);
    close(pipe2[0]);
    int st;
    waitpid(pid, &st, 0);
    return 0;

}