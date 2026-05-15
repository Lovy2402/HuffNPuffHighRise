#define EFFICIENT_CORE_LIBRARY
#include "efficient_core/efficient_core.cpp"

#include <limits>

struct GameOptions {
    bool interactive = false;
    bool show_stats = false;
    bool help = false;
    bool spins_provided = false;
    bool seed_provided = false;
    uint64_t spins = 0;
    unsigned int seed = 0;
    int threads = defaultThreadCount();
    string rtp_output_path = "outputs/rtp_report.txt";
    string symbol_output_path = "outputs/symbol_distribution.csv";
};

void printGameUsage(ostream& out) {
    out << "Usage:\n"
        << "  ./game --interactive [--seed N] [--show-stats]\n"
        << "  ./game --spins N [--seed N] [--threads N|--single-thread]\n"
        << "\nOptions:\n"
        << "  --interactive             Spin one game at a time from the terminal\n"
        << "  --spins N                 Run N simulation spins and write reports\n"
        << "  --threads N               Limit simulation workers to N threads\n"
        << "  --single-thread           Force one deterministic RNG stream\n"
        << "  --seed N                  Set RNG seed; default is random at startup\n"
        << "  --show-stats              Print running RTP in interactive mode\n"
        << "  --rtp-output PATH         RTP report path, default outputs/rtp_report.txt\n"
        << "  --symbol-output PATH      Symbol CSV path, default outputs/symbol_distribution.csv\n"
        << "  --help                    Show this message\n";
}

bool parseGameOptions(int argc, char** argv, GameOptions& options) {
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--interactive") {
            options.interactive = true;
        } else if (arg == "--show-stats") {
            options.show_stats = true;
        } else if (arg == "--spins" && i + 1 < argc) {
            options.spins_provided = parseUint64(argv[++i], options.spins);
            if (!options.spins_provided) {
                cerr << "Invalid --spins value\n";
                return false;
            }
        } else if (arg == "--seed" && i + 1 < argc) {
            uint64_t seed = 0;
            if (!parseUint64(argv[++i], seed)) {
                cerr << "Invalid --seed value\n";
                return false;
            }
            options.seed = static_cast<unsigned int>(seed);
            options.seed_provided = true;
        } else if (arg == "--threads" && i + 1 < argc) {
            uint64_t threads = 0;
            if (!parseUint64(argv[++i], threads) || threads == 0) {
                cerr << "Invalid --threads value\n";
                return false;
            }
            options.threads = static_cast<int>(min<uint64_t>(threads, numeric_limits<int>::max()));
        } else if (arg == "--single-thread") {
            options.threads = 1;
        } else if (arg == "--rtp-output" && i + 1 < argc) {
            options.rtp_output_path = argv[++i];
        } else if (arg == "--symbol-output" && i + 1 < argc) {
            options.symbol_output_path = argv[++i];
        } else if (arg == "--help") {
            options.help = true;
        } else {
            cerr << "Unknown or incomplete option: " << arg << "\n";
            return false;
        }
    }

    if (!options.interactive && !options.spins_provided) {
        options.interactive = true;
    }
    return true;
}

unsigned int randomStartupSeed() {
    random_device rd;
    auto time_seed = static_cast<unsigned int>(
        chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    return rd() ^ time_seed;
}

long double totalPayoutCredits(const Aggregate& aggregate) {
    return aggregate.win_xbet_sum * BET;
}

long double totalWagerCredits(const Aggregate& aggregate) {
    return static_cast<long double>(aggregate.spins) * BET;
}

long double aggregateRtp(const Aggregate& aggregate) {
    return aggregate.spins > 0 ? aggregate.win_xbet_sum / aggregate.spins : 0.0L;
}

long double aggregateVariance(const Aggregate& aggregate) {
    long double mean = aggregateRtp(aggregate);
    return aggregate.spins > 0 ? aggregate.win_xbet_square_sum / aggregate.spins - mean * mean : 0.0L;
}

void printSpinDetails(ostream& out, uint64_t spin_index, const DetailedSpinResult& spin) {
    out << "\nSPIN " << spin_index << "\n";
    printWindow(out, spin.pay_window, "PAY WINDOW");

    out << "\nFeatures:\n";
    out << "  EXPHAT present in active window: " << (spin.explosive_hat_present ? "yes" : "no") << "\n";
    if (spin.explosive_hat_present) {
        out << fixed << setprecision(6)
            << "  EXPHAT roll: " << spin.explosive_hat_roll
            << ", active start row: " << spin.initial_active_start_row
            << " -> " << spin.active_start_row << "\n";
    }

    printActiveWindow(out, spin.active_window_before_girder, spin.active_window_rows, "ACTIVE WINDOW BEFORE GIRDER");

    out << "\nWins:\n";
    if (spin.wins.empty()) {
        out << "  none\n";
    } else {
        for (const SymbolWinDetail& win : spin.wins) {
            out << "  " << symbolToString(win.symbol)
                << " " << win.left2right << "x"
                << " ways=" << win.ways
                << " pay=" << win.pay
                << " symbol_win=" << win.symbol_win << "\n";
        }
    }

    out << "\nGIRDER:\n";
    out << "  eligible: " << (spin.girder_eligible ? "yes" : "no") << "\n";
    if (spin.girder_eligible) {
        out << fixed << setprecision(6)
            << "  roll: " << spin.girder_roll
            << ", triggered: " << (spin.girder_triggered ? "yes" : "no") << "\n";
    }
    out << "  hats before: " << spin.hats_before_girder
        << ", hats after: " << spin.hats_after_girder << "\n";
    if (spin.girder_triggered) {
        printActiveWindow(out, spin.active_window_after_girder, spin.active_window_rows, "ACTIVE WINDOW AFTER GIRDER");
    }

    out << "\nResult:\n";
    out << "  wager=" << BET
        << ", payout=" << spin.result.round_win
        << ", free_game_trigger=" << spin.result.free_game << "\n";
}

void printRunningStats(ostream& out, const Aggregate& aggregate) {
    out << fixed << setprecision(6)
        << "Running stats: spins=" << aggregate.spins
        << ", total_wager=" << static_cast<double>(totalWagerCredits(aggregate))
        << ", total_payout=" << static_cast<double>(totalPayoutCredits(aggregate))
        << ", RTP=" << static_cast<double>(aggregateRtp(aggregate))
        << ", hit_frequency="
        << (aggregate.spins > 0 ? static_cast<double>(aggregate.paid_hits) / aggregate.spins : 0.0)
        << "\n";
}

int runInteractive(const GameOptions& options) {
    mt19937 rng(options.seed);
    Aggregate aggregate;
    uint64_t spin_index = 0;

    cout << "Interactive mode. Press Enter or type 's' to spin, 'stats' for RTP, 'q' to quit.\n";
    cout << "Seed=" << options.seed << ", Bet=" << BET << "\n";

    string command;
    while (true) {
        cout << "\ncommand> ";
        if (!getline(cin, command)) {
            break;
        }

        if (command == "q" || command == "quit" || command == "exit") {
            break;
        }
        if (command == "stats") {
            printRunningStats(cout, aggregate);
            continue;
        }
        if (!command.empty() && command != "s" && command != "spin") {
            cout << "Commands: Enter/s/spin, stats, q\n";
            continue;
        }

        DetailedSpinResult spin = simulateOneSpinDetailed(rng, &aggregate);
        aggregate.addSpin(spin.result);
        spin_index++;
        printSpinDetails(cout, spin_index, spin);
        if (options.show_stats) {
            printRunningStats(cout, aggregate);
        }
    }

    cout << "\nFinal ";
    printRunningStats(cout, aggregate);
    return 0;
}

void writeRtpReport(
    ostream& out,
    const GameOptions& options,
    const Aggregate& aggregate,
    double elapsed_seconds
) {
    long double wager = totalWagerCredits(aggregate);
    long double payout = totalPayoutCredits(aggregate);
    double free_game_trigger = aggregate.free_games > 0
        ? static_cast<double>(aggregate.spins) / static_cast<double>(aggregate.free_games)
        : 0.0;

    out << fixed << setprecision(6);
    out << "game simulation report\n";
    out << "Spins=" << aggregate.spins << "\n";
    out << "Seed=" << options.seed << "\n";
    out << "Threads=" << options.threads << "\n";
    out << "Bet=" << BET << "\n";
    out << "TotalWager=" << static_cast<double>(wager) << "\n";
    out << "TotalPayout=" << static_cast<double>(payout) << "\n";
    out << "Net=" << static_cast<double>(payout - wager) << "\n";
    out << "RTP=" << static_cast<double>(aggregateRtp(aggregate)) << "\n";
    out << "Variance=" << static_cast<double>(aggregateVariance(aggregate)) << "\n";
    out << "HitFrequency="
        << (aggregate.spins > 0 ? static_cast<double>(aggregate.paid_hits) / aggregate.spins : 0.0)
        << "\n";
    out << "PaidHits=" << aggregate.paid_hits << "\n";
    out << "FreeGames=" << aggregate.free_games << "\n";
    out << "FreeGameTrigger=" << free_game_trigger << "\n";
    out << "ElapsedSeconds=" << elapsed_seconds << "\n";
    out << "SpinsPerSecond=" << (elapsed_seconds > 0.0 ? aggregate.spins / elapsed_seconds : 0.0) << "\n";
}

void writeSymbolDistributionCsv(ostream& out, const Aggregate& aggregate) {
    long double wager = totalWagerCredits(aggregate);
    out << fixed << setprecision(9);
    out << "Symbol,Occurrence,Win,Mode,Hits,RTP\n";
    for (Symbol symbol : SYMBOL_ARRAY) {
        for (int match = 5; match >= 3; match--) {
            uint64_t hits = aggregate.hit_table[symbol][match];
            int64_t win_total = aggregate.win_table[symbol][match];
            long double hit_rate = aggregate.spins > 0
                ? static_cast<long double>(hits) / aggregate.spins
                : 0.0L;
            long double rtp = wager > 0.0L ? static_cast<long double>(win_total) / wager : 0.0L;

            out << symbolToString(symbol) << ","
                << match << "x,"
                << PAY_TABLE[symbol][match] << ","
                << "BaseGame,"
                << static_cast<double>(hit_rate) << ","
                << static_cast<double>(rtp) << "\n";
        }
        out << "\n";
    }

    out << "Metric,Value\n";
    out << setprecision(6);
    out << "BaseGameRTP," << static_cast<double>(aggregateRtp(aggregate) * 100.0L) << "\n";
    out << "TotalRTP," << static_cast<double>(aggregateRtp(aggregate) * 100.0L) << "\n";
    out << "TotalWager," << static_cast<double>(totalWagerCredits(aggregate)) << "\n";
    out << "TotalPayout," << static_cast<double>(totalPayoutCredits(aggregate)) << "\n";
    out << "PaidHits," << aggregate.paid_hits << "\n";
    out << "FreeGames," << aggregate.free_games << "\n";
    out << "Spins," << aggregate.spins << "\n";
}

int runSimulation(const GameOptions& options) {
    auto started = chrono::steady_clock::now();
    Aggregate aggregate = simulateThreaded(options.spins, options.seed, options.threads);
    auto finished = chrono::steady_clock::now();
    double elapsed = chrono::duration<double>(finished - started).count();

    ofstream rtp_output(options.rtp_output_path, ios::out | ios::trunc);
    if (!rtp_output.is_open()) {
        cerr << "Failed to open " << options.rtp_output_path << "\n";
        return 1;
    }
    writeRtpReport(rtp_output, options, aggregate, elapsed);

    ofstream symbol_output(options.symbol_output_path, ios::out | ios::trunc);
    if (!symbol_output.is_open()) {
        cerr << "Failed to open " << options.symbol_output_path << "\n";
        return 1;
    }
    writeSymbolDistributionCsv(symbol_output, aggregate);

    writeRtpReport(cout, options, aggregate, elapsed);
    cout << "Wrote " << options.rtp_output_path << "\n";
    cout << "Wrote " << options.symbol_output_path << "\n";
    return 0;
}

int main(int argc, char** argv) {
    GameOptions options;
    if (!parseGameOptions(argc, argv, options)) {
        printGameUsage(cerr);
        return 1;
    }
    if (!options.seed_provided) {
        options.seed = randomStartupSeed();
    }
    if (options.help) {
        printGameUsage(cout);
        return 0;
    }
    if (options.interactive) {
        return runInteractive(options);
    }
    return runSimulation(options);
}
