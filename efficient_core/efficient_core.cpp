#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

enum Symbol {
    HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4, WILD, HAT, EXPHAT, HORHAT, VERTHAT, BLANK
};

constexpr int NO_OF_SYMBOLS = 8;
constexpr int SYMBOL_COUNT = 9;
constexpr int MAX_MATCH = 6;
constexpr int NO_OF_REELS = 5;
constexpr int NO_OF_ROWS = 6;
constexpr int BET = 20;

constexpr unsigned int DEFAULT_SEED = 123456789;
constexpr uint64_t DEFAULT_SPINS = 25;
constexpr uint64_t DEV_BENCH_SPINS = 1000000;

constexpr array<Symbol, NO_OF_SYMBOLS> SYMBOL_ARRAY = {HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4};

constexpr int PAY_TABLE[SYMBOL_COUNT][MAX_MATCH] = {
    {0, 0, 0, 20, 100, 500},
    {0, 0, 0, 15, 80, 400},
    {0, 0, 0, 10, 60, 300},
    {0, 0, 0, 8, 40, 200},
    {0, 0, 0, 5, 25, 100},
    {0, 0, 0, 5, 20, 80},
    {0, 0, 0, 3, 15, 60},
    {0, 0, 0, 3, 10, 50},
    {0, 0, 0, 0, 0, 0}
};

constexpr array<Symbol, 18> BG1_REEL1 = {LV1, LV2, HV1, LV4, HAT, LV3, HV2, LV1, LV2, HV3, EXPHAT, LV4, LV1, HV1, HAT, LV3, HV4, LV2};
constexpr array<Symbol, 16> BG1_REEL2 = {HV4, LV1, HV1, LV2, LV3, HAT, HV2, LV4, WILD, LV1, HV3, LV2, LV4, HV1, LV3, HV2};
constexpr array<Symbol, 20> BG1_REEL3 = {LV4, HV4, LV1, HV1, HAT, LV2, HV3, WILD, LV3, HV2, LV1, LV4, HV1, LV2, HV4, LV3, HAT, LV1, HV2, LV4};
constexpr array<Symbol, 17> BG1_REEL4 = {LV2, HV2, LV4, HAT, LV3, HV1, WILD, LV1, HV3, LV2, LV4, HV2, LV3, HV4, LV1, HAT, LV2};
constexpr array<Symbol, 19> BG1_REEL5 = {HV4, LV2, HV3, LV1, HAT, LV4, HV1, LV3, WILD, LV2, HV2, LV1, LV4, HV3, LV2, HAT, HV1, LV3, HV4};

using Window = array<array<Symbol, NO_OF_REELS>, NO_OF_ROWS>;
using ActiveWindow = array<array<Symbol, NO_OF_REELS>, NO_OF_ROWS>;

struct SpinResult {
    int round_win = 0;
    int free_game = 0;
};

struct Aggregate {
    uint64_t spins = 0;
    long double win_xbet_sum = 0.0L;
    long double win_xbet_square_sum = 0.0L;
    uint64_t free_games = 0;
    array<array<uint64_t, MAX_MATCH>, SYMBOL_COUNT> hit_table{};
    array<array<int64_t, MAX_MATCH>, SYMBOL_COUNT> win_table{};

    void addSpin(const SpinResult& result) {
        double win_xbet = result.round_win * 1.0 / BET;
        spins++;
        win_xbet_sum += win_xbet;
        win_xbet_square_sum += win_xbet * win_xbet;
        free_games += result.free_game;
    }

    void merge(const Aggregate& other) {
        spins += other.spins;
        win_xbet_sum += other.win_xbet_sum;
        win_xbet_square_sum += other.win_xbet_square_sum;
        free_games += other.free_games;
        for (int symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
            for (int match = 0; match < MAX_MATCH; match++) {
                hit_table[symbol][match] += other.hit_table[symbol][match];
                if (other.win_table[symbol][match] != 0) {
                    win_table[symbol][match] = other.win_table[symbol][match];
                }
            }
        }
    }
};

struct Options {
    uint64_t spins = DEFAULT_SPINS;
    unsigned int seed = DEFAULT_SEED;
    int threads = 1;
    bool feature_tests = false;
    bool benchmark = false;
    string output_path = "outputs/efficient_summary.txt";
};

const char* symbolToString(Symbol symbol) {
    switch (symbol) {
        case HV1: return "HV1";
        case HV2: return "HV2";
        case HV3: return "HV3";
        case HV4: return "HV4";
        case LV1: return "LV1";
        case LV2: return "LV2";
        case LV3: return "LV3";
        case LV4: return "LV4";
        case WILD: return "WILD";
        case HAT: return "HAT";
        case EXPHAT: return "XHAT";
        case HORHAT: return "HHAT";
        case VERTHAT: return "VHAT";
        case BLANK: return "BLANK";
        default: return "UNK";
    }
}

int reelSize(int reel) {
    static constexpr int sizes[NO_OF_REELS] = {
        static_cast<int>(BG1_REEL1.size()),
        static_cast<int>(BG1_REEL2.size()),
        static_cast<int>(BG1_REEL3.size()),
        static_cast<int>(BG1_REEL4.size()),
        static_cast<int>(BG1_REEL5.size())
    };
    return sizes[reel];
}

Symbol reelSymbol(int reel, int index) {
    switch (reel) {
        case 0: return BG1_REEL1[index];
        case 1: return BG1_REEL2[index];
        case 2: return BG1_REEL3[index];
        case 3: return BG1_REEL4[index];
        case 4: return BG1_REEL5[index];
        default: return BLANK;
    }
}

int getRandom(mt19937& rng, int a, int b) {
    uniform_int_distribution<int> dist(a, b);
    return dist(rng);
}

double getUniform(mt19937& rng) {
    uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

int cumProbability(double p) {
    static constexpr double cumulative[3] = {0.75, 0.95, 1.0};
    for (int i = 0; i < 3; i++) {
        if (p < cumulative[i]) {
            return i;
        }
    }
    return 2;
}

bool isHat(Symbol symbol) {
    return symbol == HAT || symbol == EXPHAT || symbol == HORHAT || symbol == VERTHAT;
}

void generatePayWindow(Window& window, mt19937& rng) {
    for (int reel = 0; reel < NO_OF_REELS; reel++) {
        int size = reelSize(reel);
        int start_idx = getRandom(rng, 0, size - 1);
        for (int row = 0; row < NO_OF_ROWS; row++) {
            int idx = (start_idx + row) % size;
            window[row][reel] = reelSymbol(reel, idx);
        }
    }
}

int countHatWindow(const Window& window, int start_row) {
    int count = 0;
    for (int row = start_row; row < NO_OF_ROWS; row++) {
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            count += isHat(window[row][reel]) ? 1 : 0;
        }
    }
    return count;
}

int countHatActive(const ActiveWindow& window, int rows) {
    int count = 0;
    for (int row = 0; row < rows; row++) {
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            count += isHat(window[row][reel]) ? 1 : 0;
        }
    }
    return count;
}

bool hasExplosiveHat(const Window& window) {
    for (int row = 0; row < NO_OF_ROWS; row++) {
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            if (window[row][reel] == EXPHAT) {
                return true;
            }
        }
    }
    return false;
}

int explosiveHat(const Window& window, int active_rows, mt19937& rng) {
    if (hasExplosiveHat(window)) {
        double p = getUniform(rng);
        return active_rows - (cumProbability(p) + 1);
    }
    return active_rows;
}

void copyActiveWindow(const Window& source, int active_rows, ActiveWindow& target, int& target_rows) {
    target_rows = NO_OF_ROWS - active_rows;
    for (int row = active_rows; row < NO_OF_ROWS; row++) {
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            target[row - active_rows][reel] = source[row][reel];
        }
    }
}

int waysWinCalculation(const Window& window, int start_row, Aggregate* aggregate) {
    int symbol_count[NO_OF_SYMBOLS] = {};
    for (int row = start_row; row < NO_OF_ROWS; row++) {
        Symbol symbol = window[row][0];
        if (symbol >= HV1 && symbol <= LV4) {
            symbol_count[symbol]++;
        }
    }

    int win = 0;
    for (Symbol symbol : SYMBOL_ARRAY) {
        if (symbol_count[symbol] <= 0) {
            continue;
        }

        int left2right = 0;
        int ways = 1;
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            int occ_in_reel = 0;
            for (int row = start_row; row < NO_OF_ROWS; row++) {
                Symbol current = window[row][reel];
                if (current == symbol || current == WILD) {
                    occ_in_reel++;
                }
            }
            if (occ_in_reel > 0) {
                ways *= occ_in_reel;
                left2right++;
            } else {
                break;
            }
        }

        int symbol_win = PAY_TABLE[symbol][left2right] * ways;
        if (aggregate != nullptr) {
            aggregate->hit_table[symbol][left2right]++;
            aggregate->win_table[symbol][left2right] = symbol_win;
        }
        win += symbol_win;
    }
    return win;
}

void girderTrigger(ActiveWindow& window, int rows, mt19937& rng) {
    int hat_counts = countHatActive(window, rows);
    int required_hats = max(0, 6 - hat_counts);
    array<int, NO_OF_ROWS * NO_OF_REELS> allowed_positions{};
    int allowed_count = 0;

    for (int row = 0; row < rows; row++) {
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            if (!isHat(window[row][reel])) {
                allowed_positions[allowed_count++] = row * NO_OF_REELS + reel;
            }
        }
    }

    shuffle(allowed_positions.begin(), allowed_positions.begin() + allowed_count, rng);
    int selected = min(required_hats, allowed_count);
    for (int i = 0; i < selected; i++) {
        int pos = allowed_positions[i];
        window[pos / NO_OF_REELS][pos % NO_OF_REELS] = HAT;
    }
}

SpinResult simulateOneSpin(mt19937& rng, Aggregate* aggregate) {
    Window window{};
    ActiveWindow active_window{};
    int active_window_rows = 0;

    int active_rows = 3;
    generatePayWindow(window, rng);
    active_rows = explosiveHat(window, active_rows, rng);
    int round_win = waysWinCalculation(window, active_rows, aggregate);

    copyActiveWindow(window, active_rows, active_window, active_window_rows);
    int hat_counts = countHatActive(active_window, active_window_rows);
    if (hat_counts > 0) {
        double p = getUniform(rng);
        if (p < 0.1) {
            girderTrigger(active_window, active_window_rows, rng);
        }
    }

    hat_counts = countHatActive(active_window, active_window_rows);
    return {round_win, hat_counts >= 6 ? 1 : 0};
}

Aggregate simulateRangeWithRng(uint64_t spins, mt19937& rng) {
    Aggregate aggregate;
    for (uint64_t i = 0; i < spins; i++) {
        SpinResult result = simulateOneSpin(rng, &aggregate);
        aggregate.addSpin(result);
    }
    return aggregate;
}

Aggregate simulateRange(uint64_t spins, unsigned int seed) {
    mt19937 rng(seed);
    return simulateRangeWithRng(spins, rng);
}

unsigned int deriveThreadSeed(unsigned int seed, int thread_index) {
    seed_seq seq{seed, static_cast<unsigned int>(thread_index), 0x9e3779b9U};
    array<unsigned int, 1> generated{};
    seq.generate(generated.begin(), generated.end());
    return generated[0];
}

Aggregate simulateThreaded(uint64_t spins, unsigned int seed, int threads) {
    if (threads <= 1) {
        return simulateRange(spins, seed);
    }

    vector<thread> workers;
    vector<Aggregate> local_results(threads);
    workers.reserve(threads);

    uint64_t base = spins / threads;
    uint64_t remainder = spins % threads;
    for (int t = 0; t < threads; t++) {
        uint64_t worker_spins = base + (static_cast<uint64_t>(t) < remainder ? 1 : 0);
        unsigned int worker_seed = deriveThreadSeed(seed, t);
        workers.emplace_back([worker_spins, worker_seed, &local_results, t]() {
            local_results[t] = simulateRange(worker_spins, worker_seed);
        });
    }

    for (thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    Aggregate aggregate;
    for (const Aggregate& local : local_results) {
        aggregate.merge(local);
    }
    return aggregate;
}

void printWindow(ostream& out, const Window& window, const string& title) {
    out << "\n===== " << title << " =====\n";
    for (int row = 0; row < NO_OF_ROWS; row++) {
        out << "row " << row << " | ";
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            out << left << setw(7) << symbolToString(window[row][reel]);
        }
        out << "\n";
    }
}

void printActiveWindow(ostream& out, const ActiveWindow& window, int rows, const string& title) {
    out << "\n===== " << title << " =====\n";
    for (int row = 0; row < rows; row++) {
        out << "row " << row << " | ";
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            out << left << setw(7) << symbolToString(window[row][reel]);
        }
        out << "\n";
    }
}

void runFeatureTests(ostream& out, mt19937& rng) {
    Aggregate scratch;

    out << "\nFEATURE-SPECIFIC FORCED TEST CASES\n";

    {
        out << "\nFEATURE TEST: FORCED GIRDER VIA INJECTED HAT\n";
        Window window{};
        ActiveWindow active{};
        int active_rows_count = 0;
        generatePayWindow(window, rng);
        printWindow(out, window, "GIRDER TEST ORIGINAL FULL PAY WINDOW");
        int round_win = waysWinCalculation(window, 3, &scratch);
        copyActiveWindow(window, 3, active, active_rows_count);
        printActiveWindow(out, active, active_rows_count, "GIRDER TEST ORIGINAL ACTIVE PAY WINDOW");
        Symbol old_a = active[0][0];
        Symbol old_b = active[1][2];
        active[0][0] = HAT;
        active[1][2] = HAT;
        out << "Injected HAT at row=0, reel=0 replacing " << symbolToString(old_a) << "\n";
        out << "Injected HAT at row=1, reel=2 replacing " << symbolToString(old_b) << "\n";
        printActiveWindow(out, active, active_rows_count, "GIRDER TEST MODIFIED ACTIVE PAY WINDOW");
        int hats_before = countHatActive(active, active_rows_count);
        girderTrigger(active, active_rows_count, rng);
        int hats_after = countHatActive(active, active_rows_count);
        out << "GIRDER TEST state change: hats_before=" << hats_before
            << ", hats_after=" << hats_after
            << ", free_game=" << (hats_after >= 6 ? 1 : 0) << "\n";
        out << "GIRDER TEST payout after feature: round_win remains "
            << round_win
            << " because core ways payout is calculated before girder logic\n";
    }

    {
        out << "\nFEATURE TEST: FORCED EXPHAT/HAT ACTIVE ROW EXPANSION\n";
        Window window{};
        generatePayWindow(window, rng);
        printWindow(out, window, "EXPHAT TEST ORIGINAL FULL PAY WINDOW");
        Symbol old = window[4][0];
        window[4][0] = EXPHAT;
        out << "Injected XHAT at row=4, reel=0 replacing " << symbolToString(old) << "\n";
        printWindow(out, window, "EXPHAT TEST MODIFIED FULL PAY WINDOW");
        int forced_active_rows = 3 - (cumProbability(0.10) + 1);
        int before_win = waysWinCalculation(window, 3, &scratch);
        int after_win = waysWinCalculation(window, forced_active_rows, &scratch);
        out << "EXPHAT TEST payout/state change: round_win_before=" << before_win
            << ", round_win_after=" << after_win
            << ", hats_before=" << countHatWindow(window, 3)
            << ", hats_after=" << countHatWindow(window, forced_active_rows)
            << ", free_game=" << (countHatWindow(window, forced_active_rows) >= 6 ? 1 : 0) << "\n";
    }

    {
        out << "\nFEATURE TEST: INJECTED HAT ON FIRST ACTIVE REEL\n";
        Window window{};
        generatePayWindow(window, rng);
        printWindow(out, window, "HAT TEST ORIGINAL FULL PAY WINDOW");
        Symbol old = window[3][0];
        window[3][0] = HAT;
        out << "Injected HAT at row=3, reel=0 replacing " << symbolToString(old) << "\n";
        printWindow(out, window, "HAT TEST MODIFIED FULL PAY WINDOW");
        int round_win = waysWinCalculation(window, 3, &scratch);
        out << "HAT TEST state: round_win=" << round_win
            << ", hats_active=" << countHatWindow(window, 3)
            << ", note=HAT is counted for bonus state but not seeded as a paying ways symbol\n";
    }
}

int defaultThreadCount() {
    unsigned int hw = thread::hardware_concurrency();
    if (hw == 0) {
        return 1;
    }
    int conservative = static_cast<int>(hw / 10);
    return max(1, conservative);
}

bool parseUint64(const string& text, uint64_t& value) {
    char* end = nullptr;
    unsigned long long parsed = strtoull(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    value = static_cast<uint64_t>(parsed);
    return true;
}

Options parseOptions(int argc, char** argv) {
    Options options;
    options.threads = defaultThreadCount();

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--spins" && i + 1 < argc) {
            parseUint64(argv[++i], options.spins);
        } else if (arg == "--seed" && i + 1 < argc) {
            uint64_t seed = 0;
            if (parseUint64(argv[++i], seed)) {
                options.seed = static_cast<unsigned int>(seed);
            }
        } else if (arg == "--threads" && i + 1 < argc) {
            options.threads = max(1, atoi(argv[++i]));
        } else if (arg == "--output" && i + 1 < argc) {
            options.output_path = argv[++i];
        } else if (arg == "--feature-tests") {
            options.feature_tests = true;
        } else if (arg == "--benchmark") {
            options.benchmark = true;
            options.spins = DEV_BENCH_SPINS;
        } else if (arg == "--single-thread") {
            options.threads = 1;
        }
    }

    return options;
}

void writeSummary(ostream& out, const Options& options, const Aggregate& aggregate, double elapsed_seconds) {
    long double mean = aggregate.spins > 0 ? aggregate.win_xbet_sum / aggregate.spins : 0.0L;
    long double variance = aggregate.spins > 0 ? aggregate.win_xbet_square_sum / aggregate.spins - mean * mean : 0.0L;
    double free_game_trigger = aggregate.free_games > 0
        ? static_cast<double>(aggregate.spins) / static_cast<double>(aggregate.free_games)
        : 0.0;

    out << fixed << setprecision(6);
    out << "efficient_core summary\n";
    out << "Spins=" << aggregate.spins << "\n";
    out << "Seed=" << options.seed << "\n";
    out << "Threads=" << options.threads << "\n";
    out << "Free Games:" << free_game_trigger << "\n";
    out << "RTP:" << static_cast<double>(mean) << "\n";
    out << "Var:" << static_cast<double>(variance) << "\n";
    out << "ElapsedSeconds:" << elapsed_seconds << "\n";
    out << "SpinsPerSecond:" << (elapsed_seconds > 0.0 ? aggregate.spins / elapsed_seconds : 0.0) << "\n";
}

int main(int argc, char** argv) {
    Options options = parseOptions(argc, argv);

    auto started = chrono::steady_clock::now();
    Aggregate aggregate = simulateThreaded(options.spins, options.seed, options.threads);
    auto finished = chrono::steady_clock::now();
    double elapsed = chrono::duration<double>(finished - started).count();

    ofstream output(options.output_path, ios::out | ios::trunc);
    if (!output.is_open()) {
        cerr << "Failed to open " << options.output_path << "\n";
        return 1;
    }

    writeSummary(output, options, aggregate, elapsed);
    if (options.feature_tests) {
        mt19937 feature_rng(options.seed);
        simulateRangeWithRng(options.spins, feature_rng);
        runFeatureTests(output, feature_rng);
    }

    writeSummary(cout, options, aggregate, elapsed);
    return 0;
}
