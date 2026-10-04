#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <vector>

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
    if (argc < 2) {
        write_all(STDERR_FILENO, "child: не передано имя файла\n");
        return 1;
    }

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        write_all(STDERR_FILENO, "child: не удалось открыть файл\n");
        return 1;
    }

    std::string pending;
    char buf[256];
    std::vector<int> nums;

    while (true) {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            write_all(STDERR_FILENO, "child: ошибка чтения\n");
            close(fd);
            return 1;
        }
        if (n == 0) break; 
        pending.append(buf, n);

        size_t pos;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            pending.erase(0, pos + 1);

            if (!parse(line, nums)) {
                write_all(STDOUT_FILENO, "e");
                continue;
            }

            
            long long result = nums[0];
            bool zero = false;
            for (size_t i = 1; i < nums.size(); i++) {
                if (nums[i] == 0) { zero = true; break; }
                result /= nums[i];
            }

            if (zero) {
                write_all(STDOUT_FILENO, "z"); 
                close(fd);
                return 0;
            }

            if (!write_all(fd, std::to_string(result) + "\n")) {
                write_all(STDERR_FILENO, "child: ошибка записи в файл\n");
                close(fd);
                return 1;
            }
            write_all(STDOUT_FILENO, "k");
        }
    }

    close(fd);
    return 0;
}
