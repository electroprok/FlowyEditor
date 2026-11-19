#pragma once
// Runs a shell command in the background, streams stdout+stderr, supports stdin (POSIX) and stop.
#if !defined(_WIN32)
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

struct Proc {
    std::mutex mu; std::string buf;
    std::atomic<bool> running{false}, finished{false}; std::atomic<int> exitCode{0};
    std::thread th; int inFd = -1; long pid = -1; Uint32 stopTick = 0;

    ~Proc() { stop(); join(); }
    void join() { if (th.joinable()) th.join(); }
    void append(const std::string& s) { std::lock_guard<std::mutex> l(mu); buf += s; }
    std::string drain() { std::lock_guard<std::mutex> l(mu); std::string r; r.swap(buf); return r; }

    bool start(const std::string& cmd, const std::string& cwd, std::string& err) {
        if (running) { err = "A process is already running"; return false; }
        join();
#if defined(__IPHONEOS__) || defined(__IOS__)
        (void)cmd; (void)cwd; err = "Running programs is not possible on iOS"; return false;
#elif defined(_WIN32)
        running = true; finished = false; stopTick = 0;
        std::string full = (cwd.empty() ? "" : "cd /d \"" + cwd + "\" && ") + cmd + " 2>&1";
        th = std::thread([this, full] {
            FILE* f = _popen(full.c_str(), "r");
            if (!f) { append("Cannot start process\n"); exitCode = -1; }
            else { char b[512]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) append(std::string(b, n)); exitCode = _pclose(f); }
            running = false; finished = true;
        });
        return true;
#else
        int po[2], pi[2];
        if (pipe(po) != 0) { err = "pipe() failed"; return false; }
        if (pipe(pi) != 0) { close(po[0]); close(po[1]); err = "pipe() failed"; return false; }
        pid_t p = fork();
        if (p < 0) { close(po[0]); close(po[1]); close(pi[0]); close(pi[1]); err = "fork() failed"; return false; }
        if (p == 0) {
            setsid(); dup2(pi[0], 0); dup2(po[1], 1); dup2(po[1], 2);
            close(po[0]); close(po[1]); close(pi[0]); close(pi[1]);
            if (!cwd.empty() && chdir(cwd.c_str()) != 0) {}
#if defined(__ANDROID__)
            execl("/system/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
#else
            execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
#endif
            _exit(127);
        }
        close(po[1]); close(pi[0]); inFd = pi[1]; pid = p; running = true; finished = false; stopTick = 0; int rfd = po[0];
        th = std::thread([this, rfd, p] {
            char b[1024]; ssize_t n;
            while ((n = read(rfd, b, sizeof b)) > 0) append(std::string(b, (size_t)n));
            close(rfd); int stt = 0; waitpid(p, &stt, 0);
            exitCode = WIFEXITED(stt) ? WEXITSTATUS(stt) : (WIFSIGNALED(stt) ? 128 + WTERMSIG(stt) : -1);
            { std::lock_guard<std::mutex> l(mu); if (inFd >= 0) { close(inFd); inFd = -1; } }
            running = false; finished = true;
        });
        return true;
#endif
    }
    void sendLine(const std::string& s) {
#if !defined(_WIN32)
        std::lock_guard<std::mutex> l(mu);
        if (inFd >= 0) { std::string t = s + "\n"; if (write(inFd, t.data(), t.size()) < 0) {} }
#else
        (void)s;
#endif
    }
    bool canSend() const {
#if defined(_WIN32)
        return false;
#else
        return running && inFd >= 0;
#endif
    }
    void stop() {
#if !defined(_WIN32)
        if (running && pid > 0) { kill(-(pid_t)pid, SIGTERM); stopTick = SDL_GetTicks(); }
#endif
    }
    void poll() {   // escalate to SIGKILL if the process ignores SIGTERM
#if !defined(_WIN32)
        if (running && stopTick && SDL_GetTicks() - stopTick > 1500 && pid > 0) { kill(-(pid_t)pid, SIGKILL); stopTick = 0; }
#endif
    }
};
