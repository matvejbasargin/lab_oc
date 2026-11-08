#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <errno.h>
#include <string>
#include <vector>

#include <stdint.h>

#define SHM_SIZE 4096
#define MSG_EXIT UINT32_MAX   

struct SharedData {
    uint32_t len;                                   
    char     status;                                
    char     data[SHM_SIZE - sizeof(uint32_t) - 1]; 
};

static_assert(sizeof(SharedData) <= SHM_SIZE, "SharedData не помещается в SHM_SIZE");

static bool write_all(int fd, const std::string& s) {
    const char* p = s.c_str();
    size_t left = s.size();
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += n;
        left -= n;
    }
    return true;
}

static bool parse(const std::string& s, std::vector<int>& out) {
    out.clear();
    size_t i = 0, n = s.size();
    while (true) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) i++;
        if (i >= n) break;

        bool neg = false;
        if (s[i] == '-' || s[i] == '+') {
            neg = (s[i] == '-');
            i++;
        }
        if (i >= n || s[i] < '0' || s[i] > '9') return false;

        long long v = 0;
        while (i < n && s[i] >= '0' && s[i] <= '9') {
            v = v * 10 + (s[i] - '0');
            if (v > 2147483648LL) return false;
            i++;
        }
        if (i < n && s[i] != ' ' && s[i] != '\t' && s[i] != '\r') return false;
        if (neg) v = -v;
        if (v > 2147483647LL || v < -2147483648LL) return false;
        out.push_back((int)v);
    }
    return !out.empty();
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        write_all(STDERR_FILENO, "child: ожидаются аргументы: файл shm sem_cmd sem_resp\n");
        return 1;
    }

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        write_all(STDERR_FILENO, "child: не удалось открыть файл\n");
        return 1;
    }

    int shm = shm_open(argv[2], O_RDWR, 0);
    if (shm == -1) {
        write_all(STDERR_FILENO, "child: не удалось открыть shared memory\n");
        close(fd);
        return 1;
    }
    SharedData* sh = (SharedData*)mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE,
                                       MAP_SHARED, shm, 0);
    if (sh == MAP_FAILED) {
        write_all(STDERR_FILENO, "child: mmap не удался\n");
        close(shm);
        close(fd);
        return 1;
    }
    sem_t* cmd  = sem_open(argv[3], O_RDWR);
    sem_t* resp = sem_open(argv[4], O_RDWR);
    if (cmd == SEM_FAILED || resp == SEM_FAILED) {
        write_all(STDERR_FILENO, "child: не удалось открыть семафор\n");
        munmap(sh, SHM_SIZE);
        close(shm);
        close(fd);
        return 1;
    }

    int code = 0;
    std::vector<int> nums;

    while (true) {
        if (sem_wait(cmd) == -1) {
            if (errno == EINTR) continue;
            write_all(STDERR_FILENO, "child: ошибка sem_wait\n");
            code = 1;
            break;
        }

        if (sh->len == MSG_EXIT) break;   

        std::string line(sh->data, sh->len);
        if (!line.empty() && line.back() == '\n') line.pop_back();

        if (!parse(line, nums)) {
            sh->status = 'e';
            sem_post(resp);
            continue;
        }

        
        long long result = nums[0];
        bool zero = false;
        for (size_t i = 1; i < nums.size(); i++) {
            if (nums[i] == 0) { zero = true; break; }
            result /= nums[i];
        }

        if (zero) {
            sh->status = 'z';
            sem_post(resp);
            break;                        
        }

        if (!write_all(fd, std::to_string(result) + "\n")) {
            write_all(STDERR_FILENO, "child: ошибка записи в файл\n");
            code = 1;
            break;                        
        }
        sh->status = 'k';
        sem_post(resp);
    }

    sem_close(cmd);
    sem_close(resp);
    munmap(sh, SHM_SIZE);
    close(shm);
    close(fd);
    return code;
}
