#define EFFICIENT_CORE_NEW_LIBRARY
#include "efficient_core/efficient_core_new.cpp"

#include <limits>

using namespace std;

struct GameNewOptions {
    bool interactive = false;
    bool show_stats = false;
    bool seed_provided = false;
    bool spins_provided = false;
    bool help = false;
    uint64_t spins = 0;
    unsigned int seed = hnp_new::DEFAULT_SEED;
    int threads = hnp_new::defaultThreadCount();
    string rtp_output = "outputs/rtp_report_new.md";
    string symbol_output = "outputs/symbol_win_distribution_new.csv";
};

void printGameNewUsage(ostream& out) {
    out << "Usage:\n"
        << "  ./game_new --interactive [--seed N] [--show-stats]\n"
        << "  ./game_new --spins N [--seed N] [--threads N|--single-thread]\n"
        << "\nOptions:\n"
        << "  --interactive          Spin one game at a time\n"
        << "  --spins N              Run a simulation/report\n"
        << "  --seed N               Fixed seed; random startup seed if omitted\n"
        << "  --threads N            Worker cap for simulation mode\n"
        << "  --single-thread        Use one worker\n"
        << "  --show-stats           Print running RTP in interactive mode\n"
        << "  --rtp-output PATH      Default outputs/rtp_report_new.md\n"
        << "  --symbol-output PATH   Default outputs/symbol_win_distribution_new.csv\n"
        << "  --help                 Show this message\n";
}

unsigned int randomStartupSeed() {
    random_device rd;
    auto time_seed = static_cast<unsigned int>(
        chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    return rd() ^ time_seed;
}

bool parseGameNewOptions(int argc, char** argv, GameNewOptions& options) {
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--interactive") {
            options.interactive = true;
        } else if (arg == "--show-stats") {
            options.show_stats = true;
        } else if (arg == "--spins" && i + 1 < argc) {
            options.spins_provided = hnp_new::parseUint64(argv[++i], options.spins);
            if (!options.spins_provided) return false;
        } else if (arg == "--seed" && i + 1 < argc) {
            uint64_t seed = 0;
            if (!hnp_new::parseUint64(argv[++i], seed)) return false;
            options.seed = static_cast<unsigned int>(seed);
            options.seed_provided = true;
        } else if (arg == "--threads" && i + 1 < argc) {
            uint64_t threads = 0;
            if (!hnp_new::parseUint64(argv[++i], threads) || threads == 0) return false;
            options.threads = static_cast<int>(min<uint64_t>(threads, numeric_limits<int>::max()));
        } else if (arg == "--single-thread") {
            options.threads = 1;
        } else if (arg == "--rtp-output" && i + 1 < argc) {
            options.rtp_output = argv[++i];
        } else if (arg == "--symbol-output" && i + 1 < argc) {
            options.symbol_output = argv[++i];
        } else if (arg == "--help") {
            options.help = true;
        } else {
            return false;
        }
    }

    if (!options.seed_provided) {
        options.seed = randomStartupSeed();
    }
    if (!options.interactive && !options.spins_provided) {
        options.interactive = true;
    }
    return true;
}

void printRunningStats(const hnp_new::Statistics& stats) {
    cout << fixed << setprecision(6)
         << "spins=" << stats.spins
         << ", wager=" << stats.total_bet
         << ", win=" << stats.total_win
         << ", RTP=" << (stats.total_bet > 0 ? static_cast<double>(stats.total_win) / stats.total_bet : 0.0)
         << ", hits=" << stats.hits
         << "\n";
}

void printSpin(const hnp_new::SpinResult& spin, uint64_t spin_index) {
    cout << "\nSPIN " << spin_index << "\n";
    cout << "Pay window:\n";
    hnp_new::printWindow(cout, spin.window);
    cout << "Active rows: " << spin.active_rows << "\n";
    cout << "Hard Hat count: " << spin.hard_hat_count << "\n";
    cout << "Base win: " << spin.base_win << "\n";
    cout << "Feature win: " << spin.feature_win << "\n";
    cout << "Jackpot win: " << spin.jackpot_win << "\n";
    cout << "Overflow win: " << spin.overflow_win << "\n";
    cout << "Normal FS trigger: " << (spin.triggered_normal_fs ? "yes" : "no") << "\n";
    cout << "Super Saw FS trigger: " << (spin.triggered_super_fs ? "yes" : "no") << "\n";
    cout << "Total payout: " << (spin.base_win + spin.feature_win) << "\n";
}

int runInteractive(const GameNewOptions& options) {
    hnp_new::Engine engine(options.seed);
    hnp_new::Statistics stats;
    uint64_t spin_index = 0;

    cout << "Interactive mode. Press Enter or type spin/s to spin; stats for RTP; q to quit.\n";
    cout << "Seed=" << options.seed << ", Bet=" << hnp_new::BET << "\n";

    string command;
    while (true) {
        cout << "\ncommand> ";
        if (!getline(cin, command)) break;
        if (command == "q" || command == "quit" || command == "exit") break;
        if (command == "stats") {
            printRunningStats(stats);
            continue;
        }
        if (!command.empty() && command != "s" && command != "spin") {
            cout << "Commands: Enter/s/spin, stats, q\n";
            continue;
        }

        hnp_new::SpinResult spin = engine.spinBaseGame(stats);
        hnp_new::updateStatistics(stats, spin);
        spin_index++;
        printSpin(spin, spin_index);
        if (options.show_stats) {
            printRunningStats(stats);
        }
    }

    cout << "\nFinal ";
    printRunningStats(stats);
    return 0;
}

int runReport(const GameNewOptions& options) {
    auto start = chrono::steady_clock::now();
    hnp_new::Statistics stats = hnp_new::runSimulation(options.spins, options.seed, options.threads);
    auto end = chrono::steady_clock::now();
    double elapsed = chrono::duration<double>(end - start).count();

    hnp_new::writeRtpSummary(cout, stats, options.seed, options.threads, elapsed);

    hnp_new::ensureParentDirectory(options.rtp_output);
    ofstream rtp_file(options.rtp_output);
    hnp_new::writeRtpSummary(rtp_file, stats, options.seed, options.threads, elapsed);

    hnp_new::ensureParentDirectory(options.symbol_output);
    ofstream symbol_file(options.symbol_output);
    hnp_new::writeSymbolDistribution(symbol_file, stats);

    cout << "\nReports written:\n"
         << "  " << options.rtp_output << "\n"
         << "  " << options.symbol_output << "\n";
    return 0;
}

int main(int argc, char** argv) {
    GameNewOptions options;
    if (!parseGameNewOptions(argc, argv, options)) {
        printGameNewUsage(cerr);
        return 1;
    }
    if (options.help) {
        printGameNewUsage(cout);
        return 0;
    }
    if (options.interactive) {
        return runInteractive(options);
    }
    return runReport(options);
}
