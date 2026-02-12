#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>
#include <sys/select.h>
#include <cstring>
#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include <thread>
#include <atomic>
#include <chrono>

#define DEV_NUM_KEYS 12
#define EXTRA_BUTTONS 2
#define OFFSET 263

using namespace std;

// Variable global necesaria exclusivamente para el manejador de señales
int side_btn_fd_global = -1;

void cleanup(int sig) {
    if (side_btn_fd_global != -1) {
        ioctl(side_btn_fd_global, EVIOCGRAB, 0);
        close(side_btn_fd_global);
    }
    exit(0);
}

// Funciones auxiliares estáticas
static bool contains_case_insensitive(std::string h, std::string n){
    auto lower = [](unsigned char c){ return std::tolower(c); };
    std::transform(h.begin(), h.end(), h.begin(), lower);
    std::transform(n.begin(), n.end(), n.begin(), lower);
    return h.find(n) != std::string::npos;
}

static bool ends_with(const std::string& s, const std::string& suf){
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

static std::string safe_home_dir() {
    const char* home = getenv("HOME");
    if (home && *home) return std::string(home);
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return std::string(pw->pw_dir);
    return std::string("/");
}

// Busca pares (event-kbd, event-mouse) para Razer Naga en /dev/input/by-id
static bool supports_naga_side_keys(const std::string& devpath) {
    int fd = open(devpath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return false;

    // Bitset de teclas soportadas
    unsigned long keybits[(KEY_MAX + 8) / 8 / sizeof(unsigned long)] = {0};
    int rc = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
    close(fd);
    if (rc < 0) return false;

    auto test_bit = [&](int code) -> bool {
        const int idx = code / (8 * sizeof(unsigned long));
        const int bit = code % (8 * sizeof(unsigned long));
        return (keybits[idx] >> bit) & 1UL;
    };

    for (int code = 2; code <= 13; ++code) {
        if (!test_bit(code)) return false;
    }
    return true;
}

static bool find_naga_pair(std::string& out_kbd, std::string& out_mouse) {
    const char* dirpath = "/dev/input/by-id";
    DIR* d = opendir(dirpath);
    if (!d) return false;

    std::vector<std::string> entries;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        entries.emplace_back(e->d_name);
    }
    closedir(d);

    // Filtra entradas "Razer" y "Naga"
    std::vector<std::string> naga;
    for(auto& name : entries){
        if(contains_case_insensitive(name, "razer") && contains_case_insensitive(name, "naga") &&
           contains_case_insensitive(name, "event")) {
            naga.push_back(name);
        }
    }

    for(const auto& k : naga){
        if(!ends_with(k, "-if02-event-kbd")) continue;

        std::string kbd_path = std::string(dirpath) + "/" + k;
        if (!supports_naga_side_keys(kbd_path)) continue;
      
        std::string prefix = k;
        prefix.resize(prefix.size() - std::string("-event-kbd").size());

        std::string candidate1 = prefix + "-event-mouse";

        std::string prefix2 = prefix;
        auto pos = prefix2.rfind("-if");
        if(pos != std::string::npos) {
            prefix2.resize(pos);
        }
        std::string candidate2 = prefix2 + "-event-mouse";

        auto exists = [&](const std::string& fname){
            std::string path = std::string(dirpath) + "/" + fname;
            return access(path.c_str(), R_OK) == 0;
        };

        if(exists(candidate1)) {
            out_kbd  = std::string(dirpath) + "/" + k;
            out_mouse= std::string(dirpath) + "/" + candidate1;
            return true;
        }
        if(exists(candidate2)) {
            out_kbd  = std::string(dirpath) + "/" + k;
            out_mouse= std::string(dirpath) + "/" + candidate2;
            return true;
        }
    }

    // Plan B: si no encontramos pareja “por prefijo”, cogemos el primer kbd y el primer mouse
    std::string kbd_any, mouse_any;
    for(const auto& n : naga){
        if(kbd_any.empty() && ends_with(n, "-event-kbd")) kbd_any = std::string(dirpath) + "/" + n;
        if(mouse_any.empty() && ends_with(n, "-event-mouse")) mouse_any = std::string(dirpath) + "/" + n;
    }
    if(!kbd_any.empty() && !mouse_any.empty()){
        out_kbd = kbd_any;
        out_mouse = mouse_any;
        return true;
    }

    return false;
}

class NagaDaemon {
    enum class Operators { chmap, key, run, run2, click, workspace, workspace_r, position, delay, media, toggle };
    struct input_event ev1[64], ev2[64];
    int id, side_btn_fd, extra_btn_fd, size;

    vector<vector<string>> args;
    vector<vector<Operators>> options;
    vector<vector<int>> state;
    vector<pair<const char *,const char *>> devices;

    // Estado interno encapsulado
    std::atomic<bool> repeat_active[DEV_NUM_KEYS + EXTRA_BUTTONS];
    std::chrono::steady_clock::time_point last_press[DEV_NUM_KEYS + EXTRA_BUTTONS];

public:
    NagaDaemon(int argc, char *argv[]) {
        size = sizeof(struct input_event);

        // Inicializar atomics
        for (int i = 0; i < (DEV_NUM_KEYS + EXTRA_BUTTONS); ++i) {
            repeat_active[i] = false;
        }

        std::string dev_kbd, dev_mouse;
        bool ok = false;

        for (int attempt = 0; attempt < 50; ++attempt) { // ~5s
            if (find_naga_pair(dev_kbd, dev_mouse)) { ok = true; break; }
            usleep(100000); // 100ms
        }

        if (!ok) {
            std::cerr << "Naga not found yet in /dev/input/by-id" << std::endl;
            exit(1);
        }

        if(find_naga_pair(dev_kbd, dev_mouse)) {
            side_btn_fd  = open(dev_kbd.c_str(), O_RDONLY);
            extra_btn_fd = open(dev_mouse.c_str(), O_RDONLY);

            if(side_btn_fd != -1 && extra_btn_fd != -1) {
                side_btn_fd_global = side_btn_fd;
                std::cout << "Reading from (dynamic): " << dev_kbd << " and " << dev_mouse << std::endl;
            }
        }

        // Si la detección dinámica falla, usar método legacy
        if(side_btn_fd == -1 || extra_btn_fd == -1) {
            for (auto &device : devices) {
                if ((side_btn_fd = open(device.first, O_RDONLY)) != -1 &&
                    (extra_btn_fd = open(device.second, O_RDONLY)) != -1) {
                    
                    side_btn_fd_global = side_btn_fd;
                    std::cout << "Reading from (legacy): " << device.first << " and " << device.second << std::endl;
                    break;
                }
            }
        }

        if(side_btn_fd == -1 || extra_btn_fd == -1) {
            std::cerr << "No naga devices found." << std::endl;
            exit(1);
        }

        // Initialize config
        this->loadConf("mapping_01.txt");
    }  

    void loadConf(string filename) {
        args.clear();
        options.clear();
        state.clear();
        args.resize(DEV_NUM_KEYS + EXTRA_BUTTONS);
        options.resize(DEV_NUM_KEYS + EXTRA_BUTTONS);
        state.resize(DEV_NUM_KEYS + EXTRA_BUTTONS);

        string conf_file = safe_home_dir() + "/.naga/" + filename;
        ifstream in(conf_file.c_str(), ios::in);
        if (!in) {
            cerr << "Cannot open " << conf_file << ". Exiting." << endl;
            exit(1);
        }

        string line, line1, token1;
        size_t eqpos, dashpos;

        while (getline(in, line)) {
            if (line.empty()) continue;
            if (line[0] == '#') continue;

            eqpos = line.find('=');
            if (eqpos == std::string::npos) continue;

            line1 = line.substr(0, eqpos);
            line.erase(0, eqpos + 1);

            line1.erase(std::remove(line1.begin(), line1.end(), ' '), line1.end());

            if (line1.empty()) continue;
            if (line1[0] == '#') continue;

            dashpos = line1.find('-');
            if (dashpos == std::string::npos) continue;

            token1 = line1.substr(0, dashpos);
            line1  = line1.substr(dashpos + 1);

            int idx;
            try {
                idx = stoi(token1) - 1;
            } catch (...) {
                continue;
            }
            if (idx < 0 || idx >= (DEV_NUM_KEYS + EXTRA_BUTTONS)) continue;

            if      (line1 == "chmap")       options[idx].push_back(Operators::chmap);
            else if (line1 == "key")         options[idx].push_back(Operators::key);
            else if (line1 == "run")         options[idx].push_back(Operators::run);
            else if (line1 == "run2")        options[idx].push_back(Operators::run2);
            else if (line1 == "click")       options[idx].push_back(Operators::click);
            else if (line1 == "workspace_r") options[idx].push_back(Operators::workspace_r);
            else if (line1 == "workspace")   options[idx].push_back(Operators::workspace);
            else if (line1 == "position") {
                options[idx].push_back(Operators::position);
                std::replace(line.begin(), line.end(), ',', ' ');
            }
            else if (line1 == "delay")       options[idx].push_back(Operators::delay);
            else if (line1 == "media")       options[idx].push_back(Operators::media);
            else if (line1 == "toggle")      options[idx].push_back(Operators::toggle);
            else {
                cerr << "Not supported key action, check syntax in " << conf_file << endl;
                exit(1);
            }

            // Guardar args/estado
            clog << "Line : " << line << endl;
            args[idx].push_back(line);
            state[idx].push_back(0);
        }

        in.close();
    }

    void run() {
        fd_set readset;

        // Intentar control exclusivo
        if (ioctl(side_btn_fd, EVIOCGRAB, 1) == -1) {
            perror("EVIOCGRAB failed (firmware events will pass through)");
        }
        
        while (1) {
            FD_ZERO(&readset);
            FD_SET(side_btn_fd, &readset);
            FD_SET(extra_btn_fd, &readset);
            
            if (select(FD_SETSIZE, &readset, NULL, NULL, NULL) == -1) exit(2);

            if (FD_ISSET(side_btn_fd, &readset)) { // Side buttons
                if (read(side_btn_fd, ev1, size * 64) == -1) exit(2);

                if (ev1[1].type == EV_KEY) { // Key event (press or release)
                    switch (ev1[1].code) {
                        case 2: case 3: case 4: case 5: case 6: case 7:
                        case 8: case 9: case 10: case 11: case 12: case 13:
                            chooseAction(ev1[1].code - 2, ev1[1].value);
                            break;
                    }
                }
            } else { // Extra buttons
                if (read(extra_btn_fd, ev2, size * 64) == -1) exit(2);

                if (ev2[1].type == 1 && ev2[1].value == 1) { // Only extra buttons
                    switch (ev2[1].code) {
                        case 275: case 276:
                            chooseAction(ev2[1].code - OFFSET, 1);
                            break;
                    }
                }
            }
        }
    }

    void chooseAction(int i, int eventCode /*1 for press, 0 for release*/) {
        using namespace std::chrono;

        if (eventCode == 1) {
            auto now = steady_clock::now();
            auto diff = duration_cast<milliseconds>(now - last_press[i]).count();

            if (diff < 120) return; // filtro antibounce

            last_press[i] = now;
        }
      
        std::cout << "Button index: " << i << " event: " << eventCode << std::endl;

        // Only accept press or release events
        if (eventCode > 1) return;

        const string keydownop = "ydotool key ";
        const string keyupop   = "ydotool key ";
        const string clickop   = "ydotool click ";
        const string workspace_r = "true ";
        const string workspace   = "true ";
        const string position = "ydotool mousemove ";

        string command;
        bool execution;

        for (unsigned int j = 0; j < options[i].size(); j++) {
            execution = true;
            
            switch (options[i][j]) {
                case Operators::key: {
                    if (eventCode == 1) {
                        repeat_active[i] = true;
                        command = "ydotool key " + args[i][j];
                        
                        // Ignoramos el warning del retorno de system()
                        if (system(command.c_str()) == -1) { /* error silenciado */ }

                        // Lanzar hilo de repetición
                        std::thread([this, i, j]() {
                            usleep(250000); // delay inicial 250ms
                            while (repeat_active[i]) {
                                std::string cmd = "ydotool key " + args[i][j];
                                if (system(cmd.c_str()) == -1) {}
                                usleep(150000); // velocidad repetición
                            }
                        }).detach();
                        
                        execution = false; // Evita que se vuelva a ejecutar system() abajo
                    }
                    else if (eventCode == 0) {
                        repeat_active[i] = false;
                        execution = false;
                    }
                    break;
                }
                case Operators::run:
                    command = "setsid " + args[i][j] + " &";
                    if (eventCode == 0) execution = false; 
                    break;
                case Operators::run2:
                    command = "setsid " + args[i][j] + " &"; 
                    break;
                case Operators::click:
                    command = clickop + args[i][j];
                    if (eventCode == 0) execution = false; 
                    break;
                case Operators::workspace_r:
                    command = workspace_r + args[i][j];
                    if (eventCode == 0) execution = false; 
                    break;
                case Operators::workspace:
                    command = workspace + args[i][j];
                    if (eventCode == 0) execution = false; 
                    break;
                case Operators::position:
                    command = position + args[i][j];
                    if (eventCode == 0) execution = false; 
                    break;
                case Operators::media: {
                    if (eventCode == 1) {
                        command = "ydotool key " + args[i][j];
                    } else {
                        execution = false;
                    }
                    break;
                }
                case Operators::toggle:
                    if (eventCode == 0) {
                        execution = false;
                    } else {
                        if (state[i][j] == 0) {
                            command = "ydotool key " + args[i][j] + ":1";
                            state[i][j] = 1;
                        }
                        else if (state[i][j] == 1) {
                            command = "ydotool key " + args[i][j] + ":0";
                            state[i][j] = 0;
                        }
                    }
                    break;
                default:
                    // Captura de otros operadores (chmap, delay, etc)
                    execution = false;
                    break;
            }

            if (execution && !command.empty()) {
                if (system(command.c_str()) == -1) { /* error silenciado */ }
            }
        }
    }
};

int main(int argc, char *argv[]) {
    // This option kills any other naga daemons and exits
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if (argc > 1) {
        if (strcmp(argv[1], "-kill") == 0) {
            if (system("killall naga") == -1) { /* error silenciado */ }
            exit(0);
        }
    }

    clog << "Starting naga daemon" << endl;
    NagaDaemon daemon(argc, argv);
    daemon.run();

    return 0;
}
